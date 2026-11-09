#include "Plugin.h"

#include "OptionsDialog.h"

#include "../util/PathUtil.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>

namespace pb
{
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
        // Persist defaults for newly added devices so users can discover and
        // edit every supported item in the generated INI.
        m_settings.Save(m_configPath, m_registry);

        BuildItems();
        m_manager.Start();
    }

    void Plugin::BuildItems()
    {
        m_items.clear();

        std::vector<DeviceDescriptor> active;
        for (const DeviceDescriptor& device : m_registry.All())
        {
            const DeviceSettings& deviceSettings = m_settings.ForDevice(device.id);
            if (!deviceSettings.enabled)
                continue;

            DeviceDescriptor selected = device;
            if (!deviceSettings.name.empty())
                selected.name = deviceSettings.name;
            if (!deviceSettings.label.empty())
                selected.defaultLabel = deviceSettings.label;
            selected.priority = deviceSettings.priority;
            active.push_back(std::move(selected));
        }

        std::stable_sort(active.begin(), active.end(),
            [](const DeviceDescriptor& left, const DeviceDescriptor& right)
            {
                return left.priority < right.priority;
            });

        if (static_cast<int>(active.size()) > m_settings.maxDevices)
            active.resize(m_settings.maxDevices);

        for (const DeviceDescriptor& device : active)
        {
            const DeviceSettings& deviceSettings = m_settings.ForDevice(device.id);
            std::wstring itemName = device.name + L" 电量";
            std::wstring label = deviceSettings.label.empty()
                ? device.defaultLabel : deviceSettings.label;
            bool customDraw = _wcsicmp(m_settings.displayMode.c_str(), L"bar") == 0;
            COLORREF color = BatteryItem::ParseColor(deviceSettings.color,
                RGB(79, 195, 247));
            m_items.push_back(std::unique_ptr<BatteryItem>(
                new BatteryItem(device, itemName, label, customDraw, color)));
        }

        m_manager.Configure(active, m_settings.refreshIntervalSeconds);
    }

    IPluginItem* Plugin::GetItem(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_items.size()))
            return nullptr;
        return m_items[index].get();
    }

    void Plugin::DataRequired()
    {
        m_manager.Snapshot(m_snapshot);
        m_tooltip.clear();

        for (const std::unique_ptr<BatteryItem>& item : m_items)
        {
            std::wstring value = m_settings.showOffline ? L"--" : L"";
            std::wstring tooltipLine = item->GetItemName();
            BatteryReading reading;

            for (const DeviceState& state : m_snapshot)
            {
                if (state.device.id != item->DeviceId())
                    continue;

                reading = state.reading;
                value = FormatPercent(reading, m_settings.showOffline);
                tooltipLine += L": " + value;
                if (!reading.note.empty())
                    tooltipLine += L" (" + reading.note + L")";
                break;
            }

            item->SetReading(reading, value);
            if (!m_tooltip.empty())
                m_tooltip += L"\n";
            m_tooltip += tooltipLine;
        }
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
            return L"1.2.0";
        case TMI_URL:
            return L"";
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
            break;
        case EI_LABEL_TEXT_COLOR:
            m_labelColor = static_cast<COLORREF>(wcstoul(data, nullptr, 10));
            ApplyDisplayOptions();
            break;
        case EI_VALUE_TEXT_COLOR:
            m_valueColor = static_cast<COLORREF>(wcstoul(data, nullptr, 10));
            ApplyDisplayOptions();
            break;
        default:
            break;
        }
    }

    void Plugin::ApplyDisplayOptions()
    {
        bool hidePercent = m_drawTaskbar
            ? m_hidePercentTaskbar : m_hidePercentMain;
        for (const std::unique_ptr<BatteryItem>& item : m_items)
            item->SetDisplayOptions(hidePercent, m_labelColor, m_valueColor);
    }

    ITMPlugin::OptionReturn Plugin::ShowOptionsDialog(void* hParent)
    {
        HWND parent = reinterpret_cast<HWND>(hParent);
        if (!OptionsDialog::Show(parent, m_settings, m_registry))
            return OR_OPTION_UNCHANGED;

        m_settings.Save(m_configPath, m_registry);
        MessageBoxW(parent,
            L"设置已保存。重启 TrafficMonitor 后全部生效。",
            L"无线外设电量", MB_OK | MB_ICONINFORMATION);
        return OR_OPTION_CHANGED;
    }

    int Plugin::GetCommandCount()
    {
        return 2;
    }

    const wchar_t* Plugin::GetCommandName(int command_index)
    {
        if (command_index == 0)
            return L"立即刷新";
        if (command_index == 1)
            return L"打开设置文件";
        return L"";
    }

    void Plugin::OnPluginCommand(int command_index, void* hWnd, void*)
    {
        if (command_index == 0)
            m_manager.RequestRefresh();
        else if (command_index == 1)
            OpenSettingsFile(hWnd);
    }

    void Plugin::OpenSettingsFile(void* hWnd)
    {
        m_settings.Save(m_configPath, m_registry);
        ShellExecuteW(reinterpret_cast<HWND>(hWnd), L"open",
            m_configPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        MessageBoxW(reinterpret_cast<HWND>(hWnd),
            L"设置文件已打开。修改保存后，请重启 TrafficMonitor 使设置生效。",
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
