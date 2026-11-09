#pragma once

#include "IProtocol.h"

#include <vector>

namespace pb
{
    /**
     * AUSDOM / Juxing mouse dongle.
     *
     * Output report 11, request starts with 0x13.
     * Reply starts with 0A 13 xx 00; battery is byte 18, charging flag byte 17.
     */
    class JuxingProtocol : public IProtocol
    {
    public:
        const wchar_t* Id() const override { return L"juxing"; }
        BatteryReading Read(const DeviceDescriptor& device) const override;

        static std::vector<unsigned char> BuildRequest(unsigned short outputLength);
        static bool ParseResponse(const std::vector<unsigned char>& response, BatteryReading& reading);
    };
}
