# Changelog

## [1.5.6] - 2026-11-15

### Added

- 「高级设置...」button in the options dialog opens a second dialog that edits
  every remaining global `[plugin]` key: `keep_last_known`,
  `low_battery_notify` / `_percent`, `presence_grace_seconds`, `show_offline`,
  `stacked_show_percent`, `row_style`, `hybrid_rows`, `row_bar_width` and the
  four colour thresholds. Common configuration no longer needs hand-editing
  the INI. The secondary dialog owns no list box, so the main dialog's
  `LBN_SELCHANGE` guard and layout are untouched.
- Neutral tones (battery track, offline outline, offline badge) now follow a
  **custom** host text colour: `EI_VALUE_TEXT_COLOR` feeds
  `ColorRules::hostTextColor` and tints the greys by 20 %. With no colour, or
  with the built-in black / white defaults, the pixels are byte-for-byte
  unchanged — the verified look is preserved.

### Changed

- `Settings::ClampValues()` is now the single source of range limits, shared by
  INI loading and the advanced dialog (including the low < good and
  orange < red ordering), so the two paths cannot drift apart.

### Tests

- `settings_tests`: `ClampValues()` limits and ordering.
- `display_tests`: `NeutralColor` default-unchanged / custom-tint assertions
  and a ninth render segment using a custom host colour.

## [1.5.5] - 2026-11-15

### Added

- **Low-battery notification.** One taskbar notification when a **live,
  discharging** reading drops to `low_battery_notify_percent` (default 10).
  A per-device latch plus a +5 % recovery hysteresis prevents repeat alerts;
  cached (stale) readings and charging devices never trigger one. Disable with
  `low_battery_notify=false`.
- Third plugin command「诊断信息」: a Ctrl+C-copyable message box showing the
  version of the DLL the host actually loaded, the module and INI paths, the
  effective display/alert settings, and a per-device table (online/sleeping,
  level, cached flag, last good reading age, consecutive failed reads). It makes
  "the code changed but the host still runs the old DLL" and "why is this device
  not reporting" visible without external tools.

### Fixed

- `text` mode no longer appends `×` to a cached reading (a 1.5.4 addition).
  `GetItemValueSampleText()` fixes the host's text column width, so the extra
  characters could be clipped or overlap the following item. `text` mode now
  renders exactly the same value as a live reading; the offline state is only
  in the tooltip.

### Tests

- `settings_tests`: low-battery setting round-trip and the full alert state
  machine (once per episode, hysteresis, stale / disabled / re-enabled).
- `display_tests`: cached text equals live text and the sample width is identical.

## [1.5.4] - 2026-11-15

### Added

- Sleeping or disconnected devices keep their **last measured battery level**
  instead of falling back to `--` and then disappearing. A cached reading is
  drawn exactly like a live one — same percentage, fill length and level/charging
  colour — with a small high-contrast `x` badge in the top-right corner of the
  battery icon (`bar` / `stacked`) or the row bar (`hybrid`, and `stacked` when
  the height is too small for text). `text` mode appends `×` to the value and
  the tooltip reads「离线，最后读数 · N 分钟前」.
- `keep_last_known=true` (default) in `[plugin]` controls this; setting it to
  `false` restores the old `--` / grace-window behaviour. The cache lives in
  memory only and is cleared when TrafficMonitor restarts. A device that never
  reported a valid level still follows `presence_grace_seconds` and shows `--`.
- `DeviceState` now stores `lastGood` plus a timestamp, and `BatteryReading`
  carries a `stale` flag. `present` stays true for any device that once reported
  a level, so the taskbar layout no longer changes when a keyboard/mouse sleeps.

### Changed

- A failed read while the receiver is still attached no longer uses the long
  exponential backoff if that device has worked before: it is retried on the
  next poll, so waking the device clears the offline badge within one refresh
  interval. Devices that never produced a reading keep the old backoff.

### Tests

- `display_tests`: cached-reading text marker, unchanged level colour, unchanged
  bar / stacked / hybrid widths, GDI smoke tests for the badge in all draw
  paths, and two extra offline-badge segments in `--render`.
- `settings_tests`: `keep_last_known` default, persistence and round-trip, plus
  `BatteryReading::stale` not invalidating `HasPercent()`.

## [1.5.3] - 2026-11-15

### Fixed

- Options dialog: selecting any device in「已发现 / 已配置的设备」snapped the
  selection straight back to the first entry, making only the first device
  editable. `CommitCurrent()` unconditionally did `LB_DELETESTRING` /
  `LB_INSERTSTRING` / `LB_SETCURSEL(state.current)` before the selection-change
  handler read `LB_GETCURSEL`, so the handler always loaded the previously
  selected (initially first) row.
- The selection handler now captures the clicked index first, `CommitCurrent`
  only rewrites a list row when its caption actually changed, and a
  `suppressSelChange` guard covers every programmatic list rebuild
  (`PopulateDevices`, row refresh, `SelectDeviceRow`) to stop `LBN_SELCHANGE`
  re-entry. Editing a name / visibility still refreshes that row's caption
  without losing the current selection.
