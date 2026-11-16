#pragma once

#include "../core/DeviceRegistry.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace pb
{
    /** One device's input to the low-battery alert state machine. */
    struct LowBatterySample
    {
        std::wstring id;
        std::wstring name;
        int percent = -1;
        bool live = false;   // fresh, discharging reading (never a cached one)
    };

    /**
     * Fires one notification per device per low-battery episode. The level has
     * to climb back to threshold + 5 before it can alert again, so a battery
     * hovering around the threshold cannot spam the taskbar.
     */
    class LowBatteryNotifier
    {
    public:
        void Configure(bool enabled, int percent);
        void Reset();
        std::vector<std::wstring> Update(
            const std::vector<LowBatterySample>& samples);

    private:
        bool m_enabled = true;
        int m_percent = 10;
        std::map<std::wstring, bool> m_notified;
    };

    struct DeviceSettings
    {
        // "hidden" replaces the old "enabled" flag. Whether a device exists is
        // decided by hardware discovery; settings only decide whether a found
        // device is allowed on the taskbar.
        bool hidden = false;
        std::wstring name;
        std::wstring label;
        int priority = 100;
        // "auto" colours the battery by its charge level; #RRGGBB overrides it.
        std::wstring color = L"auto";
    };

    struct Settings
    {
        int refreshIntervalSeconds = 30;
        int maxDevices = 6;
        std::wstring priorityMode = L"fixed";   // fixed / lowest / charging
        bool showOffline = true;
        std::wstring displayMode = L"bar";      // bar / text / hybrid / stacked
        int presenceGraceSeconds = 300;         // grace for never-seen devices
        // Keep showing the last measured level (with an offline badge) after a
        // device sleeps, until it answers again. false restores the old "--".
        bool keepLastKnown = true;
        // One taskbar notification when a live, discharging reading drops to
        // this level. 0/false disables alerts; stale readings never alert.
        bool lowBatteryNotify = true;
        int lowBatteryNotifyPercent = 10;
        bool singleItemNoticeShown = false;     // one-time upgrade hint
        bool colorAutoMigrationDone = false;   // one-time legacy palette -> auto

        // Hybrid layout: two system rows plus up to two battery rows.
        int hybridRows = 4;
        int rowBarWidth = 48;                   // row bar width at 96 DPI
        // stacked hides percentages by default; showing them widens the whole
        // grid (each cell reserves a value column) and keeps every edge aligned.
        bool stackedShowPercent = false;
        // solid: no outline/inset/padding, fills the whole row height.
        // framed: old 1 px stroke + 2 px inset, kept as a rollback option.
        std::wstring rowStyle = L"solid";

        // Mobile-device battery convention: >= good green, <= low red,
        // otherwise orange; charging is a distinct colour; offline is grey.
        int batteryLowPercent = 20;
        int batteryGoodPercent = 60;
        int cpuOrangePercent = 50;
        int cpuRedPercent = 80;
        std::wstring colorHigh = L"#4CAF50";
        std::wstring colorMedium = L"#FF9800";
        std::wstring colorLow = L"#F44336";
        std::wstring colorCharging = L"#03A9F4";

        std::map<std::wstring, DeviceSettings> devices;

        // Only these devices get a [device:...] section on save. The registry
        // may contain hundreds of models; writing all of them would bury the
        // devices the user actually owns.
        std::set<std::wstring> persistedDevices;

        void Load(const std::wstring& path, const DeviceRegistry& registry);
        void Save(const std::wstring& path, const DeviceRegistry& registry) const;

        /** Single source of range limits, shared by INI load and dialog edits. */
        void ClampValues();

        /** Record a discovered device. Returns true when it is new. */
        bool NoteDevice(const std::wstring& id);
        bool IsPersisted(const std::wstring& id) const;
        std::vector<std::wstring> KnownDeviceIds(const DeviceRegistry& registry) const;

        const DeviceSettings& ForDevice(const std::wstring& id) const;
    };
}
