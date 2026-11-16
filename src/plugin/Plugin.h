#pragma once

#include "PluginInterface.h"

#include "../core/DeviceManager.h"
#include "../core/DeviceRegistry.h"
#include "BatteryGroupItem.h"
#include "Settings.h"

#include <memory>
#include <string>
#include <vector>

namespace pb
{
    class Plugin : public ITMPlugin
    {
    public:
        Plugin() = default;
        ~Plugin();

        IPluginItem* GetItem(int index) override;
        void DataRequired() override;
        void OnMonitorInfo(const MonitorInfo& monitor_info) override;
        const wchar_t* GetInfo(PluginInfoIndex index) override;
        void OnInitialize(ITrafficMonitor* pApp) override;
        const wchar_t* GetTooltipInfo() override;
        OptionReturn ShowOptionsDialog(void* hParent) override;
        void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

        int GetCommandCount() override;
        const wchar_t* GetCommandName(int command_index) override;
        void OnPluginCommand(int command_index, void* hWnd, void* para) override;

        void Stop();

    private:
        void ApplySettings();
        void ApplyDisplayOptions();
        void ApplyColorRules();
        void MigrateLegacyColors();
        void CheckLowBattery(const std::vector<BatteryEntry>& entries);
        void OpenSettingsFile(void* hWnd);
        void ShowDiagnostics(void* hWnd);
        void SaveSettings();
        void ReloadSettingsIfChanged();
        void ShowMigrationNoticeOnce();
        std::wstring ConfigPath() const;

        DeviceRegistry m_registry;
        Settings m_settings;
        DeviceManager m_manager;
        LowBatteryNotifier m_notifier;

        // Exactly one item for the whole process lifetime. TrafficMonitor
        // caches this pointer at startup and never re-enumerates GetItem().
        std::unique_ptr<BatteryGroupItem> m_item;

        std::vector<DeviceState> m_snapshot;
        std::vector<std::wstring> m_presentIds;   // visible: hardware or grace
        std::vector<std::wstring> m_onlineIds;    // receiver truly attached
        std::wstring m_configPath;
        std::wstring m_tooltip;
        int m_cpuUsage = -1;
        int m_memoryUsage = -1;
        ITrafficMonitor* m_app = nullptr;
        unsigned long long m_configStamp = 0;
        unsigned long long m_lastStampCheck = 0;
        bool m_hidePercentTaskbar = false;
        bool m_hidePercentMain = false;
        bool m_drawTaskbar = true;
        COLORREF m_labelColor = CLR_INVALID;
        COLORREF m_valueColor = CLR_INVALID;
    };
}
