# Architecture

## Goals

- Read battery state directly from vendor HID reports, without vendor software.
- Never block the TrafficMonitor UI thread.
- Make a new device adaptation a small, reviewable change.
- Keep protocol parsing testable without hardware.
- Fail soft: an offline or sleeping device must not freeze the host.

## Layers

```
src/core/HidTransport
    SetupAPI enumeration, HID caps, overlapped read/write/feature with timeouts.
    Caches the device path per VID/PID/usage and only re-enumerates when the
    cached path stops opening.

src/protocols/*
    IProtocol implementations. One class per protocol family, not per model.

src/core/DeviceRegistry
    Compiled-in table of known devices: VID/PID, usage page/usage, protocol ID,
    priority, default label.

src/core/DeviceManager
    Owns the polling worker thread and a thread-safe snapshot of readings.
    Failed devices back off exponentially instead of blocking every cycle.

src/plugin/*
    ITMPlugin / IPluginItem mapping, settings file, plugin commands.
```

## Data flow

1. `Plugin::OnInitialize` loads `PeripheralBatteryPlugin.ini`.
2. `DeviceRegistry` provides all known descriptors.
3. `Plugin::BuildItems` filters disabled devices, sorts by priority, truncates
   to `max_devices`, and creates one `BatteryItem` per device.
4. `DeviceManager` starts a worker thread.
5. The worker calls `IProtocol::Read` for each active device.
6. `BatteryItem` values are updated from a snapshot in `DataRequired`, which
   runs on the TrafficMonitor UI thread.
7. `GetItemValueText` only returns a cached string.

## Threading

- The worker thread performs all HID I/O.
- `DeviceManager::Snapshot` copies a small vector under a mutex.
- `DataRequired` and `GetItemValueText` are UI-thread only; no HID calls there.
- Every HID read/write has a timeout. A sleeping device answers with `--` after
  a bounded retry window.
- After a failed read the device is retried with exponential backoff (up to
  300 s). “立即刷新” clears the backoff and retries immediately.

## Non-HID transports

`IProtocol` is not required to use HID. `flydigi_pipe` reads the Flydigi Space
Station named pipe and ignores the descriptor's VID/PID/usage fields; those
fields are informational. Future protocols may use Bluetooth GATT, WinUSB, or
another local service through the same interface.

## Item lifecycle

TrafficMonitor caches `IPluginItem*` pointers. This plugin therefore creates the
item list once, based on settings at load time. Changing `max_devices`, the
enabled device set, or item order requires restarting TrafficMonitor. Names,
labels and readings can be updated in place.

## Why a compiled-in device registry

It keeps the core small and makes protocol code reviewable. The `DeviceDescriptor`
is deliberately data-only, so an external JSON/INI registry can be added later
without changing `DeviceManager` or the plugin layer.

## Constraints

- TrafficMonitor plugin API version: 8 (do not override `GetAPIVersion`).
- The plugin DLL must match the host bitness (x64/x86).
- The plugin interface header comes from TrafficMonitor; see `NOTICE.md`.
- Do not send firmware-update, reset or pairing commands from a battery reader.
