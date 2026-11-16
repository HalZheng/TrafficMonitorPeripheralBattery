#pragma once

#include "../core/BatteryReading.h"
#include "../core/DeviceDescriptor.h"
#include "../core/HidTransport.h"

namespace pb
{
    /**
     * A protocol family, not a single model.
     *
     * Implementations must be read-only: never send firmware-update, reset or
     * pairing commands while reading battery state.
     */
    class IProtocol
    {
    public:
        virtual ~IProtocol() = default;
        virtual const wchar_t* Id() const = 0;
        virtual BatteryReading Read(const DeviceDescriptor& device) const = 0;

        /**
         * Whether the receiver/service for this device is currently attached.
         *
         * The default matches a freshly enumerated HID interface list by
         * VID/PID and (when set) usage page/usage. Transports that do not use
         * HID must override it. This runs once per polling cycle on the worker
         * thread, so it has to stay cheap and must never block for long.
         */
        virtual bool IsPresent(const DeviceDescriptor& device,
            const std::vector<HidInterfaceInfo>& interfaces) const;
    };
}
