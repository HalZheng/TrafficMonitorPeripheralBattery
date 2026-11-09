# Release

## Versioning

- Update `project(... VERSION ...)` in `CMakeLists.txt`. The plugin's
  `TMI_VERSION` and the DLL version resource are generated from this value.
- Add a `CHANGELOG.md` section.
- Update `docs/PROTOCOLS.md` and the README table for new devices.

## Build

```powershell
scripts\build.ps1
```

This produces:

```text
bin\x64\PeripheralBatteryPlugin.dll
bin\x86\PeripheralBatteryPlugin.dll
bin\x64\protocol_tests.exe
bin\x64\settings_tests.exe
bin\x64\hid-probe.exe
```

## Test

```powershell
bin\x64\protocol_tests.exe
bin\x64\settings_tests.exe
```

Manual smoke test:

1. Copy the matching DLL to TrafficMonitor `plugins` as
   `PeripheralBatteryPlugin.dll`.
2. Restart TrafficMonitor.
3. Enable the items in “显示设置”.
4. Verify online, charging and offline states.

## Package

```powershell
scripts\package.ps1
```

The archive is written to `dist/PeripheralBatteryPlugin-<version>.zip` and
contains the x64/x86 DLLs plus README, LICENSE, NOTICE and CHANGELOG.

## GitHub Actions

`.github/workflows/build.yml` builds x64 and x86 on `windows-latest`, runs the
protocol and settings tests for x64, and uploads the DLLs as artifacts.
