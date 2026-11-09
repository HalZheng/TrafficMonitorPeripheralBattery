# Device adaptation checklist

## Discovery

- [ ] Brand and exact model recorded.
- [ ] Connection method recorded (2.4G / Bluetooth / cable).
- [ ] Receiver VID:PID and vendor usage page/usage recorded.
- [ ] Product/manufacturer strings recorded.
- [ ] Vendor software exists? Desktop / web / none.

## Capture

- [ ] Request hex captured.
- [ ] Response hex captured.
- [ ] Battery byte identified with a known percentage.
- [ ] Charging behavior captured.
- [ ] Device-off / sleep behavior captured.
- [ ] Checksum or second invariant identified.

## Implementation

- [ ] Descriptor added with stable ASCII ID.
- [ ] Protocol registered.
- [ ] Read-only commands only; no DFU/reset/pairing.
- [ ] Timeouts on every read/write.
- [ ] Retry while asleep.
- [ ] No UI-thread HID I/O.

## Tests

- [ ] Request frame test.
- [ ] Response battery test.
- [ ] Charging flag test.
- [ ] Bad prefix / bad checksum rejected.
- [ ] `scripts\build.ps1 -SkipX86` passes.
- [ ] `protocol_tests.exe` passes.

## Real device

- [ ] Value matches the vendor UI (or a plausible value when no UI exists).
- [ ] Value changes with real use.
- [ ] Device off shows `--` or a configured offline state.
- [ ] TrafficMonitor does not freeze while the device sleeps.
- [ ] x64 and x86 builds both load.

## Documentation

- [ ] `docs/PROTOCOLS.md` updated.
- [ ] `README.md` supported-device row added.
- [ ] `CHANGELOG.md` updated.
- [ ] Fixture committed.
