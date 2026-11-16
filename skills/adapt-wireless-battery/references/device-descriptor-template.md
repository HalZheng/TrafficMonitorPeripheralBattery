# Device descriptor template

## 1. Registry entry

Add to `src/core/DeviceRegistry.cpp`:

```cpp
m_devices.push_back(DeviceDescriptor{
    L"vendor_model",          // stable ASCII device ID (INI section key)
    L"Vendor Model",          // name shown in TrafficMonitor settings
    L"Vendor",                // brand
    DeviceKind::Mouse,        // Mouse / Keyboard / Gamepad / Headset / Other
    0x1234, 0x5678,           // VID / PID
    0xFF00, 0x0002,           // usage page / usage of the vendor collection
    L"example_protocol",      // protocol registered in ProtocolRegistry
    30,                       // priority: lower sorts first
    true,                     // visible by default (not hidden)
    L"VM"                     // short taskbar label
});
```

The host only ever sees one plugin item (`peripheral_battery`). The device ID
is internal: it names the INI section and identifies a battery group inside
that item. Never rename an existing ID without a migration note.

For a non-HID transport, override `IProtocol::IsPresent()` so discovery can
tell whether the receiver/service is attached; the default implementation
matches the enumerated HID interfaces by VID/PID/usage.

## 2. Protocol skeleton

Create `src/protocols/ExampleProtocol.h` and `.cpp`:

```cpp
#pragma once
#include "IProtocol.h"
#include <vector>

namespace pb
{
    class ExampleProtocol : public IProtocol
    {
    public:
        const wchar_t* Id() const override { return L"example_protocol"; }
        BatteryReading Read(const DeviceDescriptor& device) const override;
        // bool IsPresent(const DeviceDescriptor&,
        //     const std::vector<HidInterfaceInfo>&) const override;  // non-HID only

        static std::vector<unsigned char> BuildRequest(unsigned short outputLength);
        static bool ParseResponse(const std::vector<unsigned char>& response,
            BatteryReading& reading);
    };
}
```

Register it in `src/protocols/ProtocolRegistry.cpp`:

```cpp
Register(std::unique_ptr<IProtocol>(new ExampleProtocol()));
```

## 3. User settings

Sections are created lazily, once the device is discovered or the user edits
it. Users can override the defaults in `PeripheralBatteryPlugin.ini`:

```ini
[device:vendor_model]
hidden=false
name=Vendor Model
label=VM
priority=30
color=#4FC3F7
```

## 4. Tests

Add a fixture to `tests/protocol_tests.cpp` using the captured bytes:

```cpp
std::vector<unsigned char> response = { /* captured response */ };
BatteryReading reading;
Check(ExampleProtocol::ParseResponse(response, reading), "example parsed");
Check(reading.percent == 76, "example battery is 76%");
```
