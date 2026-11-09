#pragma once

#include "../core/DeviceRegistry.h"

#include <map>
#include <string>

namespace pb
{
    struct DeviceSettings
    {
        bool enabled = true;
        std::wstring name;
        std::wstring label;
        int priority = 100;
        std::wstring color = L"#FFFFFF";
    };

    struct Settings
    {
        int refreshIntervalSeconds = 30;
        int maxDevices = 4;
        std::wstring priorityMode = L"fixed";   // fixed / lowest / charging
        bool showOffline = true;
        std::wstring displayMode = L"bar";      // bar / text

        std::map<std::wstring, DeviceSettings> devices;

        void Load(const std::wstring& path, const DeviceRegistry& registry);
        void Save(const std::wstring& path, const DeviceRegistry& registry) const;

        const DeviceSettings& ForDevice(const std::wstring& id) const;
    };
}
