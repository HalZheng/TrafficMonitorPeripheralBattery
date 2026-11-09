#include "AulaBycomboProtocol.h"

#include "../core/HidTransport.h"

#include <windows.h>

namespace pb
{
    std::vector<unsigned char> AulaBycomboProtocol::BuildRequest(unsigned char command,
        unsigned short outputLength)
    {
        std::vector<unsigned char> request(outputLength, 0);
        if (request.size() < 3)
            return request;

        request[0] = 0x13;
        request[1] = command;
        request[2] = command == 0x05 ? 0x01 : 0x00;

        unsigned int sum = 0;
        for (size_t i = 0; i + 1 < request.size(); ++i)
            sum += request[i];
        request.back() = static_cast<unsigned char>(sum & 0xFF);
        return request;
    }

    bool AulaBycomboProtocol::ParseResponse(const std::vector<unsigned char>& response,
        BatteryReading& reading)
    {
        if (response.size() < 8 || response[0] != 0x13)
            return false;

        int percent = -1;
        unsigned char status = 0;

        // iLovBee B87: 13 05 01 00 0A ... <percent at byte 10>
        if (response.size() > 10 && response[1] == 0x05)
        {
            percent = response[10];
            status = 0x01;
        }
        // Other BYCOMBO keyboards: 13 4A 01 00 02 <percent> <status>
        else if (response.size() > 6 &&
            response[1] == 0x4A && response[2] == 0x01 &&
            response[3] == 0x00 && response[4] == 0x02)
        {
            percent = response[5];
            status = response[6];
        }
        // Other BYCOMBO keyboards: 13 0A 01 00 04 05 <percent> <status>
        else if (response.size() > 7 &&
            response[1] == 0x0A && response[2] == 0x01 &&
            response[3] == 0x00 && response[4] == 0x04 && response[5] == 0x05)
        {
            percent = response[6];
            status = response[7];
        }

        if (percent < 0 || percent > 100)
            return false;

        reading.online = true;
        reading.percent = percent;
        if (status == 0x10)
            reading.charge = ChargeState::Charging;
        else if (status == 0x01)
            reading.charge = ChargeState::Discharging;
        else
            reading.charge = ChargeState::Unknown;

        if (reading.percent >= 100 && reading.charge == ChargeState::Charging)
            reading.charge = ChargeState::Full;
        return true;
    }

    BatteryReading AulaBycomboProtocol::Read(const DeviceDescriptor& device) const
    {
        BatteryReading reading;
        reading.protocol = Id();

        HidDevice hid;
        if (!hid.Open(device.vid, device.pid, device.usagePage, device.usage))
        {
            reading.note = L"HID interface not found";
            return reading;
        }

        reading.found = true;

        // 0x05 is the iLovBee / BYCOMBO status command; 0xCA is the AULA-family
        // query. Trying both keeps one protocol implementation useful for more
        // keyboards in the same family.
        const unsigned char commands[] = { 0x05, 0xCA };
        for (unsigned char command : commands)
        {
            std::vector<unsigned char> request = BuildRequest(command, hid.OutputLength());
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                if (!hid.Write(request, 1000))
                    continue;

                ULONGLONG end = GetTickCount64() + 700;
                while (GetTickCount64() < end)
                {
                    std::vector<unsigned char> response;
                    if (!hid.Read(response, 250))
                        continue;
                    if (ParseResponse(response, reading))
                        return reading;
                }
            }
        }

        reading.online = false;
        reading.note = L"no response";
        return reading;
    }
}
