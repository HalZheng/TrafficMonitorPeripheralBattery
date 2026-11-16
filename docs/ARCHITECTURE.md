# Architecture

## Goals

- Read battery state directly from vendor HID reports, without vendor software.
- Never block the TrafficMonitor UI thread.
- Make a new device adaptation a small, reviewable change.
- Keep protocol parsing testable without hardware.
- Fail soft: an offline or sleeping device must not freeze the host.
- Show only devices that are actually attached; settings must take effect
  without restarting TrafficMonitor.

## Layers

```
src/core/HidTransport
    SetupAPI enumeration, HID caps, overlapped read/write/feature with timeouts.
    Caches the device path per VID/PID/usage and only re-enumerates when the
    cached path stops opening.

src/protocols/*
    IProtocol implementations. One class per protocol family, not per model.
    IsPresent() decides whether the receiver/service is attached: the default
    matches the enumerated HID interfaces by VID/PID/usage, non-HID transports
    (flydigi_pipe) override it.

src/core/DeviceRegistry
    Compiled-in table of known devices: VID/PID, usage page/usage, protocol ID,
    priority, default label. Adding an entry never rewrites the user's INI.

src/core/DeviceManager
    Enumerates HID interfaces once per cycle, computes the presence set with a
    configurable grace window, reads battery only for present, non-hidden
    devices, and publishes a thread-safe snapshot. Failed reads back off
    exponentially instead of blocking every cycle.

src/plugin/BatteryGroupItem
    The single IPluginItem. Draws a variable number of battery groups inside
    one host display item. In `bar` mode GetItemWidthEx() reports the summed
    width; in `hybrid` mode it reports the widest of four stacked rows
    (CPU, memory and up to two batteries) and requests the host's
    double-line slot through IsDoubleLineExclusive(). `stacked` uses three
    rows (CPU, memory, then two independent battery cells) sharing one
    measured grid, so the bar and both battery edges align exactly and the
    reported width is the content width, not two stretched columns.

src/plugin/*
    ITMPlugin wiring, settings hot reload, options dialog, plugin commands.
```

## Data flow

1. `Plugin::OnInitialize` loads `PeripheralBatteryPlugin.ini` and creates the
   one permanent `BatteryGroupItem`.
2. `DeviceRegistry` provides all known descriptors to `DeviceManager`.
3. The worker enumerates HID interfaces and asks each protocol `IsPresent()`;
   a device stays visible for `presence_grace_seconds` after its receiver
   disappears, showing its cached last-known level (with an offline badge) when
   `keep_last_known` is on, or `--` when it never reported one.
4. Only present, non-hidden devices are read with `IProtocol::Read`.
5. `DataRequired` (UI thread) copies the snapshot, filters hidden devices,
   sorts by priority, truncates to `max_devices` and replaces the item's
   render entries.
6. The host recalculates custom-draw widths every second, so the item grows or
   shrinks as devices come and go. Settings are applied in place; the INI is
   watched by timestamp and hot-reloaded.
7. `GetItemValueText` only returns a cached string.
8. `OnMonitorInfo` (same monitor cycle, right after `DataRequired`) supplies
   `cpu_usage` / `memory_usage`; the item caches them for the hybrid rows and
   the tooltip. No host API polling or extra thread is required.

## Threading

- The worker thread performs all HID I/O.
- `DeviceManager::Snapshot` copies a small vector under a mutex.
- `DataRequired` and `GetItemValueText` are UI-thread only; no HID calls there.
- Every HID read/write has a timeout. A sleeping device answers with its
  cached last-known level (or `--` if it never reported one) after a bounded
  retry window.
- After a failed read the device is retried with exponential backoff (up to
  300 s) unless it has worked before, in which case it is retried on the next
  poll so waking it clears the offline badge within one interval. “立即刷新”
  clears the backoff and retries immediately.

## Non-HID transports

`IProtocol` is not required to use HID. `flydigi_pipe` reads the Flydigi Space
Station named pipe and ignores the descriptor's VID/PID/usage fields; those
fields are informational. Future protocols may use Bluetooth GATT, WinUSB, or
another local service through the same interface.

## Item lifecycle

TrafficMonitor enumerates `GetItem(index)` only while loading plugins, caches
the raw `IPluginItem*` pointers, and keys its own display settings off
`GetItemId()`. It never re-enumerates, and `OR_OPTION_CHANGED` does not trigger
a rebuild in the host. Replacing or adding items at runtime would leave the
host with dangling pointers.

Therefore the plugin exposes exactly one item (`peripheral_battery`) whose
pointer and ID never change. Everything dynamic lives inside it: entries,
width, label, value and colours. The host calls `IsCustomDraw()`, 
`GetItemWidthEx()`, `DrawItem()` and the text getters live, and the taskbar
window recalculates widths about once per second. That is what makes hot-plug,
hiding, reordering, renaming, colour changes and INI edits take effect without
a restart.

The main window's skin renderer uses a layout-defined width instead of
`GetItemWidthEx()`, so a very large device set may be clipped there. The
taskbar window is the primary target and is fully dynamic.

### Vertical layout height budget

`TASKBAR_WND_HEIGHT` is `DPI(32)` in the host. A custom-draw item receives
`rect.Height()` as `h` and cannot grow the window:

- `horizontal_arrange = true`: `32 * 2/3 = 21 px` at 100% DPI.
- non-horizontal layout, ordinary item: `16 px`.
- non-horizontal layout with `IsDoubleLineExclusive() == 1`: `32 px`.

`IsDoubleLineExclusive()` (plugin API 8) is only consulted by the host in the
non-horizontal / screen-edge taskbar layout; it is ignored when horizontal
arrange is on. It also forces any preceding unpaired item into its own
two-line column, so it changes the layout of other taskbar items.

Four rows therefore get ~8 px each at 100% DPI: enough for 7 px labels and a
4–6 px bar, but not for the host's 9 pt font. Three rows (`stacked`) get
10 / 10 / 12 px, which allows a 9 px font and a full 16x10 battery icon in the
last row. When `h` is below 28 px the renderers drop the labels and draw plain
bars / battery silhouettes instead, keeping all values in the tooltip. The plugin must never draw outside its rect, since the
taskbar window is fixed-height and neighbouring items share the same DC.

## Why a compiled-in device registry

It keeps the core small and makes protocol code reviewable. The `DeviceDescriptor`
is deliberately data-only, so an external JSON/INI registry can be added later
without changing `DeviceManager` or the plugin layer.

## Constraints

- TrafficMonitor plugin API version: 8 (do not override `GetAPIVersion`).
- The plugin DLL must match the host bitness (x64/x86).
- The plugin interface header comes from TrafficMonitor; see `NOTICE.md`.
- Do not send firmware-update, reset or pairing commands from a battery reader.
