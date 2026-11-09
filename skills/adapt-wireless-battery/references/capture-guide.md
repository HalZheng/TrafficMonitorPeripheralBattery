# Capturing a wireless peripheral's HID protocol

This guide is intentionally vendor-neutral. Use the least invasive method that
produces a clear request/response pair.

## 1. Find the vendor collection

```powershell
bin\x64\hid-probe.exe --list
```

Look for a collection with:

- `usagePage` in the vendor range `0xFF00`-`0xFFFF`
- `usage` usually `0x0002`
- input/output/feature report lengths that match the vendor driver

Do not send vendor commands to the standard `0x0001` keyboard/mouse collection.

## 2. WebHID driver (easiest)

Open the vendor's web driver in Chrome or Edge. In DevTools Console, before
connecting the device:

```js
const originalSend = HIDDevice.prototype.sendReport;
HIDDevice.prototype.sendReport = function (reportId, data) {
  const bytes = Array.from(new Uint8Array(data))
    .map(b => b.toString(16).padStart(2, '0')).join(' ');
  console.log('DOWN id=' + reportId + ' ' + bytes);
  return originalSend.apply(this, arguments);
};
```

After the driver connects, attach an input listener to every granted device:

```js
navigator.hid.getDevices().then(devices => devices.forEach(device => {
  device.addEventListener('inputreport', event => {
    const bytes = Array.from(new Uint8Array(event.data.buffer))
      .map(b => b.toString(16).padStart(2, '0')).join(' ');
    console.log('UP id=' + event.reportId + ' ' + bytes);
  });
}));
```

Then click the battery/status action in the driver. Copy the DOWN/UP pairs.

## 3. Desktop driver + USB capture

Use USBPcap with Wireshark, or an equivalent HID monitor:

1. Start capture on the root hub that owns the receiver.
2. Filter by the receiver VID/PID, for example `usb.idVendor == 0x3554`.
3. Start the vendor driver and open the page that shows the battery.
4. Save the request and response hex.

If the driver only exposes a UI, take a screenshot of the battery value at the
same time as the capture. The value is needed to identify the battery byte.

## 4. Passive listening

Some receivers push battery frames periodically:

```powershell
bin\x64\hid-probe.exe --read 3554 FA09 FF02 0002 15
```

If nothing arrives, send the vendor's status/battery request first. A sleeping
device may need a keypress or mouse movement before it answers.

## 5. Verify the field

Do not stop at "the prefix matches". Verify at least one of:

- power the device off: the frame disappears or changes to a sleep sentinel;
- connect the charging cable: a charging flag or the percentage changes;
- wait for real discharge: the candidate byte decreases while other bytes stay
  stable;
- compare the candidate byte with the vendor UI's exact percentage.

## 6. Keep the capture

Store the raw request/response pair in the protocol report and in a unit-test
fixture. Never commit a capture that contains a MAC address, serial number, or
other personal identifier without redacting it.
