#include "DeviceRegistry.h"

namespace pb
{
    const wchar_t* DeviceKindName(DeviceKind kind)
    {
        switch (kind)
        {
        case DeviceKind::Mouse:
            return L"mouse";
        case DeviceKind::Keyboard:
            return L"keyboard";
        case DeviceKind::Gamepad:
            return L"gamepad";
        case DeviceKind::Headset:
            return L"headset";
        default:
            return L"other";
        }
    }

    DeviceRegistry::DeviceRegistry()
    {
        // AUSDOM G03 Pro 8K dongle.
        // Protocol: Juxing / WEBHID report 11, command 0x13.
        m_devices.push_back(DeviceDescriptor{
            L"g03_pro",
            L"AUSDOM G03 Pro",
            L"AUSDOM",
            DeviceKind::Mouse,
            0x363C,
            0xFF99,
            0xFFA0,
            0x0002,
            L"juxing",
            10,
            true,
            L"G03"
        });

        // iLovBee B87 2.4G keyboard receiver (Compx / BK3632 BYCOMBO).
        // Protocol: report 0x13, command 0x05, battery in response byte 10.
        m_devices.push_back(DeviceDescriptor{
            L"ilovbee_b87",
            L"iLovBee B87",
            L"iLovBee",
            DeviceKind::Keyboard,
            0x3554,
            0xFA09,
            0xFF02,
            0x0002,
            L"aula_bycombo",
            20,
            true,
            L"B87"
        });

        // Add new devices here. Keep IDs ASCII and stable, because they are
        // written to the TrafficMonitor plugin_display_item configuration.

        // Flydigi Direwolf 4. XInput exposes 045E:028E, but its battery value
        // is unreliable. The flydigi_pipe protocol reads the official Space
        // Station service instead; VID/PID/usage are informational only.
        m_devices.push_back(DeviceDescriptor{
            L"flydigi_direwolf_4",
            L"Flydigi Direwolf 4",
            L"Flydigi",
            DeviceKind::Gamepad,
            0x045E,
            0x028E,
            0x0000,
            0x0000,
            L"flydigi_pipe",
            30,
            true,
            L"Direwolf"
        });
    }

    const DeviceDescriptor* DeviceRegistry::FindById(const std::wstring& id) const
    {
        for (const DeviceDescriptor& device : m_devices)
        {
            if (device.id == id)
                return &device;
        }
        return nullptr;
    }
}
