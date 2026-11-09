#pragma once

#include "IProtocol.h"

#include <string>
#include <vector>

namespace pb
{
    /**
     * Flydigi Direwolf / APEX series battery via the Flydigi Space Station
     * background service.
     *
     * The controller's Xbox-compatible XInput battery value is not reliable
     * (the receiver commonly reports WIRED/FULL). The official service exposes
     * a 0-6 battery band through the named pipe \\.\pipe\fcs.sock.
     *
     * This protocol does not use HID. It ignores the descriptor's VID/PID and
     * talks to the local service instead.
     */
    class FlydigiPipeProtocol : public IProtocol
    {
    public:
        struct DeviceInfo
        {
            std::wstring productName;
            int category = 0;
            int connectType = 0;
            bool connected = false;
            int batteryLevel = -1;
        };

        const wchar_t* Id() const override { return L"flydigi_pipe"; }
        BatteryReading Read(const DeviceDescriptor& device) const override;

        /**
         * Parse a complete IpcResult protobuf payload.
         * Exposed for unit tests; returns false if the response is malformed.
         */
        static bool ParseConnectedDevices(const std::vector<unsigned char>& payload,
            std::vector<DeviceInfo>& devices, int* errorCode = nullptr);
    };
}
