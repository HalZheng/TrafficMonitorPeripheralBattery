#include "Plugin.h"

#include "OptionsDialog.h"

#include "../util/PathUtil.h"
#include "Version.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <utility>

namespace pb
{
    namespace
    {
        unsigned long long ConfigStamp(const std::wstring& path)
        {
            WIN32_FILE_ATTRIBUTE_DATA info = {};
            if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info))
                return 0;
            ULARGE_INTEGER stamp = {};
            stamp.HighPart = info.ftLastWriteTime.dwHighDateTime;
            stamp.LowPart = info.ftLastWriteTime.dwLowDateTime;
            return stamp.QuadPart;
        }
    }

    Plugin::~Plugin()
    {
        Stop();
    }

    void Plugin::OnInitialize(ITrafficMonitor* pApp)
    {
        m_app = pApp;
        m_configPath = ConfigPath();

        // Make sure the plugin config directory exists.
        size_t slash = m_configPath.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            CreateDirectoryW(m_configPath.substr(0, slash).c_str(), nullptr);

        m_settings.Load(m_configPath, m_registry);
        MigrateLegacyColors();

        // The item is created once and never replaced: the host caches the
        // pointer and uses GetItemId() as the display-settings key.
        m_item.reset(new BatteryGroupItem());

        m_manager.Configure(m_registry.All(), m_settings.refreshIntervalSeconds);
        ApplySettings();
        m_manager.Start();

        m_configStamp = ConfigStamp(m_configPath);
        ShowMigrationNoticeOnce();
    }

    void Plugin::ApplySettings()
    {
        if (m_item)
        {
            m_item->SetDisplayMode(m_settings.displayMode);
            m_item->SetHybridRows(m_settings.hybridRows);
            m_item->SetStackedOptions(m_settings.stackedShowPercent);
            ApplyColorRules();
        }
        ApplyDisplayOptions();

        m_manager.SetRefreshSeconds(m_settings.refreshIntervalSeconds);
        m_manager.SetGraceSeconds(m_settings.presenceGraceSeconds);
        m_manager.SetKeepLastKnown(m_settings.keepLastKnown);
        m_notifier.Configure(m_settings.lowBatteryNotify,
            m_settings.lowBatteryNotifyPercent);

        std::vector<std::wstring> hidden;
        for (const auto& pair : m_settings.devices)
        {
            if (pair.second.hidden)
                hidden.push_back(pair.first);
        }
        m_manager.SetHiddenIds(hidden);
    }

    void Plugin::ApplyDisplayOptions()
    {
        if (!m_item)
            return;
        bool hidePercent = m_drawTaskbar
            ? m_hidePercentTaskbar : m_hidePercentMain;
        m_item->SetDisplayOptions(hidePercent, m_labelColor, m_valueColor);
    }

    void Plugin::ApplyColorRules()
    {
        if (!m_item)
            return;

        ColorRules rules;
        rules.lowPercent = m_settings.batteryLowPercent;
        rules.goodPercent = m_settings.batteryGoodPercent;
        rules.cpuOrangePercent = m_settings.cpuOrangePercent;
        rules.cpuRedPercent = m_settings.cpuRedPercent;
        rules.rowBarWidth = m_settings.rowBarWidth;
        rules.frameless = m_settings.rowStyle != L"framed";
        rules.high = BatteryItem::ParseColor(m_settings.colorHigh, rules.high);
        rules.medium = BatteryItem::ParseColor(m_settings.colorMedium, rules.medium);
        rules.low = BatteryItem::ParseColor(m_settings.colorLow, rules.low);
        rules.charging = BatteryItem::ParseColor(m_settings.colorCharging,
            rules.charging);
        rules.hostTextColor = m_valueColor;
        m_item->SetColorRules(rules);
    }

    void Plugin::MigrateLegacyColors()
    {
        if (m_settings.colorAutoMigrationDone)
            return;

        // 1.2.0-1.4.0 assigned a palette colour by registry order, not by
        // charge level. Those values were defaults, not user choices, so move
        // them to "auto" exactly once; a genuinely custom colour survives.
        static const wchar_t* legacyPalette[] = {
            L"#4FC3F7", L"#81C784", L"#FFB74D",
            L"#E57373", L"#BA68C8", L"#64B5F6"
        };
        for (auto& pair : m_settings.devices)
        {
            for (const wchar_t* color : legacyPalette)
            {
                if (pair.second.color == color)
                {
                    pair.second.color = L"auto";
                    break;
                }
            }
        }
        m_settings.colorAutoMigrationDone = true;
        SaveSettings();
    }

    IPluginItem* Plugin::GetItem(int index)
    {
        if (index != 0 || !m_item)
            return nullptr;
        return m_item.get();
    }

    void Plugin::SaveSettings()
    {
        m_settings.Save(m_configPath, m_registry);
        m_configStamp = ConfigStamp(m_configPath);
    }

    void Plugin::ReloadSettingsIfChanged()
    {
        unsigned long long now = GetTickCount64();
        if (now - m_lastStampCheck < 2000)
            return;
        m_lastStampCheck = now;

        unsigned long long stamp = ConfigStamp(m_configPath);
        if (stamp == 0 || stamp == m_configStamp)
            return;
        m_configStamp = stamp;

        Settings reloaded;
        reloaded.Load(m_configPath, m_registry);
        m_settings = std::move(reloaded);
        ApplySettings();
        m_manager.RequestRefresh();
    }

    void Plugin::ShowMigrationNoticeOnce()
    {
        if (m_settings.singleItemNoticeShown || !FileExists(m_configPath))
            return;
        // Only old installations have [device:...] sections from the previous
        // per-device item layout; they need to re-tick the single new item.
        if (m_settings.persistedDevices.empty())
            return;

        m_settings.singleItemNoticeShown = true;
        SaveSettings();
        if (m_app != nullptr)
        {
            m_app->ShowNotifyMessage(
                L"无线外设电量已改为单个显示项目，请在主程序「显示设置」中重新勾选「无线外设电量」。");
        }
    }

    void Plugin::OnMonitorInfo(const MonitorInfo& monitor_info)
    {
        // The host already computed these once per monitor cycle (default
        // 1000 ms); no PDH / GlobalMemoryStatusEx work is needed here.
        m_cpuUsage = monitor_info.cpu_usage;
        m_memoryUsage = monitor_info.memory_usage;
        if (m_item)
            m_item->SetSystemUsage(m_cpuUsage, m_memoryUsage);
    }

    void Plugin::DataRequired()
    {
        ReloadSettingsIfChanged();
        m_manager.Snapshot(m_snapshot);

        m_presentIds.clear();
        m_onlineIds.clear();
        bool discoveredNew = false;
        for (const DeviceState& state : m_snapshot)
        {
            if (state.hardwarePresent)
                m_onlineIds.push_back(state.device.id);
            if (!state.present)
                continue;
            m_presentIds.push_back(state.device.id);
            if (m_settings.NoteDevice(state.device.id))
                discoveredNew = true;
        }
        if (discoveredNew)
            SaveSettings();

        std::vector<const DeviceState*> visible;
        for (const DeviceState& state : m_snapshot)
        {
            if (!state.present || m_settings.ForDevice(state.device.id).hidden)
                continue;
            visible.push_back(&state);
        }

        std::stable_sort(visible.begin(), visible.end(),
            [this](const DeviceState* left, const DeviceState* right)
            {
                return m_settings.ForDevice(left->device.id).priority <
                    m_settings.ForDevice(right->device.id).priority;
            });

        if (static_cast<int>(visible.size()) > m_settings.maxDevices)
            visible.resize(m_settings.maxDevices);

        std::vector<BatteryEntry> entries;
        entries.reserve(visible.size());
        m_tooltip.clear();

        for (const DeviceState* state : visible)
        {
            const DeviceSettings& settings =
                m_settings.ForDevice(state->device.id);

            BatteryEntry entry;
            entry.id = state->device.id;
            entry.name = settings.name.empty()
                ? state->device.name : settings.name;
            entry.label = settings.label.empty()
                ? state->device.defaultLabel : settings.label;
            entry.value = FormatPercent(state->reading, m_settings.showOffline);
            // A stale reading still has a valid percent and online=true so the
            // level, fill and colour render exactly as they did when live.
            entry.online = state->reading.HasPercent();
            entry.stale = entry.online && state->reading.stale;
            entry.percent = entry.online ? state->reading.percent : -1;
            entry.charging = state->reading.charge == ChargeState::Charging;
            // "auto" (or any malformed value) -> CLR_INVALID -> colour by level.
            entry.color = BatteryItem::ParseColor(settings.color, CLR_INVALID);
            entries.push_back(std::move(entry));

            if (!m_tooltip.empty())
                m_tooltip += L"\n";
            m_tooltip += entries.back().name + L" 电量: " + entries.back().value;
            if (entries.back().stale)
            {
                m_tooltip += L"（离线，最后读数";
                if (state->lastGoodMs != 0)
                {
                    unsigned long long now = GetTickCount64();
                    unsigned long long minutes = now > state->lastGoodMs
                        ? (now - state->lastGoodMs) / 60000ULL : 0;
                    m_tooltip += L" · " + std::to_wstring(minutes) + L" 分钟前";
                }
                m_tooltip += L"）";
            }
            else if (!state->reading.note.empty())
            {
                m_tooltip += L" (" + state->reading.note + L")";
            }
        }

        if (m_settings.displayMode == L"hybrid" &&
            (m_cpuUsage >= 0 || m_memoryUsage >= 0))
        {
            if (!m_tooltip.empty())
                m_tooltip += L"\n";
            m_tooltip += L"CPU: ";
            m_tooltip += m_cpuUsage >= 0
                ? std::to_wstring(m_cpuUsage) + L"%" : std::wstring(L"--");
            m_tooltip += L"  内存: ";
            m_tooltip += m_memoryUsage >= 0
                ? std::to_wstring(m_memoryUsage) + L"%" : std::wstring(L"--");
        }

        CheckLowBattery(entries);
        if (m_item)
            m_item->SetEntries(std::move(entries));
    }

    void Plugin::CheckLowBattery(const std::vector<BatteryEntry>& entries)
    {
        if (m_app == nullptr || !m_settings.lowBatteryNotify)
            return;

        std::vector<LowBatterySample> samples;
        samples.reserve(entries.size());
        for (const BatteryEntry& entry : entries)
        {
            // Only a fresh discharging reading qualifies. A cached level may
            // be hours old, and a charging battery is already being handled.
            samples.push_back({ entry.id, entry.name, entry.percent,
                entry.online && !entry.stale && !entry.charging });
        }
        for (const std::wstring& message : m_notifier.Update(samples))
            m_app->ShowNotifyMessage(message.c_str());
    }

    const wchar_t* Plugin::GetInfo(PluginInfoIndex index)
    {
        switch (index)
        {
        case TMI_NAME:
            return L"无线外设电量";
        case TMI_DESCRIPTION:
            return L"在任务栏显示无线鼠标、键盘和手柄的电量";
        case TMI_AUTHOR:
            return L"TrafficMonitorPeripheralBattery contributors";
        case TMI_COPYRIGHT:
            return L"MIT";
        case TMI_VERSION:
            return TMB_VERSION_WSTRING;
        case TMI_URL:
            return TMB_REPOSITORY_URL;
        default:
            return L"";
        }
    }

    const wchar_t* Plugin::GetTooltipInfo()
    {
        return m_tooltip.c_str();
    }

    void Plugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
    {
        if (data == nullptr)
            return;

        switch (index)
        {
        case EI_TASKBAR_WND_NOT_SHOW_PERCENT:
            m_hidePercentTaskbar = wcscmp(data, L"0") != 0;
            ApplyDisplayOptions();
            break;
        case EI_MAIN_WND_NOT_SHOW_PERCENT:
            m_hidePercentMain = wcscmp(data, L"0") != 0;
            ApplyDisplayOptions();
            break;
        case EI_DRAW_TASKBAR_WND:
            m_drawTaskbar = wcscmp(data, L"1") == 0;
            if (!m_drawTaskbar)
                m_labelColor = CLR_INVALID; // main window uses the value colour
            ApplyDisplayOptions();
            ApplyColorRules();
            break;
        case EI_LABEL_TEXT_COLOR:
            m_labelColor = static_cast<COLORREF>(wcstoul(data, nullptr, 10));
            ApplyDisplayOptions();
            ApplyColorRules();
            break;
        case EI_VALUE_TEXT_COLOR:
            m_valueColor = static_cast<COLORREF>(wcstoul(data, nullptr, 10));
            ApplyDisplayOptions();
            ApplyColorRules();  // neutral tones follow the host's text colour
            break;
        default:
            break;
        }
    }

    ITMPlugin::OptionReturn Plugin::ShowOptionsDialog(void* hParent)
    {
        HWND parent = reinterpret_cast<HWND>(hParent);

        auto presentProvider = [this]() -> std::vector<std::wstring>
        {
            return m_onlineIds;
        };
        auto rescan = [this]()
        {
            m_manager.RequestRefresh();
        };

        if (!OptionsDialog::Show(parent, m_settings, m_registry,
                presentProvider, rescan))
        {
            return OR_OPTION_UNCHANGED;
        }

        SaveSettings();
        ApplySettings();
        m_manager.RequestRefresh();
        return OR_OPTION_CHANGED;
    }

    int Plugin::GetCommandCount()
    {
        return 3;
    }

    const wchar_t* Plugin::GetCommandName(int command_index)
    {
        if (command_index == 0)
            return L"立即刷新";
        if (command_index == 1)
            return L"打开设置文件";
        if (command_index == 2)
            return L"诊断信息";
        return L"";
    }

    void Plugin::OnPluginCommand(int command_index, void* hWnd, void*)
    {
        if (command_index == 0)
            m_manager.RequestRefresh();
        else if (command_index == 1)
            OpenSettingsFile(hWnd);
        else if (command_index == 2)
            ShowDiagnostics(hWnd);
    }

    void Plugin::ShowDiagnostics(void* hWnd)
    {
        // A single message box answers the questions that cost the most time
        // in practice: which DLL the host actually loaded, which INI it reads,
        // and why a particular device is (not) reporting a level. Windows
        // message boxes support Ctrl+C, so the whole report can be pasted.
        std::vector<DeviceState> snapshot;
        m_manager.Snapshot(snapshot);

        std::wstring text = L"无线外设电量 · 诊断信息\n";
        text += L"已加载版本: " + std::wstring(TMB_VERSION_WSTRING) + L"\n";
        // GetModuleDirectory() defaults to the host EXE's directory. Ask the
        // loader for this DLL's own module instead, so the reported path is the
        // file the host really loaded (the usual "old DLL still running" trap).
        static int moduleAnchor = 0;
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&moduleAnchor), &module))
        {
            std::vector<wchar_t> path(1024, L'\0');
            DWORD length = GetModuleFileNameW(module, path.data(),
                static_cast<DWORD>(path.size()));
            text += L"模块路径: " + std::wstring(path.data(), length) + L"\n";
        }
        else
        {
            text += L"模块路径: (无法获取)\n";
        }
        text += L"配置文件: " + m_configPath + L"\n";
        text += L"配置文件存在: " +
            std::wstring(FileExists(m_configPath) ? L"是" : L"否") + L"\n";
        text += L"显示模式: " + m_settings.displayMode + L"\n";
        text += L"刷新间隔: " +
            std::to_wstring(m_settings.refreshIntervalSeconds) + L" 秒\n";
        text += L"离线保留最后读数: " +
            std::wstring(m_settings.keepLastKnown ? L"开" : L"关") + L"\n";
        text += L"低电量提醒: " +
            std::wstring(m_settings.lowBatteryNotify ? L"开" : L"关") +
            L"，阈值 " + std::to_wstring(m_settings.lowBatteryNotifyPercent) +
            L"%\n";
        text += L"\n设备（在线=接收器在；缓存=显示最后一次读数）:\n";

        const unsigned long long now = GetTickCount64();
        int shown = 0;
        int skipped = 0;
        for (const DeviceState& state : snapshot)
        {
            const bool known = state.present ||
                m_settings.IsPersisted(state.device.id) ||
                state.lastGoodMs != 0;
            if (!known)
                continue;
            if (shown >= 20)
            {
                ++skipped;
                continue;
            }
            ++shown;

            std::wstring status = state.hardwarePresent ? L"在线" :
                (state.reading.stale ? L"休眠" : L"离线");
            std::wstring level = state.reading.HasPercent()
                ? std::to_wstring(state.reading.percent) + L"%" : L"--";
            if (state.reading.stale)
                level += L"(缓存)";
            std::wstring age = state.lastGoodMs != 0
                ? std::to_wstring((now > state.lastGoodMs
                    ? now - state.lastGoodMs : 0) / 60000ULL) + L" 分钟前"
                : L"无";

            text += L"[" + status + L"] " + state.device.id + L"  " + level +
                L"  present=" + std::to_wstring(state.present ? 1 : 0) +
                L"  最后读数=" + age +
                L"  连续失败=" + std::to_wstring(state.failedPolls) + L"\n";
        }
        if (skipped > 0)
            text += L"… 其余 " + std::to_wstring(skipped) + L" 台省略\n";
        if (shown == 0)
            text += L"（尚未发现任何设备）\n";
        text += L"\n按 Ctrl+C 可复制以上信息。";

        MessageBoxW(reinterpret_cast<HWND>(hWnd), text.c_str(),
            L"无线外设电量 · 诊断信息", MB_OK | MB_ICONINFORMATION);
    }

    void Plugin::OpenSettingsFile(void* hWnd)
    {
        SaveSettings();
        ShellExecuteW(reinterpret_cast<HWND>(hWnd), L"open",
            m_configPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        MessageBoxW(reinterpret_cast<HWND>(hWnd),
            L"设置文件已打开。保存后会自动生效，无需重启 TrafficMonitor。",
            L"无线外设电量", MB_OK | MB_ICONINFORMATION);
    }

    std::wstring Plugin::ConfigPath() const
    {
        if (m_app != nullptr)
        {
            const wchar_t* directory = m_app->GetPluginConfigDir();
            if (directory != nullptr && directory[0] != L'\0')
                return JoinPath(directory, L"PeripheralBatteryPlugin.ini");
        }
        return JoinPath(GetModuleDirectory(), L"PeripheralBatteryPlugin.ini");
    }

    void Plugin::Stop()
    {
        m_manager.Stop();
    }
}
