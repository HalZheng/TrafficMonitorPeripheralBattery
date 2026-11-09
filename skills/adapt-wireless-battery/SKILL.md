---
name: adapt-wireless-battery
description: Add a new wireless mouse, keyboard, gamepad, or headset to the TrafficMonitorPeripheralBattery plugin. Use when a device does not expose battery through Windows, when the user asks to adapt a new 2.4G/BLE peripheral, or when a HID protocol needs to be documented and turned into a reader/test.
---

# Adapt a wireless peripheral to TrafficMonitorPeripheralBattery

This skill is a repeatable workflow for an AI coding agent. The goal is a
read-only, tested protocol reader that appears as a TrafficMonitor taskbar item.

## Inputs

Ask for or determine:

- Brand and exact model (for example: `iLovBee B87`, `Flydigi Direwolf 3`).
- Connection used for the receiver: 2.4G dongle, Bluetooth, or USB cable.
- Whether vendor software exists (desktop driver, web driver, or both).
- Whether the device can be powered off/on for live verification.
- Whether the battery is shown in the vendor app, and its approximate value.

Do not guess the protocol from the model name alone. Different batches can use
different receivers.

## Safety rules

1. Battery reading is read-only. Never send firmware-update, reset, pairing, or
   flash commands while probing.
2. Prefer commands the vendor driver uses for "get status" or "get battery".
   Avoid unknown command bytes that may enter DFU/bootloader mode.
3. Every HID read/write must have a timeout. The plugin worker thread must never
   block the TrafficMonitor UI.
4. Treat `0xFF`, `0x00`, and unchanged values as "unknown" until verified.
5. Verify a value by changing the physical state: power off/on, unplug/replug,
   charge/discharge. A value that never changes is not a battery reading.

## Workflow

### 1. Enumerate the receiver

```powershell
bin\x64\hid-probe.exe --list
```

Record every interface of the receiver:

- VID / PID
- usage page / usage
- input / output / feature report lengths
- product and manufacturer strings

Pick the vendor collection, usually `usagePage 0xFFxx` and `usage 2`. Do not use
the standard keyboard/mouse collection (`0x0001`) for vendor queries.

### 2. Capture the vendor conversation

Use one of these methods, in order of preference:

1. Web driver: open it in Chrome/Edge, monkeypatch `HIDDevice.prototype.sendReport`
   and the `inputreport` event, then click "connect/read battery" in the UI.
2. Desktop driver: capture USB/HID traffic with USBPcap + Wireshark, or an
   equivalent HID monitor, while the driver displays the battery.
3. Public protocol: search GitHub for the VID/PID and the chip family
   (`Compx`, `BYCOMBO`, `BK3632`, `Nordic`, `Telink`, etc.). Port only code whose
   license allows it and keep attribution.

Write down the exact request and response bytes. Hex dumps are required.

### 3. Reproduce with hid-probe

```powershell
# Send a full output report. The first byte is the report ID.
bin\x64\hid-probe.exe --send 3554 FA09 FF02 0002 "13 05 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 19" 3

# Passive listen
bin\x64\hid-probe.exe --read 363C FF99 FFA0 0002 5

# Feature report
bin\x64\hid-probe.exe --feature 3554 FA09 FF04 0002 06 8
```

A device that is asleep may not answer. Retry after pressing a key or moving the
mouse. Record which attempts answer and how long they take.

### 4. Derive the parser

For each accepted response, determine:

- Frame prefix that identifies the reply (for example `13 05 01 00 0A`).
- Battery byte offset and scale (`0..100`, `0..255`, `0.5%`, voltage curve).
- Charging flag / full flag (if present).
- Offline or sleep sentinel.
- Checksum algorithm and whether it must be validated.
- Whether the same receiver reports multiple devices (keyboard + mouse slots).

Do not accept a reply only because the prefix matches. Validate a checksum or a
second invariant whenever the protocol provides one. Other reports on the same
collection can otherwise be mistaken for battery frames.

### 5. Add the descriptor

Add one entry to `src/core/DeviceRegistry.cpp`:

```cpp
m_devices.push_back(DeviceDescriptor{
    L"vendor_model",          // stable ASCII item ID
    L"Vendor Model",          // display name
    L"Vendor",                // brand
    DeviceKind::Mouse,        // Mouse / Keyboard / Gamepad / Headset / Other
    0x1234, 0x5678,           // VID / PID
    0xFF00, 0x0002,           // usage page / usage
    L"protocol_id",           // registered protocol
    30,                       // default priority (lower = earlier)
    true,                     // enabled by default
    L"VM"                     // short taskbar label
});
```

The `id` becomes the TrafficMonitor `plugin_display_item` key. Never rename an
existing ID without a migration note.

### 6. Add or extend the protocol

- If the device matches an existing protocol family, extend that parser and add
  a fixture instead of creating a new class.
- Otherwise implement `IProtocol` in `src/protocols/` and register it in
  `ProtocolRegistry.cpp`.
- Keep framing, checksum, and parsing in pure static helpers so they can be unit
  tested without hardware.

Protocol class shape:

```cpp
class ExampleProtocol : public IProtocol
{
public:
    const wchar_t* Id() const override { return L"example"; }
    BatteryReading Read(const DeviceDescriptor& device) const override;

    static std::vector<unsigned char> BuildRequest(unsigned short outputLength);
    static bool ParseResponse(const std::vector<unsigned char>& response,
        BatteryReading& reading);
};
```

### 7. Add tests

Copy the captured bytes into `tests/protocol_tests.cpp` and assert:

- request length, report ID, command, checksum;
- battery percentage;
- charging flag;
- rejection of a wrong prefix or bad checksum.

Then run:

```powershell
scripts\build.ps1 -SkipX86
bin\x64\protocol_tests.exe
```

### 8. Verify against real hardware

1. Build `x64` and `x86`.
2. Copy the matching DLL to TrafficMonitor `plugins`.
3. Add the item ID to `plugin_display_item` or enable it in
   "任务栏窗口 -> 显示设置".
4. Confirm the value changes when the device is used and becomes `--` when the
   device is powered off.
5. Check that a sleeping device does not freeze TrafficMonitor.

### 9. Update documentation

Update all of:

- `docs/PROTOCOLS.md` — protocol table and byte offsets.
- `README.md` — supported devices table.
- `CHANGELOG.md` — added device.
- `docs/ADDING_A_DEVICE.md` if the workflow itself changed.

## Deliverables checklist

- [ ] Descriptor entry with stable ID.
- [ ] Protocol implementation or extension.
- [ ] Captured request/response fixture in tests.
- [ ] `docs/PROTOCOLS.md` entry.
- [ ] README supported-device row.
- [ ] CHANGELOG entry.
- [ ] x64 and x86 builds.
- [ ] Real-device verification: online, charging, offline.

## References

- `references/capture-guide.md`
- `references/protocol-report-template.md`
- `references/device-descriptor-template.md`
- `references/checklist.md`
- Project docs: `docs/ARCHITECTURE.md`, `docs/PROTOCOLS.md`,
  `docs/ADDING_A_DEVICE.md`, `docs/SETTINGS.md`