- Opening the dialog now highlights the first device explicitly instead of
  leaving the list box with no selected row.
- `ReadGlobals()` no longer overwrites `display_mode` with `bar` when the mode
  combo reports `CB_ERR`; the loaded value is preserved. This could silently
  reset a `stacked` configuration when the dialog was confirmed.

## [1.5.2] - 2026-11-15

### Changed

- `stacked` now uses one shared grid for all three rows. The CPU/memory bar
  and the first battery icon start at exactly the same x, and the last battery
  icon (or its value) ends at exactly the right edge. Width comes from the
  actual cells instead of stretching two equal columns, which removes the
  wide blank space after each battery group: **168 px -> 98 px** at 100% DPI.
- `stacked` hides percentages by default. New `stacked_show_percent=true`
  adds a measured value column to every cell; the grid widens to ~152 px and
  stays fully aligned (system values, both battery values and both bar/icon
  edges keep the same grid).
- A single device now shrinks the grid instead of leaving an empty second
  column; with no devices only the two system rows are drawn.
- `DrawBatteryIcon` is shared by `bar` and `stacked`, so charging outlines and
  low-level fills look identical in both modes.

### Tests

- `display_tests`: exact grid width invariants, default-hidden percentages,
  widened grid when enabled, single-device shrink, GDI smoke tests for both
  variants, and a six-segment visual preview.
- `settings_tests`: `stacked_show_percent` default and round-trip.

## [1.5.1] - 2026-11-14

### Added

- `display_mode=stacked`: three rows instead of four — CPU, memory, then a
  third row holding two separate battery cells (short label + 16x10 battery
  icon + percentage). A 32 px slot splits into 10 / 10 / 12 px, so the row
  font grows from 7 px to 9 px and the battery icon needs no clipping.
- The battery row reuses the original `bar`-style icon with level-based fill,
  but with compact 9 px text. Fewer than two devices collapses to two rows
  (16 px each, 9 px font); with no devices only CPU and memory are drawn.
- `BatteryItem::StackedWidth()` measures the two cells with the same small
  font used for drawing, so `GetItemWidthEx()` stays accurate (~168 px at
  100% DPI: two ~76 px cells plus a gap; wider than hybrid's ~100 px but
  still below the old ~194 px three-device horizontal group).
- Options dialog gets a fourth display mode; `display_tests --render` now
  outputs five segments (stacked / solid hybrid / framed hybrid / 21 px
  fallback / horizontal bar).

### Changed

- `IsDoubleLineExclusive()` now returns 1 for both `hybrid` and `stacked`.
- Charging batteries tint the icon outline as well as the fill, and the fill
  is rounded up to at least 2 px, so a low charging level stays visible in a
  16x10 icon.
- Unknown `display_mode` values still fall back to `bar`.

### Tests

- `display_tests`: stacked double-line flag, width invariants, GDI smoke test
  at 32 px and at the 21 px fallback.
- `settings_tests`: `stacked` round-trip.

## [1.5.0] - 2026-11-14

### Added

- `display_mode=hybrid`: four stacked rows — CPU, memory, then up to two
  batteries in priority order. The system rows use the host-style progress
  bar, battery rows use a mini battery outline; every row has a short label
  and a percentage, and all values are also in the tooltip.
- CPU/memory usage comes from `ITMPlugin::OnMonitorInfo`, pushed by the host
  once per monitor cycle (default 1000 ms). No sampling thread, no PDH work,
  no repeated host API polling.
- `IsDoubleLineExclusive()` (plugin API 8) requests the full 32 px taskbar slot
  in hybrid mode; `GetItemWidthEx()` returns the widest row instead of the sum,
  which is what actually reduces the horizontal footprint.
- Level-based colours following the common mobile battery convention: ≥60%
  green, 21–59% orange, ≤20% red, charging cyan blue, offline grey. CPU/memory
  use the same ramp against usage thresholds (50 / 80). All four colours and
  all four thresholds are configurable; dark mode lifts them towards white.
- New INI keys: `hybrid_rows`, `row_bar_width`, `battery_low_percent`,
  `battery_good_percent`, `cpu_orange_percent`, `cpu_red_percent`,
  `color_high` / `color_medium` / `color_low` / `color_charging`.
- Options dialog: third display mode and a per-device「按电量自动着色」toggle.
- `row_style=solid` (default): frameless rows with no outline, inset or
  vertical padding. The coloured fill grows from ~2 px to ~7 px inside the
  same 8 px row, and the label/bar gap shrinks from 3 px to 1 px.
  `row_style=framed` keeps the old stroked look for one-key rollback.
- `display_tests --render <path.bmp>` renders the solid / framed / fallback /
  bar layouts into a bitmap for visual regression.

### Changed

- `device:<id>.color` defaults to `auto` (colour by charge level) instead of a
  palette entry chosen by registry order. A one-time migration rewrites the six
  old default palette values to `auto` (`color_auto_migrated`); genuinely
  custom colours are preserved.
