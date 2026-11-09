# Changelog

## [1.2.0] - 2026-11-09

### Added

- Win32 options dialog behind the plugin manager's “选项” button: refresh
  interval, max devices, display mode, device list, per-device enable/name/
  label/priority/colour. OK saves back to the INI.
- Host display settings are honoured through `OnExtenedInfo`:
  `EI_TASKBAR_WND_NOT_SHOW_PERCENT`, `EI_MAIN_WND_NOT_SHOW_PERCENT`,
  `EI_DRAW_TASKBAR_WND`, `EI_LABEL_TEXT_COLOR`, `EI_VALUE_TEXT_COLOR`.
- Custom-drawn battery display following the official Battery plugin pattern:
  label + battery outline + proportional fill + percentage.
- Per-device color is now used by the bar renderer.
- Low battery (≤20%) turns the fill red; charging turns it green.
- Dark/light taskbar themes choose a readable border and text colour.
- Plugin metadata now reports the repository URL; the DLL carries a generated
  version resource (file/product version, description, copyright).
- `settings_tests` covers INI defaults, save/load round-trip and duplicate
  section regressions.

### Changed

- `display_mode` now defaults to `bar`. `text` remains available for the old
  host-rendered `G03 84%` style.
- `fill` and `outline` are drawn with GDI; no icon resources or GDI+ dependency.
- The plugin version is generated from the CMake project version, so
  `TMI_VERSION`, the DLL resource and the release archive stay in sync.
- README now includes a real taskbar screenshot and the new test targets.

### Fixed

- Settings were written with a UTF-8 BOM, so `WritePrivateProfileStringW` kept
  appending duplicate `[plugin]` and `[device:...]` sections. The file is now
  rewritten from scratch on every save.

## [1.1.0] - 2026-11-09

### Added

- Flydigi Direwolf 4 battery item via the Flydigi Space Station named pipe
  (`\\.\pipe\fcs.sock`), with 0–6 band parsing and approximate percentage.
- `FlydigiPipeProtocol` and protobuf unit test for the Space Station response.
- Documentation for the Flydigi pipe protocol and its limitations.

### Notes

- Flydigi Direwolf 4 requires the official Flydigi Space Station service to be
  installed and running. XInput's battery value is not used because the receiver
  commonly reports `WIRED/FULL`.
- The Space Station App occupies the pipe while open; close the App and leave
  the background service running.
- The tooltip shows the raw `n/6` band alongside the approximate percentage.

## [1.0.0] - 2026-11-09

### Added

- TrafficMonitor native plugin with taskbar battery items for AUSDOM G03 Pro
  (`363C:FF99`) and iLovBee B87 (`3554:FA09`).
- Generic HID transport with overlapped I/O and per-operation timeouts.
- Protocol registry: Juxing and Compx/BK3632 BYCOMBO.
- Device registry with stable item IDs, priority and default labels.
- INI settings: refresh interval, max devices, per-device enable/name/label/
  priority/color.
- Plugin commands: "立即刷新" and "打开设置文件".
- `hid-probe` console tool for HID enumeration, raw send, passive read and
  feature reports.
- Protocol unit tests using captured frames.
- CMake presets, `scripts/build.ps1`, `scripts/package.ps1` and GitHub Actions.
- AI skill `skills/adapt-wireless-battery` with capture, template and checklist
  references.

### Changed

- Project renamed from the initial single-file prototype to
  `TrafficMonitorPeripheralBattery`.
- Keyboard label corrected to iLovBee B87; the old `redragon_battery` item ID is
  replaced by `ilovbee_b87`.

### Known limitations

- Display mode is text only; custom-drawn battery bars are planned.
- `priority_mode=lowest` and `priority_mode=charging` are reserved but currently
  behave as fixed order at startup.
- A change to `max_devices` or the enabled device set requires restarting
  TrafficMonitor.
- Flydigi Direwolf support is not implemented yet.
