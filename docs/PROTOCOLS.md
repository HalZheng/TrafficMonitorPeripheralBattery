# Protocols

All protocols here are read-only. They were derived from vendor drivers or
public open-source implementations and verified on real hardware where marked.

## Summary

| Protocol ID | Device | VID:PID | Usage | Request | Reply | Battery |
|---|---|---|---|---|---|---|
| `juxing` | AUSDOM G03 Pro 8K | `363C:FF99` | `FFA0/0002` | output report 11, `13 00 00` | `0A 13 xx 00 ...` | byte 18; charging byte 17 |
| `aula_bycombo` | iLovBee B87 | `3554:FA09` | `FF02/0002` | report `13`, command `05`, checksum | `13 05 01 00 0A ...` | byte 10 |
| `aula_bycombo` | BYCOMBO family | various | `FF02/0002` | report `13`, command `05` or `CA` | `13 4A ...` / `13 0A ...` | byte 5 / byte 6 |
| `flydigi_pipe` | Flydigi Direwolf 4 / APEX | `045E:028E` (XInput) | 飞智空间站命名管道 | `08 81 20 1A 00` | protobuf `IpcResult` | 0–6 档 |

## Juxing (AUSDOM G03 Pro 8K)

- Interface: usage page `0xFFA0`, usage `0x0002`.
- Output report length: 256; input report length: 64.
- Request:

```text
0B 13 00 00 00 ... 00
^^ report ID
   ^^ command 0x13
```

- Reply:

```text
0A 13 37 00 05 ... 00 <percent> <charging> <wireless> ...
                  ^ byte 18       ^ byte 17
```

- Verified live: battery 67-69%, matching the user-visible value; charging flag
  byte 17.
- Source: AUSDOM `ausdomhub.com` WebHID application, command sequence captured
  from its JavaScript bundle.

## BYCOMBO / Compx 20-byte (iLovBee B87)

- Interface: usage page `0xFF02`, usage `0x0002`, 20-byte input/output.
- Request frame:

```text
13 05 01 00 00 ... 00 <checksum>
^^ report ID
   ^^ command 0x05
```

- Checksum: sum of the first 19 bytes, low 8 bits.
- Reply:

```text
13 05 01 00 0A 03 00 00 00 01 54 ...
                              ^^ byte 10 = 0x54 = 84%
```

- The keyboard only answers while powered on. Powering it off makes the query
  time out, which is how the byte was verified as live battery state.
- Other BYCOMBO-family keyboards may answer with:

```text
13 4A 01 00 02 <percent> <status>
13 0A 01 00 04 05 <percent> <status>
```

  Status `0x10` = charging, `0x01` = discharging.
- Reference implementations: `Volt` and `kun-qaq/wireless-battery-tray` (both
  MIT) document the Compx/BYCOMBO family.

## Flydigi Direwolf 4 / APEX (Space Station pipe)

Flydigi controllers expose a vendor-specific USB XInput interface
(class `0xFF`, subclass `0x5D`, protocol `0x01`), not a HID collection. On
Windows that interface is bound to the Xbox controller driver, so a normal
plugin cannot claim it without replacing the driver. The XInput battery API is
also unreliable: the Direwolf 4 receiver commonly reports `WIRED/FULL`.

The official Flydigi Space Station service exposes the real battery band through
a local named pipe:

- Pipe: `\\.\pipe\fcs.sock`
- Request: `08 81 20 1A 00` (protobuf `IpcCommand { cmdId = 4097 }`)
- Response framing: ASCII header + little-endian length + payload:
  - `FDG_PROTOCOL\n` + int32 length + payload
  - `FDG_CHUNK\n` + total + count + chunks
  - `FDG_COMPRESSED\n` is not implemented
- Payload: protobuf `IpcResult`
  - field 3: result code (0 = success)
  - field 5: `getConnectedDevices`
    - repeated field 1: `DeviceInfo`
      - field 1: product name
      - field 2: category (1 = gamepad)
      - field 3: connect type (1 = wired/charging, 2 = dongle, 3 = Bluetooth)
      - field 8: connected
      - field 9: battery level **0–6**

The plugin maps level `0..6` to an approximate percentage
`(level * 100 + 3) / 6` and keeps the raw `n/6` in the tooltip. The service is
single-client: while the Space Station App is open it owns the pipe; close the
App and the background service remains available.

For reference, the vendor USB info request is a 15-byte
`A5 10 00 ... B5` frame; a 32-byte reply starting with `A5 10` contains
`battery_raw` at byte 23. That path requires direct access to the
vendor-specific XInput interface and is not used by this plugin.

References (MIT):

- `wutong233/flydigi-battery` — Space Station pipe framing and protobuf fields.
- `vermi5/VaderBatteryTray` — Flydigi HID GET_INFO for Vader 5 Pro.
- `flydigi-protocol` crate — measured Direwolf 4 USB reports and the
  `A5 10` info request/reply.

## Feature report warning

Some Compx receivers expose a feature report `0x06` with values that look like
battery levels. Do not use it without verification: on several keyboards those
bytes are fixed firmware parameters and never change with the real battery.
