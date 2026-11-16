#pragma once

#include "IProtocol.h"

#include <vector>

namespace pb
{
    /**
     * Compx 17-byte vendor frame used by ATK / VXE / VGN mice and their NANO
     * receivers. Receivers often expose two FF02/0002 collections; only the
     * 17-byte one accepts this frame, so DeviceDescriptor::reportLength pins
     * the collection.
     */
    class Compx17Protocol : public IProtocol
    {
    public:
        const wchar_t* Id() const override { return L"compx_17"; }
        BatteryReading Read(const DeviceDescriptor& device) const override;

        static std::vector<unsigned char> BuildRequest(unsigned short outputLength);
        static bool ParseResponse(const std::vector<unsigned char>& response,
            BatteryReading& reading);
    };
}
