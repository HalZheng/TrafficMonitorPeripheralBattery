# Legacy C# protocol-discovery tools

These files were used during the initial reverse engineering of the AUSDOM G03
Pro and iLovBee B87 receivers. They are not part of the plugin build.

They remain useful because they can dump HID value capabilities and feature
reports that the C++ `hid-probe` does not yet expose. For new devices, prefer
`bin\<arch>\hid-probe.exe`; fall back to these tools only when you need
descriptor/value-cap inspection.

Build (optional):

```powershell
C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /target:exe /out:HidBatteryTest.exe src\*.cs
```

Files:

- `HidDevice.cs` — SetupAPI/HID P/Invoke, enumeration and overlapped I/O.
- `BatteryReaders.cs` — the first G03 Pro and BYCOMBO readers.
- `Program.cs` — `HidBatteryTest.exe` entry point with `--watch`,
  `--feature-watch`, `--redragon`, `--probe` modes.
