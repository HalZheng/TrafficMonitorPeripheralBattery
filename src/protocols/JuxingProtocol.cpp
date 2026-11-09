#include "JuxingProtocol.h"

#include "../core/HidTransport.h"

#include <windows.h>

namespace pb
{
    std::vector<unsigned char> JuxingProtocol::BuildRequest(unsigned short outputLength)
    {
        std::vector<unsigned char> request(outputLength, 0);
        if (request.size() >= 4)
        {
            request[0] = 0x0B; // output report ID
            request[1] = 0x13; // get device status
            request[2] = 0x00;
            request[3] = 0x00;
        }
        return request;
    }

    bool JuxingProtocol::ParseResponse(const std::vector<unsigned char>& response,
        BatteryReading& reading)
    {
        if (response.size() <= 19)
            return false;
        if (response[0] != 0x0A || response[1] != 0x13 || response[3] != 0x00)
            return false;

        reading.online = true;
        reading.percent = response[18];
        reading.charge = response[17] != 0 ? ChargeState::Charging : ChargeState::Discharging;
        if (reading.percent >= 100 && reading.charge == ChargeState::Charging)
            reading.charge = ChargeState::Full;
        return reading.percent >= 0 && reading.percent <= 100;
    }

    BatteryReading JuxingProtocol::Read(const DeviceDescriptor& device) const
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
        std::vector<unsigned char> request = BuildRequest(hid.OutputLength());

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            if (!hid.Write(request, 1000))
                continue;

            ULONGLONG end = GetTickCount64() + 1200;
            while (GetTickCount64() < end)
            {
                std::vector<unsigned char> response;
                if (!hid.Read(response, 300))
                    continue;
                if (ParseResponse(response, reading))
                    return reading;
            }
        }

        reading.online = false;
        reading.note = L"no response";
        return reading;
    }
}
