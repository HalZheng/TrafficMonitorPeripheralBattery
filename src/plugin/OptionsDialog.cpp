#include "OptionsDialog.h"

#include "BatteryItem.h"

#include <commdlg.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

namespace pb
{
    namespace
    {
        const UINT_PTR kRescanTimer = 1;

        enum : int
        {
            IDC_REFRESH_LABEL = 1001,
            IDC_REFRESH_EDIT,
            IDC_MAX_LABEL,
            IDC_MAX_EDIT,
            IDC_MODE_LABEL,
            IDC_MODE_COMBO,
            IDC_DEVICE_GROUP,
            IDC_DEVICE_LIST,
            IDC_VISIBLE_CHECK,
            IDC_NAME_LABEL,
            IDC_NAME_EDIT,
            IDC_LABEL_LABEL,
            IDC_LABEL_EDIT,
            IDC_PRIORITY_LABEL,
            IDC_PRIORITY_EDIT,
            IDC_COLOR_LABEL,
            IDC_COLOR_BTN,
            IDC_AUTO_COLOR,
            IDC_MOVE_UP,
            IDC_MOVE_DOWN,
            IDC_RESCAN,
            IDC_STATUS,
            IDC_ADVANCED
        };

        // Controls of the secondary "advanced settings" dialog. A separate
        // range keeps them from colliding with the main dialog's IDs.
        enum : int
        {
            ADV_KEEP_LAST = 1101,
            ADV_LOW_NOTIFY,
            ADV_STACKED_PERCENT,
            ADV_FRAMED,
            ADV_SHOW_OFFLINE,
            ADV_LOW_PERCENT,
            ADV_GRACE,
            ADV_HYBRID_ROWS,
            ADV_ROW_BAR_WIDTH,
            ADV_BATTERY_LOW,
            ADV_BATTERY_GOOD,
            ADV_CPU_ORANGE,
            ADV_CPU_RED
        };

#pragma pack(push, 1)
        struct DialogTemplate
        {
            DLGTEMPLATE dlg;
            WORD menu;
            WORD windowClass;
            WCHAR title[64];
        };
#pragma pack(pop)

        struct DialogState
        {
            Settings working;
            Settings* target = nullptr;
            const DeviceRegistry* registry = nullptr;
            std::function<std::vector<std::wstring>()> presentProvider;
            std::function<void()> onRescan;
            std::vector<std::wstring> presentIds;
            HWND hDlg = nullptr;
            HWND refreshEdit = nullptr;
            HWND maxEdit = nullptr;
            HWND modeCombo = nullptr;
            HWND deviceList = nullptr;
            HWND visibleCheck = nullptr;
            HWND nameEdit = nullptr;
            HWND labelEdit = nullptr;
            HWND priorityEdit = nullptr;
            HWND colorBtn = nullptr;
            HWND autoColorCheck = nullptr;
            HWND statusText = nullptr;
            std::vector<std::wstring> deviceIds;
            int current = -1;
            int rescanTicks = 0;
            bool rescanPending = false;
            // Guard against LBN_SELCHANGE re-entry while we rebuild list rows.
            bool suppressSelChange = false;
            COLORREF customColors[16] = {};
        };

        /**
         * State for the secondary dialog. It edits only global [plugin] keys,
         * so it needs no list box selection guard and cannot disturb the main
         * dialog's device list.
         */
        struct AdvancedState
        {
            Settings* working = nullptr;
            HWND hDlg = nullptr;
            HWND keepLast = nullptr;
            HWND lowNotify = nullptr;
            HWND stackedPercent = nullptr;
            HWND framed = nullptr;
            HWND showOffline = nullptr;
            HWND lowPercent = nullptr;
            HWND grace = nullptr;
            HWND hybridRows = nullptr;
            HWND rowBarWidth = nullptr;
            HWND batteryLow = nullptr;
            HWND batteryGood = nullptr;
            HWND cpuOrange = nullptr;
            HWND cpuRed = nullptr;
        };

        DialogState* GetState(HWND hDlg)
        {
            return reinterpret_cast<DialogState*>(
                GetWindowLongPtrW(hDlg, DWLP_USER));
        }

        std::wstring ToWString(int value)
        {
            return std::to_wstring(value);
        }

