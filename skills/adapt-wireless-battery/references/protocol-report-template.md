# Protocol report template

Copy this file into `docs/PROTOCOLS.md` or a device-specific issue.

## Device

- Brand / model:
- Connection: 2.4G dongle / Bluetooth / USB cable
- Receiver VID:PID:
- Interface usage page / usage:
- Input / output / feature report length:
- Chip / platform (if known):

## Request

```
report ID:
hex:
```

Notes:

- Does the request need a checksum? Algorithm:
- Does it need retries while the device is asleep?
- Is the first byte a report ID or a command?

## Response

```
hex:
```

- Prefix that identifies a valid reply:
- Battery byte offset:
- Battery scale: 0..100 / 0..255 / voltage curve
- Charging flag:
- Full flag:
- Sleep/offline sentinel:
- Voltage bytes (if any):
- Checksum algorithm:

## Verification

- Vendor UI value at capture time:
- Device powered off: response changed to:
- Charging: response changed to:
- Tested on (date / machine / TrafficMonitor version):

## Source

- Vendor driver / web driver URL:
- Public protocol reference (repository / license):
- Captured by:
