#include "Settings.h"

#include "../util/PathUtil.h"

#include <windows.h>

#include <algorithm>
#include <vector>

namespace pb
{
    namespace
    {
        std::wstring ReadString(const std::wstring& path, const wchar_t* section,
            const wchar_t* key, const wchar_t* defaultValue)
        {
            wchar_t buffer[512] = {};
            GetPrivateProfileStringW(section, key, defaultValue,
                buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
            return buffer;
        }

        int ReadInt(const std::wstring& path, const wchar_t* section,
            const wchar_t* key, int defaultValue)
        {
            return static_cast<int>(GetPrivateProfileIntW(section, key,
                defaultValue, path.c_str()));
        }

        bool ReadBool(const std::wstring& path, const wchar_t* section,
            const wchar_t* key, bool defaultValue)
        {
            std::wstring value = ReadString(path, section, key,
                defaultValue ? L"true" : L"false");
            return value == L"1" || value == L"true" || value == L"yes" ||
                value == L"True" || value == L"YES";
        }

        void WriteString(const std::wstring& path, const wchar_t* section,
            const wchar_t* key, const std::wstring& value)
        {
            WritePrivateProfileStringW(section, key, value.c_str(), path.c_str());
        }

        void WriteInt(const std::wstring& path, const wchar_t* section,
            const wchar_t* key, int value)
        {
            WritePrivateProfileStringW(section, key,
                std::to_wstring(value).c_str(), path.c_str());
        }

        void WriteBool(const std::wstring& path, const wchar_t* section,
            const wchar_t* key, bool value)
        {
            WritePrivateProfileStringW(section, key,
                value ? L"true" : L"false", path.c_str());
        }

        DeviceSettings DefaultFor(const DeviceDescriptor& device)
        {
            DeviceSettings settings;
            settings.hidden = !device.enabledByDefault;
            settings.name = device.name;
            settings.label = device.defaultLabel;
            settings.priority = device.priority;
            settings.color = L"auto";
            return settings;
        }

        void EnsureDefaults(std::map<std::wstring, DeviceSettings>& devices,
            const DeviceRegistry& registry)
        {
            for (const DeviceDescriptor& device : registry.All())
            {
                if (devices.find(device.id) == devices.end())
                    devices[device.id] = DefaultFor(device);
            }
        }

        /** Sections returned by GetPrivateProfileSectionNamesW are NUL separated. */
        void CollectPersistedSections(const std::wstring& path,
            std::set<std::wstring>& out)
        {
            std::vector<wchar_t> buffer(8192, L'\0');
            DWORD length = GetPrivateProfileSectionNamesW(buffer.data(),
                static_cast<DWORD>(buffer.size()), path.c_str());
            if (length == 0 || length >= buffer.size() - 2)
                return;

            const wchar_t prefix[] = L"device:";
            const size_t prefixLength = wcslen(prefix);
            for (DWORD i = 0; i < length;)
            {
                const wchar_t* section = buffer.data() + i;
                size_t size = wcslen(section);
                if (size == 0)
                    break;
                if (wcsncmp(section, prefix, prefixLength) == 0 && size > prefixLength)
                    out.insert(std::wstring(section + prefixLength));
                i += static_cast<DWORD>(size + 1);
            }
        }
    }

    void LowBatteryNotifier::Configure(bool enabled, int percent)
    {
        m_enabled = enabled;
        m_percent = std::max(1, std::min(100, percent));
        if (!enabled)
            m_notified.clear();
    }

    void LowBatteryNotifier::Reset()
    {
        m_notified.clear();
    }

    std::vector<std::wstring> LowBatteryNotifier::Update(
        const std::vector<LowBatterySample>& samples)
    {
        std::vector<std::wstring> messages;
        if (!m_enabled)
            return messages;

        for (const LowBatterySample& sample : samples)
        {
            bool& notified = m_notified[sample.id];
            // A cached level may be hours old: never raise an alert for it.
            if (!sample.live || sample.percent < 0)
                continue;

            if (sample.percent <= m_percent)
            {
                if (!notified)
                {
                    notified = true;
                    messages.push_back(sample.name + L" 电量仅 " +
                        std::to_wstring(sample.percent) + L"%，请及时充电。");
                }
            }
            else if (sample.percent >= m_percent + 5)
            {
                notified = false;   // hysteresis before the next alert
            }
        }
        return messages;
    }

