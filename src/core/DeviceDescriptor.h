#pragma once

#include <string>

namespace pb
{
    enum class DeviceKind
    {
        Mouse,
        Keyboard,
        Gamepad,
        Headset,
        Other
    };

    const wchar_t* DeviceKindName(DeviceKind kind);

    /**
     * One physical wireless device that this plugin knows how to read.
     *
     * The descriptor intentionally contains only stable, user-visible data.
     * Protocol-specific framing and parsing live in an IProtocol implementation.
     */
    struct DeviceDescriptor
    {
        std::wstring id;            // Stable plugin item ID, ASCII only.
        std::wstring name;          // Human-readable model name.
        std::wstring vendor;        // Brand.
        DeviceKind kind = DeviceKind::Other;
        unsigned short vid = 0;
        unsigned short pid = 0;
        unsigned short usagePage = 0;
        unsigned short usage = 0;
        std::wstring protocolId;    // Registered protocol, e.g. "juxing".
        int priority = 100;         // Lower value sorts first.
        bool enabledByDefault = true;
        std::wstring defaultLabel;  // Short taskbar label.
    };
}
