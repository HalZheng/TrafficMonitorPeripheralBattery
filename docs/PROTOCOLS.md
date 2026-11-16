# Protocols

All protocols here are read-only. They were derived from vendor drivers or
public open-source implementations and verified on real hardware where marked.

## Summary

| Protocol ID | Device | VID:PID | Usage | Request | Reply | Battery |
|---|---|---|---|---|---|---|
| `juxing` | AUSDOM G03 Pro 8K | `363C:FF99` | `FFA0/0002` | output report 11, `13 00 00` | `0A 13 xx 00 ...` | byte 18; charging byte 17 |
| `aula_bycombo` | iLovBee B87 | `3554:FA09` | `FF02/0002` | report `13`, command `05`, checksum | `13 05 01 00 0A ...` | byte 10 |
| `aula_bycombo` | BYCOMBO family | various | `FF02/0002` | report `13`, command `05` or `CA` | `13 4A ...` / `13 0A ...` | byte 5 / byte 6 |
| `compx_17` | ATK Dragonfly A9 Plus 2.0 | `373B:10C9` | `FF02/0002`, 17-byte collection | `08 04 ... 49` | `08 04 00 00 00 02 <pct> ...` | byte 6; charging byte 7; big-endian mV bytes 8–9 |
| `flydigi_pipe` | Flydigi Direwolf 4 / APEX | `045E:028E` (XInput) | 飞智空间站命名管道 | `08 81 20 1A 00` | protobuf `IpcResult` | 0–6 档 |

## Compx 17-byte (ATK Dragonfly A9 Plus 2.0)

- Receiver: `373B:10C9`, product string `NK mouse NANO dongle`.
- The receiver exposes **two** `FF02/0002` collections: a 20-byte one
  (`col04`) and a 17-byte one (`col05`). Only the 17-byte collection accepts
  the frame — writing 20 bytes to `col04` fails. The descriptor therefore
  sets `reportLength = 17` so `HidDevice::Open` picks the right collection.
- Input/output length: 17 bytes, report ID `0x08`.
- Request (17 bytes, everything else zero):

```text
08 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 49
^^ report ID                                  ^^ checksum
   ^^ command 0x04 = get battery
```

- Checksum: the sum of all 17 bytes, low 8 bits, must equal `0x55`.
- Live reply (75%, 3910 mV, discharging):

```text
08 04 00 00 00 02 4B 00 0F 46 00 00 00 00 00 00 A7
                  ^^ byte 6 = 0x4B = 75%
                     ^^ byte 7 = 0 discharging, non-zero charging
                        ^^^^^ byte 8-9 = 0x0F46 = 3910 mV, big endian
```

- byte 5 = `0x02` (2.4G mode), byte 6 = percentage `0..100`,
  byte 7 = charging flag, bytes 8–9 = pack voltage in mV (big endian).
  Voltage is only trusted when `2000 < mV < 5000`.
- The same collection carries unrelated frames that also start with
  `08 04`; the whole-frame checksum is mandatory, otherwise garbage values
  such as 116 / 123 / 127 leak through as percentages.
- Some firmware revisions answer with the descriptor's `0x09` INPUT report
  ID instead of `0x08`; the parser accepts both.
- Verified live on 2026-11-14: `75%`, `3910 mV`, discharging.
- The same frame family covers ATK F1/X1, VXE MAD R, VGN F1 Pro, Dareu and
  G-Wolves receivers. References (MIT): `435311382/Volt`,
  `GyroZeppel1/atk-battery`, `kun-qaq/wireless-battery-tray`.

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

Measured on 2026-11-14:

- `3554:FA09` (`FF04/0002`) returns `06 00 02 00 F5 00 5F 8E`, where byte 6
  is `0x5F` = 95 — while the same keyboard answered **84%** (`0x54`) over
  BYCOMBO at the same moment. Not a battery level.
- `25A7:FA70` (`FF02/0002`) returns `06 00 02 00 00 00 70 08`, byte-identical
  across repeated reads and after several `HidD_SetFeature` queries; only
  byte 1 echoes the last written value. It is a static config register, and
  `0x70` = 112 is outside the percentage range.

A value that never changes is not a battery reading.
