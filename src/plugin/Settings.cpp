#include "Settings.h"

#include "../util/PathUtil.h"

#include <windows.h>

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

        const wchar_t* ColorForIndex(int index)
        {
            static const wchar_t* palette[] = {
                L"#4FC3F7", L"#81C784", L"#FFB74D",
                L"#E57373", L"#BA68C8", L"#64B5F6"
            };
            return palette[index % (sizeof(palette) / sizeof(palette[0]))];
        }

        DeviceSettings DefaultFor(const DeviceDescriptor& device, int index)
        {
            DeviceSettings settings;
            settings.enabled = device.enabledByDefault;
            settings.name = device.name;
            settings.label = device.defaultLabel;
            settings.priority = device.priority;
            settings.color = ColorForIndex(index);
            return settings;
        }

        void EnsureDefaults(std::map<std::wstring, DeviceSettings>& devices,
            const DeviceRegistry& registry)
        {
            int index = 0;
            for (const DeviceDescriptor& device : registry.All())
            {
                if (devices.find(device.id) == devices.end())
                    devices[device.id] = DefaultFor(device, index);
                ++index;
            }
        }
    }

    void Settings::Load(const std::wstring& path, const DeviceRegistry& registry)
    {
        EnsureDefaults(devices, registry);

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

        if (refreshIntervalSeconds < 5)
            refreshIntervalSeconds = 5;
        if (refreshIntervalSeconds > 3600)
            refreshIntervalSeconds = 3600;
        if (maxDevices < 1)
            maxDevices = 1;
        if (maxDevices > 8)
            maxDevices = 8;

        for (const DeviceDescriptor& device : registry.All())
        {
            std::wstring section = L"device:" + device.id;
            DeviceSettings& settings = devices[device.id];
            settings.enabled = ReadBool(path, section.c_str(), L"enabled", settings.enabled);
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

        // Rewrite the whole file. WritePrivateProfileStringW appends a new
        // section when an existing INI was saved as UTF-8 with BOM, which
        // produces duplicated [plugin]/[device:...] blocks after editing.
        DeleteFileW(path.c_str());

        WriteInt(path, L"plugin", L"refresh_interval_seconds", refreshIntervalSeconds);
        WriteInt(path, L"plugin", L"max_devices", maxDevices);
        WriteString(path, L"plugin", L"priority_mode", priorityMode);
        WriteBool(path, L"plugin", L"show_offline", showOffline);
        WriteString(path, L"plugin", L"display_mode", displayMode);

        int index = 0;
        for (const DeviceDescriptor& device : registry.All())
        {
            std::wstring section = L"device:" + device.id;
            const DeviceSettings& settings = ForDevice(device.id);
            DeviceSettings fallback = DefaultFor(device, index);
            const DeviceSettings& effective = settings.name.empty() ? fallback : settings;

            WriteBool(path, section.c_str(), L"enabled", effective.enabled);
            WriteString(path, section.c_str(), L"name", effective.name);
            WriteString(path, section.c_str(), L"label", effective.label);
            WriteInt(path, section.c_str(), L"priority", effective.priority);
            WriteString(path, section.c_str(), L"color", effective.color);
            ++index;
        }
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
