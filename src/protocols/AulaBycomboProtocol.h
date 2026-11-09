#pragma once

#include "IProtocol.h"

#include <vector>

namespace pb
{
    /**
     * Compx / BK3632 "BYCOMBO" 2.4G keyboard receiver.
     *
     * The iLovBee B87 answers report 0x13 / command 0x05 with
     * 13 05 01 00 0A ... <battery at byte 10>.
     *
     * The parser also accepts the 13 4A and 13 0A frames used by other
     * BYCOMBO-family keyboards, so new models may only need a descriptor entry.
     */
    class AulaBycomboProtocol : public IProtocol
    {
    public:
        const wchar_t* Id() const override { return L"aula_bycombo"; }
        BatteryReading Read(const DeviceDescriptor& device) const override;

        static std::vector<unsigned char> BuildRequest(unsigned char command,
            unsigned short outputLength);
        static bool ParseResponse(const std::vector<unsigned char>& response,
            BatteryReading& reading);
    };
}
