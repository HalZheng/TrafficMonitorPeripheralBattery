#include "Compx17Protocol.h"

#include "../core/HidTransport.h"

#include <windows.h>

namespace pb
{
    namespace
    {
        constexpr size_t kFrameLength = 17;
        constexpr unsigned char kRequestReportId = 0x08;
        constexpr unsigned char kCommand = 0x04; // get battery
        constexpr unsigned int kChecksumTarget = 0x55;
    }

    std::vector<unsigned char> Compx17Protocol::BuildRequest(unsigned short outputLength)
    {
        if (outputLength != kFrameLength)
            return {};

        std::vector<unsigned char> request(kFrameLength, 0);
        request[0] = kRequestReportId;
        request[1] = kCommand;

        unsigned int sum = 0;
        for (size_t i = 0; i + 1 < request.size(); ++i)
            sum += request[i];
        request.back() = static_cast<unsigned char>((kChecksumTarget - sum) & 0xFF);
        return request;
    }

    bool Compx17Protocol::ParseResponse(const std::vector<unsigned char>& response,
        BatteryReading& reading)
    {
        if (response.size() != kFrameLength)
            return false;
        // Windows answers with report ID 0x08; firmware revisions that follow
        // the descriptor's INPUT report use 0x09 instead.
        if (response[0] != 0x08 && response[0] != 0x09)
            return false;
        if (response[1] != kCommand)
            return false;

        // The same collection also carries unrelated traffic. A frame that
        // merely starts with 08 04 can hold garbage in the battery byte, so
        // the whole-frame checksum is mandatory.
        unsigned int sum = 0;
        for (unsigned char byte : response)
            sum += byte;
        if ((sum & 0xFF) != kChecksumTarget)
            return false;

        const int percent = response[6];
        if (percent < 0 || percent > 100)
            return false;

        reading.online = true;
        reading.percent = percent;
        reading.charge = response[7] != 0
            ? ChargeState::Charging : ChargeState::Discharging;

        // Pack voltage is big endian; reject zeroed/implausible fields.
        const int voltageMv = (static_cast<int>(response[8]) << 8) | response[9];
        reading.voltageMv = (voltageMv > 2000 && voltageMv < 5000) ? voltageMv : -1;

        if (reading.percent >= 100 && reading.charge == ChargeState::Charging)
            reading.charge = ChargeState::Full;
        return true;
    }

    BatteryReading Compx17Protocol::Read(const DeviceDescriptor& device) const
    {
        BatteryReading reading;
        reading.protocol = Id();

        HidDevice hid;
        if (!hid.Open(device.vid, device.pid, device.usagePage, device.usage,
                device.reportLength))
        {
            reading.note = L"HID interface not found";
            return reading;
        }

        reading.found = true;
        const std::vector<unsigned char> request = BuildRequest(hid.OutputLength());
        if (request.empty())
        {
            reading.note = L"unexpected output report length";
            return reading;
        }

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            if (!hid.Write(request, 1000))
                continue;

            const ULONGLONG end = GetTickCount64() + 800;
            while (GetTickCount64() < end)
            {
                std::vector<unsigned char> response;
                if (!hid.Read(response, 250))
                    continue;
                if (ParseResponse(response, reading))
                    return reading;
            }
        }

        reading.online = false;
        reading.note = L"no response (mouse may be asleep)";
        return reading;
    }
}