    void Settings::ClampValues()
    {
        refreshIntervalSeconds = std::max(5,
            std::min(3600, refreshIntervalSeconds));
        maxDevices = std::max(1, std::min(8, maxDevices));
        presenceGraceSeconds = std::max(0,
            std::min(3600, presenceGraceSeconds));
        lowBatteryNotifyPercent = std::max(1,
            std::min(100, lowBatteryNotifyPercent));
        hybridRows = std::max(2, std::min(4, hybridRows));
        rowBarWidth = std::max(16, std::min(160, rowBarWidth));
        if (rowStyle != L"solid" && rowStyle != L"framed")
            rowStyle = L"solid";
        batteryLowPercent = std::max(1, std::min(98, batteryLowPercent));
        batteryGoodPercent = std::max(batteryLowPercent + 1,
            std::min(100, batteryGoodPercent));
        cpuOrangePercent = std::max(1, std::min(98, cpuOrangePercent));
        cpuRedPercent = std::max(cpuOrangePercent + 1,
            std::min(100, cpuRedPercent));
        if (displayMode != L"bar" && displayMode != L"text" &&
            displayMode != L"hybrid" && displayMode != L"stacked")
            displayMode = L"bar";
    }

    void Settings::Load(const std::wstring& path, const DeviceRegistry& registry)
    {
        EnsureDefaults(devices, registry);
        persistedDevices.clear();

        if (path.empty() || !FileExists(path))
            return;

        refreshIntervalSeconds = ReadInt(path, L"plugin",
            L"refresh_interval_seconds", refreshIntervalSeconds);
        maxDevices = ReadInt(path, L"plugin", L"max_devices", maxDevices);
        priorityMode = ReadString(path, L"plugin", L"priority_mode",
            priorityMode.c_str());
        showOffline = ReadBool(path, L"plugin", L"show_offline", showOffline);
        displayMode = ReadString(path, L"plugin", L"display_mode",
            displayMode.c_str());
        presenceGraceSeconds = ReadInt(path, L"plugin",
            L"presence_grace_seconds", presenceGraceSeconds);
        keepLastKnown = ReadBool(path, L"plugin",
            L"keep_last_known", keepLastKnown);
        lowBatteryNotify = ReadBool(path, L"plugin",
            L"low_battery_notify", lowBatteryNotify);
        lowBatteryNotifyPercent = ReadInt(path, L"plugin",
            L"low_battery_notify_percent", lowBatteryNotifyPercent);
        singleItemNoticeShown = ReadBool(path, L"plugin",
            L"single_item_notice_shown", singleItemNoticeShown);
        colorAutoMigrationDone = ReadBool(path, L"plugin",
            L"color_auto_migrated", colorAutoMigrationDone);

        hybridRows = ReadInt(path, L"plugin", L"hybrid_rows", hybridRows);
        rowBarWidth = ReadInt(path, L"plugin", L"row_bar_width", rowBarWidth);
        rowStyle = ReadString(path, L"plugin", L"row_style", rowStyle.c_str());
        stackedShowPercent = ReadBool(path, L"plugin",
            L"stacked_show_percent", stackedShowPercent);
        batteryLowPercent = ReadInt(path, L"plugin",
            L"battery_low_percent", batteryLowPercent);
        batteryGoodPercent = ReadInt(path, L"plugin",
            L"battery_good_percent", batteryGoodPercent);
        cpuOrangePercent = ReadInt(path, L"plugin",
            L"cpu_orange_percent", cpuOrangePercent);
        cpuRedPercent = ReadInt(path, L"plugin",
            L"cpu_red_percent", cpuRedPercent);
        colorHigh = ReadString(path, L"plugin", L"color_high", colorHigh.c_str());
        colorMedium = ReadString(path, L"plugin", L"color_medium", colorMedium.c_str());
        colorLow = ReadString(path, L"plugin", L"color_low", colorLow.c_str());
        colorCharging = ReadString(path, L"plugin",
            L"color_charging", colorCharging.c_str());

        ClampValues();

        CollectPersistedSections(path, persistedDevices);

        for (const DeviceDescriptor& device : registry.All())
        {
            std::wstring section = L"device:" + device.id;
            DeviceSettings& settings = devices[device.id];

            // Old INIs used enabled=false. Migrate it to hidden=true once.
            bool legacyEnabled = ReadBool(path, section.c_str(),
                L"enabled", true);
            settings.hidden = ReadBool(path, section.c_str(),
                L"hidden", !legacyEnabled);
            settings.name = ReadString(path, section.c_str(), L"name", settings.name.c_str());
            settings.label = ReadString(path, section.c_str(), L"label", settings.label.c_str());
            settings.priority = ReadInt(path, section.c_str(), L"priority", settings.priority);
            settings.color = ReadString(path, section.c_str(), L"color", settings.color.c_str());
        }
    }

