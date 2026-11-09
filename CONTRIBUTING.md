# Contributing

## Adding a device

The preferred path is the AI-assisted workflow in
[`skills/adapt-wireless-battery/SKILL.md`](skills/adapt-wireless-battery/SKILL.md)
and the human-readable guide in [`docs/ADDING_A_DEVICE.md`](docs/ADDING_A_DEVICE.md).

At minimum a device contribution should include:

1. A descriptor entry in `src/core/DeviceRegistry.cpp`.
2. A protocol reader or an extension to an existing protocol family.
3. Captured request/response bytes in `tests/protocol_tests.cpp`.
4. A row in `docs/PROTOCOLS.md` and the README supported-device table.
5. A CHANGELOG entry.

## Build and test

```powershell
scripts\build.ps1
```

or:

```powershell
cmake --preset x64
cmake --build --preset x64
ctest --test-dir build\x64 -C Release --output-on-failure
```

Please test both x64 and x86 when changing the plugin layer.

## Code guidelines

- C++17, no exceptions across the DLL boundary.
- All HID reads/writes must have timeouts; never block the TrafficMonitor UI
  thread.
- Protocol readers are read-only. Do not send firmware-update, reset or pairing
  commands.
- Keep item IDs stable and ASCII; they are persisted in TrafficMonitor config.
- Add a test fixture for every new response parser.
- Do not commit vendor binaries, captures containing MAC addresses/serials, or
  personal data.

## License

By contributing, you agree that your contribution is licensed under the MIT
License. `include/PluginInterface.h` remains under the TrafficMonitor Anti-996
License; see `NOTICE.md`.
