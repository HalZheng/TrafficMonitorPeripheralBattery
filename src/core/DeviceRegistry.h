#pragma once

#include "DeviceDescriptor.h"

#include <string>
#include <vector>

namespace pb
{
    class DeviceRegistry
    {
    public:
        DeviceRegistry();

        const std::vector<DeviceDescriptor>& All() const { return m_devices; }
        const DeviceDescriptor* FindById(const std::wstring& id) const;

    private:
        std::vector<DeviceDescriptor> m_devices;
    };
}