    void Settings::Save(const std::wstring& path, const DeviceRegistry& registry) const
    {
        if (path.empty())
            return;

        // Rewrite into a temporary file and replace the target atomically.
        // WritePrivateProfileStringW appends a new section when an existing INI
        // was saved as UTF-8 with BOM, which produces duplicated
        // [plugin]/[device:...] blocks after editing.
        std::wstring temp = path + L".tmp";
        DeleteFileW(temp.c_str());

        WriteInt(temp, L"plugin", L"refresh_interval_seconds", refreshIntervalSeconds);
        WriteInt(temp, L"plugin", L"max_devices", maxDevices);
        WriteString(temp, L"plugin", L"priority_mode", priorityMode);
        WriteBool(temp, L"plugin", L"show_offline", showOffline);
        WriteString(temp, L"plugin", L"display_mode", displayMode);
        WriteInt(temp, L"plugin", L"presence_grace_seconds", presenceGraceSeconds);
        WriteBool(temp, L"plugin", L"keep_last_known", keepLastKnown);
        WriteBool(temp, L"plugin", L"low_battery_notify", lowBatteryNotify);
        WriteInt(temp, L"plugin", L"low_battery_notify_percent",
            lowBatteryNotifyPercent);
        WriteBool(temp, L"plugin", L"single_item_notice_shown", singleItemNoticeShown);
        WriteBool(temp, L"plugin", L"color_auto_migrated", colorAutoMigrationDone);

        WriteInt(temp, L"plugin", L"hybrid_rows", hybridRows);
        WriteInt(temp, L"plugin", L"row_bar_width", rowBarWidth);
        WriteString(temp, L"plugin", L"row_style", rowStyle);
        WriteBool(temp, L"plugin", L"stacked_show_percent", stackedShowPercent);
        WriteInt(temp, L"plugin", L"battery_low_percent", batteryLowPercent);
        WriteInt(temp, L"plugin", L"battery_good_percent", batteryGoodPercent);
        WriteInt(temp, L"plugin", L"cpu_orange_percent", cpuOrangePercent);
        WriteInt(temp, L"plugin", L"cpu_red_percent", cpuRedPercent);
        WriteString(temp, L"plugin", L"color_high", colorHigh);
        WriteString(temp, L"plugin", L"color_medium", colorMedium);
        WriteString(temp, L"plugin", L"color_low", colorLow);
        WriteString(temp, L"plugin", L"color_charging", colorCharging);

        for (const DeviceDescriptor& device : registry.All())
        {
            // Only devices that were seen or explicitly configured are persisted.
            if (persistedDevices.find(device.id) != persistedDevices.end())
            {
                std::wstring section = L"device:" + device.id;
                const DeviceSettings& settings = ForDevice(device.id);
                DeviceSettings fallback = DefaultFor(device);
                const DeviceSettings& effective =
                    settings.name.empty() ? fallback : settings;

                WriteBool(temp, section.c_str(), L"hidden", effective.hidden);
                WriteString(temp, section.c_str(), L"name", effective.name);
                WriteString(temp, section.c_str(), L"label", effective.label);
                WriteInt(temp, section.c_str(), L"priority", effective.priority);
                WriteString(temp, section.c_str(), L"color", effective.color);
            }
        }

        if (!MoveFileExW(temp.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(temp.c_str());
        }
    }

    bool Settings::NoteDevice(const std::wstring& id)
    {
        return persistedDevices.insert(id).second;
    }

    bool Settings::IsPersisted(const std::wstring& id) const
    {
        return persistedDevices.find(id) != persistedDevices.end();
    }

    std::vector<std::wstring> Settings::KnownDeviceIds(
        const DeviceRegistry& registry) const
    {
        std::vector<std::wstring> ids;
        for (const DeviceDescriptor& device : registry.All())
        {
            if (IsPersisted(device.id))
                ids.push_back(device.id);
        }
        return ids;
    }

    const DeviceSettings& Settings::ForDevice(const std::wstring& id) const
    {
        auto iter = devices.find(id);
        if (iter != devices.end())
            return iter->second;

        static DeviceSettings fallback;
        return fallback;
    }
}
