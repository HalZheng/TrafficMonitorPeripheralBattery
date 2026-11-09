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
            IDC_ENABLED_CHECK,
            IDC_NAME_LABEL,
            IDC_NAME_EDIT,
            IDC_LABEL_LABEL,
            IDC_LABEL_EDIT,
            IDC_PRIORITY_LABEL,
            IDC_PRIORITY_EDIT,
            IDC_COLOR_LABEL,
            IDC_COLOR_BTN,
            IDC_HINT
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
            HWND hDlg = nullptr;
            HWND refreshEdit = nullptr;
            HWND maxEdit = nullptr;
            HWND modeCombo = nullptr;
            HWND deviceList = nullptr;
            HWND enabledCheck = nullptr;
            HWND nameEdit = nullptr;
            HWND labelEdit = nullptr;
            HWND priorityEdit = nullptr;
            HWND colorBtn = nullptr;
            std::vector<std::wstring> deviceIds;
            int current = -1;
            COLORREF customColors[16] = {};
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
                    fallback.enabled = device->enabledByDefault;
                    fallback.name = device->name;
                    fallback.label = device->defaultLabel;
                    fallback.priority = device->priority;
                }
            }
            fallback.color = L"#4FC3F7";
            state.working.devices[id] = fallback;
            return state.working.devices[id];
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

            CreateControl(state, L"BUTTON", L"设备", BS_GROUPBOX,
                12, 80, width - 24, height - 150, IDC_DEVICE_GROUP);

            state.deviceList = CreateControl(state, L"LISTBOX", L"",
                LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
                22, 100, 180, std::max(80, height - 190), IDC_DEVICE_LIST);

            state.enabledCheck = CreateControl(state, L"BUTTON", L"启用",
                BS_AUTOCHECKBOX | WS_TABSTOP,
                215, 100, 120, 20, IDC_ENABLED_CHECK);

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
            state.colorBtn = CreateControl(state, L"BUTTON", L"#4FC3F7",
                BS_PUSHBUTTON | WS_TABSTOP,
                270, 209, 100, 24, IDC_COLOR_BTN);

            CreateControl(state, L"STATIC",
                L"修改后点击确定；设备数量/启用状态变化需重启。",
                SS_LEFT, 12, height - 62, width - 24, 20, IDC_HINT);

            CreateControl(state, L"BUTTON", L"确定",
                BS_DEFPUSHBUTTON | WS_TABSTOP,
                width - 190, height - 34, 80, 26, IDOK);
            CreateControl(state, L"BUTTON", L"取消",
                BS_PUSHBUTTON | WS_TABSTOP,
                width - 100, height - 34, 80, 26, IDCANCEL);

            SetText(state.refreshEdit, ToWString(state.working.refreshIntervalSeconds));
            SetText(state.maxEdit, ToWString(state.working.maxDevices));
        }

        void PopulateDevices(DialogState& state)
        {
            SendMessageW(state.deviceList, LB_RESETCONTENT, 0, 0);
            state.deviceIds.clear();

            if (state.registry == nullptr)
                return;

            for (const DeviceDescriptor& device : state.registry->All())
            {
                const DeviceSettings& settings = GetDeviceSettings(state, device.id);
                std::wstring text = settings.enabled ? L"[启用] " : L"[禁用] ";
                text += settings.name.empty() ? device.name : settings.name;
                SendMessageW(state.deviceList, LB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(text.c_str()));
                state.deviceIds.push_back(device.id);
            }
        }

        void LoadDevice(DialogState& state, int index)
        {
            if (index < 0 || index >= static_cast<int>(state.deviceIds.size()))
            {
                state.current = -1;
                EnableWindow(state.enabledCheck, FALSE);
                EnableWindow(state.nameEdit, FALSE);
                EnableWindow(state.labelEdit, FALSE);
                EnableWindow(state.priorityEdit, FALSE);
                EnableWindow(state.colorBtn, FALSE);
                return;
            }

            state.current = index;
            DeviceSettings& settings = GetDeviceSettings(state,
                state.deviceIds[index]);
            CheckDlgButton(state.hDlg, IDC_ENABLED_CHECK,
                settings.enabled ? BST_CHECKED : BST_UNCHECKED);
            SetText(state.nameEdit, settings.name);
            SetText(state.labelEdit, settings.label);
            SetText(state.priorityEdit, ToWString(settings.priority));
            SetText(state.colorBtn, settings.color.empty()
                ? L"#4FC3F7" : settings.color);

            EnableWindow(state.enabledCheck, TRUE);
            EnableWindow(state.nameEdit, TRUE);
            EnableWindow(state.labelEdit, TRUE);
            EnableWindow(state.priorityEdit, TRUE);
            EnableWindow(state.colorBtn, TRUE);
        }

        void CommitCurrent(DialogState& state)
        {
            if (state.current < 0 ||
                state.current >= static_cast<int>(state.deviceIds.size()))
                return;

            DeviceSettings& settings = GetDeviceSettings(state,
                state.deviceIds[state.current]);
            settings.enabled = IsDlgButtonChecked(state.hDlg,
                IDC_ENABLED_CHECK) == BST_CHECKED;
            settings.name = GetText(state.nameEdit);
            settings.label = GetText(state.labelEdit);

            int priority = _wtoi(GetText(state.priorityEdit).c_str());
            settings.priority = std::max(0, std::min(999, priority));

            std::wstring text = settings.enabled ? L"[启用] " : L"[禁用] ";
            text += settings.name.empty()
                ? state.deviceIds[state.current] : settings.name;
            SendMessageW(state.deviceList, LB_DELETESTRING,
                state.current, 0);
            SendMessageW(state.deviceList, LB_INSERTSTRING,
                state.current, reinterpret_cast<LPARAM>(text.c_str()));
            SendMessageW(state.deviceList, LB_SETCURSEL, state.current, 0);
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
            state.working.displayMode = (mode == 1) ? L"text" : L"bar";
        }

        void ChooseColor(DialogState& state)
        {
            if (state.current < 0)
                return;

            DeviceSettings& settings = GetDeviceSettings(state,
                state.deviceIds[state.current]);
            COLORREF initial = BatteryItem::ParseColor(settings.color,
                RGB(79, 195, 247));

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
            }
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
                SendMessageW(state->modeCombo, CB_SETCURSEL,
                    state->working.displayMode == L"text" ? 1 : 0, 0);

                PopulateDevices(*state);
                LoadDevice(*state, 0);
                return TRUE;
            }

            if (message == WM_COMMAND)
            {
                DialogState* state = GetState(hDlg);
                if (state == nullptr)
                    return FALSE;

                int id = LOWORD(wParam);
                int code = HIWORD(wParam);

                if (id == IDOK)
                {
                    CommitCurrent(*state);
                    ReadGlobals(*state);
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
                if (id == IDC_DEVICE_LIST && code == LBN_SELCHANGE)
                {
                    CommitCurrent(*state);
                    int selected = static_cast<int>(
                        SendMessageW(state->deviceList, LB_GETCURSEL, 0, 0));
                    LoadDevice(*state, selected);
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
    }

    bool OptionsDialog::Show(HWND parent, Settings& settings,
        const DeviceRegistry& registry)
    {
        DialogState state;
        state.working = settings;
        state.target = &settings;
        state.registry = &registry;

        DialogTemplate tmpl = {};
        tmpl.dlg.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;
        tmpl.dlg.dwExtendedStyle = 0;
        tmpl.dlg.cdit = 0;
        tmpl.dlg.x = 0;
        tmpl.dlg.y = 0;
        tmpl.dlg.cx = 330;
        tmpl.dlg.cy = 250;
        wcscpy_s(tmpl.title, L"无线外设电量设置");

        INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
            &tmpl.dlg, parent, DialogProc,
            reinterpret_cast<LPARAM>(&state));
        return result == IDOK;
    }
}
