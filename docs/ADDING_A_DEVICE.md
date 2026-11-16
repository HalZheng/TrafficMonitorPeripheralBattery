# Adding a device

This is the short human-readable version. The full AI workflow is in
[`skills/adapt-wireless-battery/SKILL.md`](../skills/adapt-wireless-battery/SKILL.md).

## 1. Identify the vendor collection

```powershell
bin\x64\hid-probe.exe --list
```

Look for a `usagePage` in `0xFF00`-`0xFFFF`, usually `usage 0x0002`. Record
VID/PID, report lengths and product/manufacturer strings.

## 2. Capture the conversation

- Web driver: monkeypatch `HIDDevice.prototype.sendReport` and listen to
  `inputreport`.
- Desktop driver: capture with USBPcap + Wireshark.
- Public protocol: search by VID/PID and chip family; keep attribution.

See [capture-guide.md](../skills/adapt-wireless-battery/references/capture-guide.md).

## 3. Reproduce with hid-probe

```powershell
bin\x64\hid-probe.exe --send 3554 FA09 FF02 0002 "13 05 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 19" 3
```

The first byte of the frame is the report ID. The tool pads to the interface's
output report length. If the receiver exposes several collections with the
same usage, append `--len <decimal bytes>` to pick one by report length, and
use `--set-feature` / `--get-input` for feature-only or input-only collections.
For example the ATK A9 Plus 2.0 receiver has both 20-byte and 17-byte
`FF02/0002` collections; only `--len 17` answers the battery frame.

## 4. Add the descriptor

Edit `src/core/DeviceRegistry.cpp`:

```cpp
m_devices.push_back(DeviceDescriptor{
    L"vendor_model", L"Vendor Model", L"Vendor",
    DeviceKind::Mouse, 0x1234, 0x5678, 0xFF00, 0x0002,
    L"example_protocol", 30, true, L"VM", 0   // reportLength, 0 = first match
});
```

Set the trailing `reportLength` when several collections share the same usage
so `HidDevice::Open` and `IProtocol::IsPresent` both target the right one.

Keep the ID ASCII and stable: it names the INI section and identifies the
battery group inside the plugin's single item. TrafficMonitor itself only ever
sees the fixed item ID `peripheral_battery`. Adding a descriptor does not
rewrite the user's INI or change the host's display item list.

## 5. Add the protocol

- If the device matches an existing family, extend that parser.
- Otherwise add `src/protocols/YourProtocol.h/.cpp` implementing `IProtocol`.
- Register it in `src/protocols/ProtocolRegistry.cpp`.
- For non-HID transports, override `IProtocol::IsPresent()` so discovery can
  detect the receiver/service; the default matches HID VID/PID/usage.
- Keep framing/parsing in static helpers so they can be tested without hardware.

## 6. Add tests and docs

- Add captured bytes to `tests/protocol_tests.cpp`.
- Update `docs/PROTOCOLS.md`, the README supported-device table and
  `CHANGELOG.md`.
- Run:

```powershell
scripts\build.ps1
bin\x64\protocol_tests.exe
```

## 7. Verify on hardware

- Value matches the vendor UI or changes with real discharge.
- Powering the device off makes the value become `--` (or the configured
  offline state).
- TrafficMonitor does not freeze while the device sleeps.
- Test both x64 and x86 DLLs.
