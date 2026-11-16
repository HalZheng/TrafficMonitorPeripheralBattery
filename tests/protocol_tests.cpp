#include "protocols/AulaBycomboProtocol.h"
#include "protocols/Compx17Protocol.h"
#include "protocols/FlydigiPipeProtocol.h"
#include "protocols/JuxingProtocol.h"

#include <cstdio>
#include <vector>

namespace
{
    int failures = 0;

    void Check(bool condition, const char* message)
    {
        if (condition)
        {
            std::printf("ok   %s\n", message);
        }
        else
        {
            std::printf("FAIL %s\n", message);
            ++failures;
        }
    }
}

int main()
{
    using namespace pb;

    // AULA / BYCOMBO 20-byte request.
    std::vector<unsigned char> request = AulaBycomboProtocol::BuildRequest(0x05, 20);
    Check(request.size() == 20, "aula request length");
    Check(request[0] == 0x13 && request[1] == 0x05 && request[2] == 0x01,
        "aula request header");
    Check(request[19] == 0x19, "aula request checksum");

    // iLovBee B87 response: battery at byte 10.
    std::vector<unsigned char> response = {
        0x13, 0x05, 0x01, 0x00, 0x0A, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x54, 0x00, 0x00, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0xAB
    };
    BatteryReading reading;
    Check(AulaBycomboProtocol::ParseResponse(response, reading), "aula 0x05 parsed");
    Check(reading.percent == 84, "aula 0x05 battery is 84%");

    // Other BYCOMBO family frame: 13 4A 01 00 02 <pct> <status>.
    std::vector<unsigned char> response4a = {
        0x13, 0x4A, 0x01, 0x00, 0x02, 0x5F, 0x10, 0x00, 0x00
    };
    BatteryReading reading4a;
    Check(AulaBycomboProtocol::ParseResponse(response4a, reading4a), "aula 4A parsed");
    Check(reading4a.percent == 95, "aula 4A battery is 95%");
    Check(reading4a.charge == ChargeState::Charging, "aula 4A charging flag");

    // Juxing 256-byte request.
    std::vector<unsigned char> juxingRequest = JuxingProtocol::BuildRequest(256);
    Check(juxingRequest.size() == 256, "juxing request length");
    Check(juxingRequest[0] == 0x0B && juxingRequest[1] == 0x13,
        "juxing request header");

    // Juxing reply: battery at byte 18.
    std::vector<unsigned char> juxingResponse(64, 0);
    juxingResponse[0] = 0x0A;
    juxingResponse[1] = 0x13;
    juxingResponse[3] = 0x00;
    juxingResponse[18] = 68;
    BatteryReading juxingReading;
    Check(JuxingProtocol::ParseResponse(juxingResponse, juxingReading),
        "juxing reply parsed");
    Check(juxingReading.percent == 68, "juxing battery is 68%");

    // Compx 17-byte battery frame, captured live from the ATK Dragonfly A9
    // Plus 2.0 NANO receiver (373B:10C9, FF02/0002 17-byte collection).
    std::vector<unsigned char> compxRequest = Compx17Protocol::BuildRequest(17);
    Check(compxRequest.size() == 17, "compx17 request length");
    Check(compxRequest[0] == 0x08 && compxRequest[1] == 0x04 &&
        compxRequest[16] == 0x49, "compx17 request header and checksum");
    Check(Compx17Protocol::BuildRequest(20).empty(),
        "compx17 rejects the 20-byte collection");

    std::vector<unsigned char> compxReply = {
        0x08, 0x04, 0x00, 0x00, 0x00, 0x02, 0x4B, 0x00, 0x0F, 0x46,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA7
    };
    BatteryReading compxReading;
    Check(Compx17Protocol::ParseResponse(compxReply, compxReading),
        "compx17 reply parsed");
    Check(compxReading.percent == 75, "compx17 battery is 75%");
    Check(compxReading.voltageMv == 3910, "compx17 pack voltage is 3910 mV");
    Check(compxReading.charge == ChargeState::Discharging,
        "compx17 discharging flag");

    std::vector<unsigned char> badChecksum = compxReply;
    badChecksum[16] ^= 0x01;
    BatteryReading badReading;
    Check(!Compx17Protocol::ParseResponse(badChecksum, badReading),
        "compx17 rejects a bad checksum");

    std::vector<unsigned char> wrongPrefix = compxReply;
    wrongPrefix[1] = 0x05;
    wrongPrefix[16] = static_cast<unsigned char>(wrongPrefix[16] - 1);
    BatteryReading wrongReading;
    Check(!Compx17Protocol::ParseResponse(wrongPrefix, wrongReading),
        "compx17 rejects a wrong command prefix");

    // Some firmware answers with the descriptor's 0x09 INPUT report id.
    std::vector<unsigned char> reply09 = compxReply;
    reply09[0] = 0x09;
    reply09[16] = 0xA6; // keeps the whole-frame sum at 0x55
    BatteryReading reading09;
    Check(Compx17Protocol::ParseResponse(reply09, reading09) &&
        reading09.percent == 75, "compx17 accepts the 0x09 reply id");

    // Flydigi Space Station IpcResult -> getConnectedDevices -> DeviceInfo.
    std::vector<unsigned char> deviceInfo = {
        0x0A, 0x12, 'F', 'l', 'y', 'd', 'i', 'g', 'i', ' ', 'D', 'i', 'r',
        'e', 'w', 'o', 'l', 'f', ' ', '4', // productName: "Flydigi Direwolf 4"
        0x10, 0x01,                         // category = gamepad
        0x18, 0x02,                         // connectType = dongle
        0x40, 0x01,                         // connected = true
        0x48, 0x05                          // batteryLevel = 5
    };
    std::vector<unsigned char> deviceList;
    deviceList.push_back(0x0A);
    deviceList.push_back(static_cast<unsigned char>(deviceInfo.size()));
    deviceList.insert(deviceList.end(), deviceInfo.begin(), deviceInfo.end());

    std::vector<unsigned char> flydigiPayload;
    flydigiPayload.push_back(0x18); // IpcResult.code = 0
    flydigiPayload.push_back(0x00);
    flydigiPayload.push_back(0x2A); // IpcResult.getConnectedDevices
    flydigiPayload.push_back(static_cast<unsigned char>(deviceList.size()));
    flydigiPayload.insert(flydigiPayload.end(), deviceList.begin(), deviceList.end());

    std::vector<FlydigiPipeProtocol::DeviceInfo> flydigiDevices;
    int flydigiCode = -1;
    Check(FlydigiPipeProtocol::ParseConnectedDevices(
        flydigiPayload, flydigiDevices, &flydigiCode), "flydigi payload parsed");
    Check(flydigiCode == 0, "flydigi error code is 0");
    Check(flydigiDevices.size() == 1, "flydigi one device");
    Check(!flydigiDevices.empty() &&
        flydigiDevices[0].productName == L"Flydigi Direwolf 4",
        "flydigi product name");
    Check(!flydigiDevices.empty() &&
        flydigiDevices[0].category == 1 &&
        flydigiDevices[0].connectType == 2 &&
        flydigiDevices[0].connected &&
        flydigiDevices[0].batteryLevel == 5,
        "flydigi device fields");

    std::printf(failures == 0 ? "all protocol tests passed\n" : "%d test(s) failed\n",
        failures);
    return failures == 0 ? 0 : 1;
}
