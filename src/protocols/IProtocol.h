#pragma once

#include "../core/BatteryReading.h"
#include "../core/DeviceDescriptor.h"

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
    };
}