        std::wstring GetText(HWND control)
        {
            wchar_t buffer[256] = {};
            GetWindowTextW(control, buffer,
                static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
            return buffer;
        }

        void SetText(HWND control, const std::wstring& text)
        {
            SetWindowTextW(control, text.c_str());
        }

        std::wstring ColorToHex(COLORREF color)
        {
            wchar_t buffer[16] = {};
            swprintf_s(buffer, L"#%02X%02X%02X",
                GetRValue(color), GetGValue(color), GetBValue(color));
            return buffer;
        }

        DeviceSettings& GetDeviceSettings(DialogState& state,
            const std::wstring& id)
        {
            auto iter = state.working.devices.find(id);
            if (iter != state.working.devices.end())
                return iter->second;

            DeviceSettings fallback;
            if (state.registry != nullptr)
            {
                if (const DeviceDescriptor* device = state.registry->FindById(id))
                {
                    fallback.hidden = !device->enabledByDefault;
                    fallback.name = device->name;
                    fallback.label = device->defaultLabel;
                    fallback.priority = device->priority;
                }
            }
            fallback.color = L"auto";
            state.working.devices[id] = fallback;
            return state.working.devices[id];
        }

        bool IsPresent(const DialogState& state, const std::wstring& id)
        {
            return std::find(state.presentIds.begin(), state.presentIds.end(), id)
                != state.presentIds.end();
        }

        std::wstring DeviceCaption(DialogState& state, const std::wstring& id)
        {
            const DeviceSettings& settings = GetDeviceSettings(state, id);
            std::wstring text;
            if (settings.hidden)
                text = L"[隐藏] ";
            else if (IsPresent(state, id))
                text = L"[在线] ";
            else
                text = L"[离线] ";

            const DeviceDescriptor* device = state.registry != nullptr
                ? state.registry->FindById(id) : nullptr;
            const std::wstring& fallbackName = device != nullptr
                ? device->name : id;
            text += settings.name.empty() ? fallbackName : settings.name;
            return text;
        }

        HWND CreateControl(DialogState& state, const wchar_t* className,
            const wchar_t* text, DWORD style,
            int x, int y, int width, int height, int id)
        {
            HWND control = CreateWindowExW(0, className, text,
                WS_CHILD | WS_VISIBLE | style,
                x, y, width, height, state.hDlg,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                GetModuleHandleW(nullptr), nullptr);

            HFONT font = reinterpret_cast<HFONT>(
                SendMessageW(state.hDlg, WM_GETFONT, 0, 0));
            if (font == nullptr)
                font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return control;
        }

        void LayoutControls(DialogState& state)
        {
            RECT client = {};
            GetClientRect(state.hDlg, &client);
            int width = client.right;
            int height = client.bottom;

            CreateControl(state, L"STATIC", L"刷新间隔（秒）", SS_LEFT,
                12, 18, 96, 20, IDC_REFRESH_LABEL);
            state.refreshEdit = CreateControl(state, L"EDIT", L"",
                WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
                112, 15, 60, 22, IDC_REFRESH_EDIT);

            CreateControl(state, L"STATIC", L"最大设备数", SS_LEFT,
                190, 18, 80, 20, IDC_MAX_LABEL);
            state.maxEdit = CreateControl(state, L"EDIT", L"",
                WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
                278, 15, 50, 22, IDC_MAX_EDIT);

            CreateControl(state, L"STATIC", L"显示模式", SS_LEFT,
                12, 48, 90, 20, IDC_MODE_LABEL);
            state.modeCombo = CreateControl(state, L"COMBOBOX", L"",
                CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                112, 45, width - 124, 120, IDC_MODE_COMBO);

            CreateControl(state, L"BUTTON", L"已发现 / 已配置的设备",
                BS_GROUPBOX, 12, 80, width - 24, height - 150, IDC_DEVICE_GROUP);

            state.deviceList = CreateControl(state, L"LISTBOX", L"",
                LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
                22, 100, 180, std::max(80, height - 190), IDC_DEVICE_LIST);

            state.visibleCheck = CreateControl(state, L"BUTTON",
                L"在任务栏显示", BS_AUTOCHECKBOX | WS_TABSTOP,
                215, 100, 140, 20, IDC_VISIBLE_CHECK);

            CreateControl(state, L"STATIC", L"名称", SS_LEFT,
                215, 128, 50, 20, IDC_NAME_LABEL);
            state.nameEdit = CreateControl(state, L"EDIT", L"",
                WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                270, 125, width - 295, 22, IDC_NAME_EDIT);

            CreateControl(state, L"STATIC", L"标签", SS_LEFT,
                215, 156, 50, 20, IDC_LABEL_LABEL);
            state.labelEdit = CreateControl(state, L"EDIT", L"",
                WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                270, 153, width - 295, 22, IDC_LABEL_EDIT);

            CreateControl(state, L"STATIC", L"优先级", SS_LEFT,
                215, 184, 50, 20, IDC_PRIORITY_LABEL);
            state.priorityEdit = CreateControl(state, L"EDIT", L"",
                WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
                270, 181, 70, 22, IDC_PRIORITY_EDIT);

            CreateControl(state, L"STATIC", L"颜色", SS_LEFT,
                215, 212, 50, 20, IDC_COLOR_LABEL);
            state.colorBtn = CreateControl(state, L"BUTTON", L"auto（按电量）",
                BS_PUSHBUTTON | WS_TABSTOP,
                270, 209, 100, 24, IDC_COLOR_BTN);
            state.autoColorCheck = CreateControl(state, L"BUTTON",
                L"按电量自动着色", BS_AUTOCHECKBOX | WS_TABSTOP,
                378, 211, 120, 20, IDC_AUTO_COLOR);

            CreateControl(state, L"BUTTON", L"上移",
                BS_PUSHBUTTON | WS_TABSTOP,
                215, 244, 60, 24, IDC_MOVE_UP);
            CreateControl(state, L"BUTTON", L"下移",
                BS_PUSHBUTTON | WS_TABSTOP,
                281, 244, 60, 24, IDC_MOVE_DOWN);
            CreateControl(state, L"BUTTON", L"重新扫描",
                BS_PUSHBUTTON | WS_TABSTOP,
                347, 244, 90, 24, IDC_RESCAN);

            CreateControl(state, L"BUTTON", L"高级设置...",
                BS_PUSHBUTTON | WS_TABSTOP,
                width - 130, 244, 118, 24, IDC_ADVANCED);

            state.statusText = CreateControl(state, L"STATIC",
                L"保存后立即生效；hybrid 模式需在主程序任务栏设置里关闭「水平排列」。",
                SS_LEFT, 12, height - 62, width - 24, 20, IDC_STATUS);

            CreateControl(state, L"BUTTON", L"确定",
                BS_DEFPUSHBUTTON | WS_TABSTOP,
                width - 190, height - 34, 80, 26, IDOK);
            CreateControl(state, L"BUTTON", L"取消",
                BS_PUSHBUTTON | WS_TABSTOP,
                width - 100, height - 34, 80, 26, IDCANCEL);

            SetText(state.refreshEdit, ToWString(state.working.refreshIntervalSeconds));
            SetText(state.maxEdit, ToWString(state.working.maxDevices));
        }

        /** The list is driven by discovery + persisted settings, not the registry. */
        void PopulateDevices(DialogState& state)
        {
            // LB_RESETCONTENT / LB_ADDSTRING can emit selection notifications.
            // Rebuilding the list must never look like a user selection change.
            state.suppressSelChange = true;
            SendMessageW(state.deviceList, LB_RESETCONTENT, 0, 0);
            state.deviceIds.clear();

            if (state.registry == nullptr)
            {
                state.suppressSelChange = false;
                return;
            }

            state.deviceIds = state.working.KnownDeviceIds(*state.registry);
            for (const std::wstring& id : state.presentIds)
            {
                if (state.registry->FindById(id) == nullptr)
                    continue;
                if (std::find(state.deviceIds.begin(), state.deviceIds.end(), id)
                    == state.deviceIds.end())
                {
                    state.deviceIds.push_back(id);
                }
            }

            std::stable_sort(state.deviceIds.begin(), state.deviceIds.end(),
                [&state](const std::wstring& left, const std::wstring& right)
                {
                    return GetDeviceSettings(state, left).priority <
                        GetDeviceSettings(state, right).priority;
                });

            for (const std::wstring& id : state.deviceIds)
            {
                std::wstring text = DeviceCaption(state, id);
                SendMessageW(state.deviceList, LB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(text.c_str()));
            }
            state.suppressSelChange = false;
        }

        void SyncColorControls(DialogState& state, bool autoColor)
        {
            CheckDlgButton(state.hDlg, IDC_AUTO_COLOR,
                autoColor ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(state.colorBtn, !autoColor);
        }

        void LoadDevice(DialogState& state, int index)
        {
            if (index < 0 || index >= static_cast<int>(state.deviceIds.size()))
            {
                state.current = -1;
                EnableWindow(state.visibleCheck, FALSE);
                EnableWindow(state.nameEdit, FALSE);
                EnableWindow(state.labelEdit, FALSE);
                EnableWindow(state.priorityEdit, FALSE);
                EnableWindow(state.colorBtn, FALSE);
                EnableWindow(state.autoColorCheck, FALSE);
                return;
            }

            state.current = index;
            DeviceSettings& settings = GetDeviceSettings(state,
                state.deviceIds[index]);
            CheckDlgButton(state.hDlg, IDC_VISIBLE_CHECK,
                settings.hidden ? BST_UNCHECKED : BST_CHECKED);
            SetText(state.nameEdit, settings.name);
            SetText(state.labelEdit, settings.label);
            SetText(state.priorityEdit, ToWString(settings.priority));
            bool autoColor = settings.color.empty() || settings.color == L"auto";
            SetText(state.colorBtn, autoColor
                ? L"auto（按电量）" : settings.color);

            EnableWindow(state.visibleCheck, TRUE);
            EnableWindow(state.nameEdit, TRUE);
            EnableWindow(state.labelEdit, TRUE);
            EnableWindow(state.priorityEdit, TRUE);
            EnableWindow(state.autoColorCheck, TRUE);
            SyncColorControls(state, autoColor);
        }

        void CommitCurrent(DialogState& state)
        {
            if (state.current < 0 ||
                state.current >= static_cast<int>(state.deviceIds.size()))
                return;

            DeviceSettings& settings = GetDeviceSettings(state,
                state.deviceIds[state.current]);
            settings.hidden = IsDlgButtonChecked(state.hDlg,
                IDC_VISIBLE_CHECK) != BST_CHECKED;
            settings.name = GetText(state.nameEdit);
            settings.label = GetText(state.labelEdit);

            int priority = _wtoi(GetText(state.priorityEdit).c_str());
            settings.priority = std::max(0, std::min(999, priority));

            if (IsDlgButtonChecked(state.hDlg, IDC_AUTO_COLOR) == BST_CHECKED)
                settings.color = L"auto";
            else if (settings.color.empty() || settings.color == L"auto")
                settings.color = L"#4CAF50";

            // Only touch the list when the caption actually changed. Rebuilding
            // a row emits selection notifications, and the old unconditional
            // LB_SETCURSEL(state.current) forced every new selection back to
            // the previously selected (usually first) device.
            std::wstring text = DeviceCaption(state,
                state.deviceIds[state.current]);
            wchar_t existing[512] = {};
            LRESULT length = SendMessageW(state.deviceList, LB_GETTEXT,
                state.current, reinterpret_cast<LPARAM>(existing));
            if (length == LB_ERR || text != existing)
            {
                int keep = static_cast<int>(
                    SendMessageW(state.deviceList, LB_GETCURSEL, 0, 0));
                state.suppressSelChange = true;
                SendMessageW(state.deviceList, LB_DELETESTRING,
                    state.current, 0);
                SendMessageW(state.deviceList, LB_INSERTSTRING,
                    state.current,
                    reinterpret_cast<LPARAM>(text.c_str()));
                if (keep >= 0 &&
                    keep < static_cast<int>(state.deviceIds.size()))
                {
                    SendMessageW(state.deviceList, LB_SETCURSEL, keep, 0);
                }
                state.suppressSelChange = false;
            }
        }

        /** Select a row without letting LB_SETCURSEL re-enter the handler. */
        void SelectDeviceRow(DialogState& state, int index)
        {
            state.suppressSelChange = true;
            SendMessageW(state.deviceList, LB_SETCURSEL, index, 0);
            state.suppressSelChange = false;
            LoadDevice(state, index);
        }

        void RefreshListKeepingSelection(DialogState& state)
        {
            std::wstring selectedId;
            if (state.current >= 0 &&
                state.current < static_cast<int>(state.deviceIds.size()))
            {
                CommitCurrent(state);
                selectedId = state.deviceIds[state.current];
            }

            PopulateDevices(state);

            int selected = 0;
            auto iter = std::find(state.deviceIds.begin(), state.deviceIds.end(),
                selectedId);
            if (!selectedId.empty() && iter != state.deviceIds.end())
                selected = static_cast<int>(iter - state.deviceIds.begin());
            if (state.deviceIds.empty())
                selected = -1;

            SelectDeviceRow(state, selected);
        }

        void MoveDevice(DialogState& state, int delta)
        {
            if (state.current < 0)
                return;
            int target = state.current + delta;
            if (target < 0 || target >= static_cast<int>(state.deviceIds.size()))
                return;

            CommitCurrent(state);
            std::swap(state.deviceIds[state.current], state.deviceIds[target]);
            for (size_t i = 0; i < state.deviceIds.size(); ++i)
            {
                GetDeviceSettings(state, state.deviceIds[i]).priority =
                    static_cast<int>(i + 1) * 10;
            }

            PopulateDevices(state);
            SelectDeviceRow(state, target);
        }

        void ReadGlobals(DialogState& state)
        {
            int refresh = _wtoi(GetText(state.refreshEdit).c_str());
            refresh = std::max(5, std::min(3600, refresh));
            state.working.refreshIntervalSeconds = refresh;
            SetText(state.refreshEdit, ToWString(refresh));

            int maxDevices = _wtoi(GetText(state.maxEdit).c_str());
            maxDevices = std::max(1, std::min(8, maxDevices));
            state.working.maxDevices = maxDevices;
            SetText(state.maxEdit, ToWString(maxDevices));

            LRESULT mode = SendMessageW(state.modeCombo, CB_GETCURSEL, 0, 0);
            // CB_ERR means the combo has no valid selection (e.g. it was
            // rebuilt). Keep the loaded value instead of silently writing bar
            // over the user's display mode when the dialog is confirmed.
            if (mode != CB_ERR)
            {
                if (mode == 1)
                    state.working.displayMode = L"text";
                else if (mode == 2)
                    state.working.displayMode = L"hybrid";
                else if (mode == 3)
                    state.working.displayMode = L"stacked";
                else
                    state.working.displayMode = L"bar";
            }
        }

        void ChooseColor(DialogState& state)
        {
            if (state.current < 0)
                return;

            DeviceSettings& settings = GetDeviceSettings(state,
                state.deviceIds[state.current]);
            COLORREF initial = BatteryItem::ParseColor(settings.color,
                RGB(76, 175, 80));

            CHOOSECOLORW choose = {};
            choose.lStructSize = sizeof(choose);
            choose.hwndOwner = state.hDlg;
            choose.rgbResult = initial;
            choose.lpCustColors = state.customColors;
            choose.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColorW(&choose))
            {
                settings.color = ColorToHex(choose.rgbResult);
                SetText(state.colorBtn, settings.color);
                CheckDlgButton(state.hDlg, IDC_AUTO_COLOR, BST_UNCHECKED);
                EnableWindow(state.colorBtn, TRUE);
            }
        }

        bool SamePresentSet(const std::vector<std::wstring>& left,
            const std::vector<std::wstring>& right)
        {
            std::vector<std::wstring> a = left;
            std::vector<std::wstring> b = right;
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            return a == b;
        }

        void OnRescanTick(DialogState& state, HWND hDlg)
        {
            std::vector<std::wstring> latest;
            if (state.presentProvider)
                latest = state.presentProvider();

            if (!SamePresentSet(latest, state.presentIds))
            {
                state.presentIds = std::move(latest);
                RefreshListKeepingSelection(state);
                SetText(state.statusText, L"扫描完成，设备列表已更新；保存后立即生效。");
                KillTimer(hDlg, kRescanTimer);
                state.rescanPending = false;
                return;
            }

            if (++state.rescanTicks >= 10)
            {
                SetText(state.statusText,
                    L"扫描完成，未发现新设备；保存后立即生效。");
                KillTimer(hDlg, kRescanTimer);
                state.rescanPending = false;
            }
        }

        HWND CreateAdvControl(AdvancedState& state, const wchar_t* className,
            const wchar_t* text, DWORD style,
            int x, int y, int width, int height, int id)
        {
            HWND control = CreateWindowExW(0, className, text,
                WS_CHILD | WS_VISIBLE | style,
                x, y, width, height, state.hDlg,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                GetModuleHandleW(nullptr), nullptr);
            HFONT font = reinterpret_cast<HFONT>(
                SendMessageW(state.hDlg, WM_GETFONT, 0, 0));
            if (font == nullptr)
                font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            SendMessageW(control, WM_SETFONT,
                reinterpret_cast<WPARAM>(font), TRUE);
            return control;
        }

        void LayoutAdvanced(AdvancedState& state)
        {
            RECT client = {};
            GetClientRect(state.hDlg, &client);
            const int width = client.right;
            const int height = client.bottom;
            const int col2 = width / 2 + 8;
            const int checkWidth = width / 2 - 24;

            auto check = [&](const wchar_t* text, int x, int y, int id) -> HWND
            {
                return CreateAdvControl(state, L"BUTTON", text,
                    BS_AUTOCHECKBOX | WS_TABSTOP,
                    x, y, checkWidth, 20, id);
            };
            auto number = [&](const wchar_t* label, int x, int y, int id) -> HWND
            {
                CreateAdvControl(state, L"STATIC", label, SS_LEFT,
                    x, y + 3, 130, 18, 0);
                return CreateAdvControl(state, L"EDIT", L"",
                    WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
                    x + 136, y, 54, 22, id);
            };

            state.keepLast = check(L"休眠时保留最后读数", 12, 14, ADV_KEEP_LAST);
            state.lowNotify = check(L"低电量提醒", col2, 14, ADV_LOW_NOTIFY);
            state.lowPercent = number(L"提醒阈值 (%)", 12, 40, ADV_LOW_PERCENT);
            state.grace = number(L"设备宽限期 (秒)", col2, 40, ADV_GRACE);
            state.stackedPercent = check(L"stacked 显示百分比", 12, 68,
                ADV_STACKED_PERCENT);
            state.framed = check(L"长条带边框 (framed)", col2, 68, ADV_FRAMED);
            state.hybridRows = number(L"hybrid 行数", 12, 94, ADV_HYBRID_ROWS);
            state.rowBarWidth = number(L"行条宽度 (px)", col2, 94,
                ADV_ROW_BAR_WIDTH);
            state.batteryLow = number(L"低电量阈值 (%)", 12, 122,
                ADV_BATTERY_LOW);
            state.batteryGood = number(L"良好电量阈值 (%)", col2, 122,
                ADV_BATTERY_GOOD);
            state.cpuOrange = number(L"CPU 橙色 (%)", 12, 150, ADV_CPU_ORANGE);
            state.cpuRed = number(L"CPU 红色 (%)", col2, 150, ADV_CPU_RED);
            state.showOffline = check(L"离线时显示 --", 12, 178,
                ADV_SHOW_OFFLINE);

            CreateAdvControl(state, L"STATIC",
                L"数值会自动限制在合法范围；颜色值仍可在 INI 中自定义。",
                SS_LEFT, 12, height - 62, width - 24, 20, 0);
            CreateAdvControl(state, L"BUTTON", L"确定",
                BS_DEFPUSHBUTTON | WS_TABSTOP,
                width - 190, height - 34, 80, 26, IDOK);
            CreateAdvControl(state, L"BUTTON", L"取消",
                BS_PUSHBUTTON | WS_TABSTOP,
                width - 100, height - 34, 80, 26, IDCANCEL);
        }

        void LoadAdvanced(AdvancedState& state)
        {
            const Settings& s = *state.working;
            auto tick = [&](int id, bool on)
            {
                CheckDlgButton(state.hDlg, id,
                    on ? BST_CHECKED : BST_UNCHECKED);
            };
            tick(ADV_KEEP_LAST, s.keepLastKnown);
            tick(ADV_LOW_NOTIFY, s.lowBatteryNotify);
            tick(ADV_STACKED_PERCENT, s.stackedShowPercent);
            tick(ADV_FRAMED, s.rowStyle == L"framed");
            tick(ADV_SHOW_OFFLINE, s.showOffline);

            SetText(state.lowPercent, ToWString(s.lowBatteryNotifyPercent));
            SetText(state.grace, ToWString(s.presenceGraceSeconds));
            SetText(state.hybridRows, ToWString(s.hybridRows));
            SetText(state.rowBarWidth, ToWString(s.rowBarWidth));
            SetText(state.batteryLow, ToWString(s.batteryLowPercent));
            SetText(state.batteryGood, ToWString(s.batteryGoodPercent));
            SetText(state.cpuOrange, ToWString(s.cpuOrangePercent));
            SetText(state.cpuRed, ToWString(s.cpuRedPercent));
        }

        void CommitAdvanced(AdvancedState& state)
        {
            Settings& s = *state.working;
            auto checked = [&](int id)
            {
                return IsDlgButtonChecked(state.hDlg, id) == BST_CHECKED;
            };
            auto number = [&](HWND edit)
            {
                return _wtoi(GetText(edit).c_str());
            };

            s.keepLastKnown = checked(ADV_KEEP_LAST);
            s.lowBatteryNotify = checked(ADV_LOW_NOTIFY);
            s.stackedShowPercent = checked(ADV_STACKED_PERCENT);
            s.showOffline = checked(ADV_SHOW_OFFLINE);
            s.rowStyle = checked(ADV_FRAMED) ? L"framed" : L"solid";

            s.lowBatteryNotifyPercent = number(state.lowPercent);
            s.presenceGraceSeconds = number(state.grace);
            s.hybridRows = number(state.hybridRows);
            s.rowBarWidth = number(state.rowBarWidth);
            s.batteryLowPercent = number(state.batteryLow);
            s.batteryGoodPercent = number(state.batteryGood);
            s.cpuOrangePercent = number(state.cpuOrange);
            s.cpuRedPercent = number(state.cpuRed);
            // Same limits as INI loading, including the low < good and
            // orange < red ordering, in one place that tests can exercise.
            s.ClampValues();
        }

        INT_PTR CALLBACK AdvancedProc(HWND hDlg, UINT message,
            WPARAM wParam, LPARAM lParam)
        {
            if (message == WM_INITDIALOG)
            {
                AdvancedState* state =
                    reinterpret_cast<AdvancedState*>(lParam);
                SetWindowLongPtrW(hDlg, DWLP_USER,
                    reinterpret_cast<LONG_PTR>(state));
                state->hDlg = hDlg;
                LayoutAdvanced(*state);
                LoadAdvanced(*state);
                return TRUE;
            }

            AdvancedState* state = reinterpret_cast<AdvancedState*>(
                GetWindowLongPtrW(hDlg, DWLP_USER));
            if (state == nullptr)
                return FALSE;

            if (message == WM_COMMAND)
            {
                int id = LOWORD(wParam);
                if (id == IDOK)
                {
                    CommitAdvanced(*state);
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                if (id == IDCANCEL)
                {
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                }
                return FALSE;
            }
            if (message == WM_CLOSE)
            {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            return FALSE;
        }

        bool ShowAdvanced(HWND parent, Settings& settings)
        {
            AdvancedState state;
            state.working = &settings;

            DialogTemplate tmpl = {};
            tmpl.dlg.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;
            tmpl.dlg.dwExtendedStyle = 0;
            tmpl.dlg.cdit = 0;
            tmpl.dlg.x = 0;
            tmpl.dlg.y = 0;
            tmpl.dlg.cx = 340;
            tmpl.dlg.cy = 200;
            wcscpy_s(tmpl.title, L"高级设置");

            return DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                &tmpl.dlg, parent, AdvancedProc,
                reinterpret_cast<LPARAM>(&state)) == IDOK;
        }

        INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message,
            WPARAM wParam, LPARAM lParam)
        {
            if (message == WM_INITDIALOG)
            {
                DialogState* state = reinterpret_cast<DialogState*>(lParam);
                SetWindowLongPtrW(hDlg, DWLP_USER,
                    reinterpret_cast<LONG_PTR>(state));
                state->hDlg = hDlg;
                LayoutControls(*state);

                SendMessageW(state->modeCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(L"bar（电池图标 + 电量条）"));
                SendMessageW(state->modeCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(L"text（纯文本百分比）"));
                SendMessageW(state->modeCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(
                        L"hybrid（CPU/内存 + 电量，纵向 4 行）"));
                SendMessageW(state->modeCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(
                        L"stacked（CPU/内存 + 双设备，纵向 3 行）"));
                int modeIndex = state->working.displayMode == L"text" ? 1
                    : (state->working.displayMode == L"hybrid" ? 2
                        : (state->working.displayMode == L"stacked" ? 3 : 0));
                SendMessageW(state->modeCombo, CB_SETCURSEL, modeIndex, 0);

                PopulateDevices(*state);
                SelectDeviceRow(*state,
                    state->deviceIds.empty() ? -1 : 0);
                return TRUE;
            }

            DialogState* state = GetState(hDlg);
            if (state == nullptr)
                return FALSE;

            if (message == WM_COMMAND)
            {
                int id = LOWORD(wParam);
                int code = HIWORD(wParam);

                if (id == IDOK)
                {
                    CommitCurrent(*state);
                    ReadGlobals(*state);
                    // Everything shown in the list becomes user-configured, so
                    // its section is persisted on save.
                    for (const std::wstring& deviceId : state->deviceIds)
                        state->working.NoteDevice(deviceId);
                    *state->target = state->working;
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                if (id == IDCANCEL)
                {
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                }
                if (id == IDC_COLOR_BTN)
                {
                    CommitCurrent(*state);
                    ChooseColor(*state);
                    return TRUE;
                }
                if (id == IDC_AUTO_COLOR)
                {
                    CommitCurrent(*state);
                    LoadDevice(*state, state->current);
                    return TRUE;
                }
                if (id == IDC_MOVE_UP)
                {
                    MoveDevice(*state, -1);
                    return TRUE;
                }
                if (id == IDC_MOVE_DOWN)
                {
                    MoveDevice(*state, 1);
                    return TRUE;
                }
                if (id == IDC_RESCAN)
                {
                    CommitCurrent(*state);
                    if (state->onRescan)
                        state->onRescan();
                    state->rescanTicks = 0;
                    state->rescanPending = true;
                    SetTimer(hDlg, kRescanTimer, 400, nullptr);
                    SetText(state->statusText, L"正在重新扫描设备…");
                    return TRUE;
                }
                if (id == IDC_ADVANCED)
                {
                    CommitCurrent(*state);
                    if (ShowAdvanced(hDlg, state->working))
                    {
                        SetText(state->statusText,
                            L"高级设置已更新，点「确定」后写入配置文件。");
                    }
                    return TRUE;
                }
                if (id == IDC_DEVICE_LIST && code == LBN_SELCHANGE)
                {
                    if (state->suppressSelChange)
                        return TRUE;
                    // Capture the user's new index first: CommitCurrent may
                    // rebuild the caption row and restores the previous
                    // selection, so reading LB_GETCURSEL afterwards snapped
                    // every click back to the first device.
                    int selected = static_cast<int>(
                        SendMessageW(state->deviceList, LB_GETCURSEL, 0, 0));
                    if (selected < 0 ||
                        selected >= static_cast<int>(state->deviceIds.size()))
                    {
                        return TRUE;
                    }
                    CommitCurrent(*state);
                    LoadDevice(*state, selected);
                    return TRUE;
                }
                return FALSE;
            }

            if (message == WM_TIMER && wParam == kRescanTimer &&
                state->rescanPending)
            {
                OnRescanTick(*state, hDlg);
                return TRUE;
            }

            if (message == WM_DESTROY)
            {
                KillTimer(hDlg, kRescanTimer);
                return TRUE;
            }

            if (message == WM_CLOSE)
            {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            return FALSE;
        }
    }

    bool OptionsDialog::Show(HWND parent, Settings& settings,
        const DeviceRegistry& registry,
        const std::function<std::vector<std::wstring>()>& presentProvider,
        const std::function<void()>& onRescan)
    {
        DialogState state;
        state.working = settings;
        state.target = &settings;
        state.registry = &registry;
        state.presentProvider = presentProvider;
        state.onRescan = onRescan;
        if (presentProvider)
            state.presentIds = presentProvider();

        DialogTemplate tmpl = {};
        tmpl.dlg.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;
        tmpl.dlg.dwExtendedStyle = 0;
        tmpl.dlg.cdit = 0;
        tmpl.dlg.x = 0;
        tmpl.dlg.y = 0;
        tmpl.dlg.cx = 380;
        tmpl.dlg.cy = 250;
        wcscpy_s(tmpl.title, L"无线外设电量设置");

        INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
            &tmpl.dlg, parent, DialogProc,
            reinterpret_cast<LPARAM>(&state));
        return result == IDOK;
    }
}
