# Release

## Versioning

`project(... VERSION ...)` in `CMakeLists.txt` is the single source of truth:

- the plugin's `TMI_VERSION` and the DLL version resource are generated from it;
- `scripts\package.ps1` names the archive from it;
- the release tag must be `v` + that value (for example `v1.5.6`), and the
  release job rejects a tag that does not match.

Create the tag from the same value instead of typing it by hand:

```powershell
scripts\tag-release.ps1 -Push
```

Also add a `CHANGELOG.md` section and update `docs/PROTOCOLS.md` / the README
table for new devices before tagging.

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
bin\x64\display_tests.exe
bin\x64\hid-probe.exe
```

## Test

```powershell
bin\x64\protocol_tests.exe
bin\x64\settings_tests.exe
bin\x64\display_tests.exe
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
contains the x64/x86 DLLs, README, LICENSE, NOTICE, CHANGELOG, `docs/*.md`
(with the taskbar image) and `LICENSES/`.

## GitHub Actions

`.github/workflows/build.yml` builds x64 and x86 on `windows-latest`, runs all
tests for x64, and uploads the DLLs as artifacts on every push and pull
request. Pushing a `v*` tag additionally runs the `release` job: it verifies
the tag matches the CMake version, downloads both artifacts, runs
`scripts\package.ps1` and creates a GitHub Release with the zip attached.
