#pragma once

#include <cstdio>
#include <string>

namespace pb
{
    enum class ChargeState
    {
        Unknown,
        Discharging,
        Charging,
        Full
    };

    struct BatteryReading
    {
        bool found = false;
        bool online = false;
        int percent = -1;
        ChargeState charge = ChargeState::Unknown;
        int voltageMv = -1;
        std::wstring protocol;
        std::wstring note;

        bool HasPercent() const
        {
            return online && percent >= 0 && percent <= 100;
        }
    };

    inline std::wstring FormatPercent(const BatteryReading& reading, bool showOffline)
    {
        if (!reading.HasPercent())
            return showOffline ? L"--" : L"";

        wchar_t buffer[32] = {};
        swprintf_s(buffer, L"%d%%", reading.percent);
        std::wstring value = buffer;
        if (reading.charge == ChargeState::Charging)
            value += L"+";
        return value;
    }
}