- `bar` mode now uses the same level colours, and charging no longer forces green.
- Unknown `display_mode` values fall back to `bar`.
- Hybrid host requirement: turn off「水平排列」in the taskbar display settings.
  With horizontal arrange left on (21 px), the plugin degrades to four
  textless bars rather than drawing unreadable fragments.

### Tests

- `display_tests`: level/system colour thresholds, `auto` parsing, hybrid
  double-line flag, fixed row width, GDI draw smoke test.
- `settings_tests`: new defaults, round-trips, unknown and hybrid mode handling.

## [1.4.0] - 2026-11-14

### Added

- ATK 蜻蜓 A9 Plus 2.0 (`373B:10C9`) battery support via the Compx 17-byte
  vendor frame: request `08 04 ... 49`, whole-frame checksum `0x55`,
  percentage at byte 6, charging flag at byte 7, big-endian pack voltage at
  bytes 8–9. Verified live at 75% / 3910 mV.
- `DeviceDescriptor::reportLength` and length-aware `HidDevice::Open`: the
  ATK receiver exposes both 20-byte and 17-byte `FF02/0002` collections and
  only the 17-byte one answers the battery frame.
- `hid-probe`: `--len N` to pin a collection by report length,
  `--set-feature` and `--get-input` for read-only probing.
- Protocol tests for the Compx frame: request/checksum, live reply,
  bad-checksum and wrong-prefix rejection, `0x09` reply-ID variant.

### Changed

- Default `max_devices` raised from 4 to 6 so five receivers fit.

### Investigated, not supported yet

- Redragon TL84 (`25A7:FA70`): the receiver exposes only an 8-byte feature
  collection (`FF02/0002`, report `0x06`) and an 8-byte input collection
  (`FF01/0000`). Feature `0x06` is a static config register (unchanged across
  repeated reads and after `HidD_SetFeature` queries) and the input
  collection pushed nothing during 65 s of listening. No battery telemetry
  was found over 2.4G. The TL84 desktop driver only offers key remapping,
  lighting and macros (no battery page), and the same Compx receiver on the
  Royal Kludge 68 Pro is documented as not reporting charge to a PC. The
  keyboard also enters deep sleep after 30 minutes idle, so
  `bin/probe-tl84.ps1` is provided to confirm with the keyboard awake.
  A second pass with the keyboard awake added `--exchange` (feature query and
  input listen in one process) and `--listen-all` (all eight input
  collections): zero unsolicited frames, only feature report `0x06` exists,
  and even 8-byte queries carrying the Compx `0x55` checksum only echo byte 1.
  Windows exposes no battery device for it and no vendor driver / USB capture
  tool is installed. See the feature-report warning in `docs/PROTOCOLS.md`.

## [1.3.0] - 2026-11-11

### Added

- Dynamic device discovery: HID interfaces are matched by VID/PID/usage and
  `flydigi_pipe` probes its named pipe, so only attached devices are shown.
  A configurable `presence_grace_seconds` (default 300) keeps unplugged devices
  visible as `--` instead of making the taskbar flicker.
- One permanent display item (`peripheral_battery`) that draws a variable
  number of battery groups; its width grows/shrinks with the live device set.
  The host recalculates custom-draw widths every second, so hot-plug needs no
  restart.
- Options dialog now lists only discovered or previously configured devices,
  with rescan, up/down ordering, and a「在任务栏显示」toggle.
- INI hot reload: external edits are detected by timestamp and applied in
  about two seconds.
- `display_tests` covers text concatenation, dynamic width and text-mode
  fallback; `settings_tests` covers lazy persistence, legacy migration,
  grace-window and presence logic.

### Changed

- All settings (display mode, interval, hide, order, colour, names) now take
  effect immediately. Every “restart TrafficMonitor” prompt was removed.
- `enabled` is replaced by `hidden`; old `enabled=false` is migrated on load.
- INI sections are written lazily, only for discovered or user-edited devices,
  so adding device descriptors no longer bloats the file or rewrites it at
  startup.
- `display_mode=text` concatenates all visible devices into one value instead
  of providing one host item per device.
- Upgrading from 1.2.x replaces the old per-device item IDs; the plugin shows a
  one-time notification asking the user to re-tick「无线外设电量」in the host's
  display settings.

### Known limitations

- The main window skin renderer uses a layout-defined width, so a very large
  device set can be clipped there; the taskbar window is fully dynamic.
- Per-device ordering/colour is now managed inside the plugin's own options
  dialog rather than the host's display settings.

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
- HID device paths are cached per VID/PID/usage, so polling no longer runs a
  full SetupAPI enumeration every cycle.
- Offline devices use exponential retry backoff (up to 300 s); “立即刷新”
  clears it.

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
- Settings are written to a temporary file and atomically replace the target,
  and an existing INI is no longer rewritten on every startup. Hand-written
  comments and unknown keys survive normal restarts.

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
