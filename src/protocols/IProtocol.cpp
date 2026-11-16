#include "IProtocol.h"

namespace pb
{
    bool IProtocol::IsPresent(const DeviceDescriptor& device,
        const std::vector<HidInterfaceInfo>& interfaces) const
    {
        if (device.vid == 0 && device.pid == 0)
            return false;

        for (const HidInterfaceInfo& info : interfaces)
        {
            if (info.vid != device.vid || info.pid != device.pid)
                continue;
            if (device.usagePage != 0 && info.usagePage != device.usagePage)
                continue;
            if (device.usage != 0 && info.usage != device.usage)
                continue;
            // Some receivers expose several collections with the same usage.
            // Require the exact report length the protocol talks to, otherwise
            // a dongle with only the wrong collection would look present.
            if (device.reportLength != 0 &&
                info.inputLength != device.reportLength &&
                info.outputLength != device.reportLength)
                continue;
            return true;
        }
        return false;
    }
}
