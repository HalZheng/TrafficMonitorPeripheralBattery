#pragma once

#include "PluginInterface.h"

#include "../core/DeviceManager.h"
#include "../core/DeviceRegistry.h"
#include "BatteryItem.h"
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
        void BuildItems();
        void ApplyDisplayOptions();
        void OpenSettingsFile(void* hWnd);
        std::wstring ConfigPath() const;

        DeviceRegistry m_registry;
        Settings m_settings;
        DeviceManager m_manager;
        std::vector<std::unique_ptr<BatteryItem>> m_items;
        std::vector<DeviceState> m_snapshot;
        std::wstring m_configPath;
        std::wstring m_tooltip;
        ITrafficMonitor* m_app = nullptr;
        bool m_hidePercentTaskbar = false;
        bool m_hidePercentMain = false;
        bool m_drawTaskbar = true;
        COLORREF m_labelColor = CLR_INVALID;
        COLORREF m_valueColor = CLR_INVALID;
    };
}
