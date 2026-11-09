using System;
using System.Runtime.InteropServices;
using System.Text;

namespace MouseKeyboardBattery
{
    internal static class NativeHid
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct SP_DEVICE_INTERFACE_DATA
        {
            public int cbSize;
            public Guid InterfaceClassGuid;
            public int Flags;
            public IntPtr Reserved;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct HIDD_ATTRIBUTES
        {
            public int Size;
            public ushort VendorID;
            public ushort ProductID;
            public ushort VersionNumber;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct HIDP_CAPS
        {
            public ushort Usage;
            public ushort UsagePage;
            public ushort InputReportByteLength;
            public ushort OutputReportByteLength;
            public ushort FeatureReportByteLength;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 17)]
            public ushort[] Reserved;
            public ushort NumberLinkCollectionNodes;
            public ushort NumberInputButtonCaps;
            public ushort NumberInputValueCaps;
            public ushort NumberInputDataIndices;
            public ushort NumberOutputButtonCaps;
            public ushort NumberOutputValueCaps;
            public ushort NumberOutputDataIndices;
            public ushort NumberFeatureButtonCaps;
            public ushort NumberFeatureValueCaps;
            public ushort NumberFeatureDataIndices;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct OVERLAPPED
        {
            public IntPtr Internal;
            public IntPtr InternalHigh;
            public int Offset;
            public int OffsetHigh;
            public IntPtr hEvent;
        }

        [DllImport("hid.dll")]
        public static extern void HidD_GetHidGuid(out Guid guid);
        [DllImport("hid.dll")]
        public static extern bool HidD_GetAttributes(IntPtr h, ref HIDD_ATTRIBUTES attr);
        [DllImport("hid.dll")]
        public static extern bool HidD_GetPreparsedData(IntPtr h, out IntPtr pp);
        [DllImport("hid.dll")]
        public static extern bool HidD_FreePreparsedData(IntPtr p);
        [DllImport("hid.dll")]
        public static extern int HidP_GetCaps(IntPtr p, ref HIDP_CAPS caps);
        [DllImport("hid.dll")]
        public static extern bool HidD_GetFeature(IntPtr h, byte[] buffer, int len);
        [DllImport("hid.dll", CharSet = CharSet.Unicode)]
        public static extern bool HidD_GetManufacturerString(IntPtr h, byte[] buffer, int len);
        [DllImport("hid.dll", CharSet = CharSet.Unicode)]
        public static extern bool HidD_GetProductString(IntPtr h, byte[] buffer, int len);

        [DllImport("setupapi.dll", CharSet = CharSet.Auto)]
        public static extern IntPtr SetupDiGetClassDevs(ref Guid g, IntPtr enumerator, IntPtr hwndParent, int flags);
        [DllImport("setupapi.dll", CharSet = CharSet.Auto)]
        public static extern bool SetupDiEnumDeviceInterfaces(IntPtr hDevInfo, IntPtr devInfo, ref Guid interfaceClassGuid, int memberIndex, ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData);
        [DllImport("setupapi.dll", CharSet = CharSet.Auto)]
        public static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr hDevInfo, ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData, IntPtr detail, int detailSize, out int requiredSize, IntPtr deviceInfoData);
        [DllImport("setupapi.dll")]
        public static extern bool SetupDiDestroyDeviceInfoList(IntPtr hDevInfo);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern IntPtr CreateFile(string name, uint access, uint share, IntPtr sec, uint disp, uint flags, IntPtr templ);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool CloseHandle(IntPtr h);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool WriteFile(IntPtr h, byte[] buf, int n, IntPtr written, ref OVERLAPPED ov);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool ReadFile(IntPtr h, byte[] buf, int n, IntPtr read, ref OVERLAPPED ov);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetOverlappedResult(IntPtr h, ref OVERLAPPED ov, out int transferred, bool wait);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool CancelIo(IntPtr h);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr CreateEvent(IntPtr sec, bool manual, bool initial, IntPtr name);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern uint WaitForSingleObject(IntPtr h, uint ms);
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool DeviceIoControl(IntPtr h, uint code, byte[] inBuf, int inSize, byte[] outBuf, int outSize, out int bytesReturned, IntPtr overlapped);

        public const uint IOCTL_HID_GET_REPORT_DESCRIPTOR = 0x000B0007;

        public const uint GENERIC_READ = 0x80000000;
        public const uint GENERIC_WRITE = 0x40000000;
        public const uint FILE_SHARE_READ = 1;
        public const uint FILE_SHARE_WRITE = 2;
        public const uint OPEN_EXISTING = 3;
        public const uint FILE_FLAG_OVERLAPPED = 0x40000000;
        public const int DIGCF_PRESENT = 0x02;
        public const int DIGCF_DEVICEINTERFACE = 0x10;
        public const int ERROR_IO_PENDING = 997;
    }

    public sealed class HidDevice : IDisposable
    {
        private IntPtr _handle = (IntPtr)(-1);

        public string Path { get; private set; }
        public ushort VendorId { get; private set; }
        public ushort ProductId { get; private set; }
        public ushort UsagePage { get; private set; }
        public ushort Usage { get; private set; }
        public int InputLength { get; private set; }
        public int OutputLength { get; private set; }
        public int FeatureLength { get; private set; }

        public string Manufacturer
        {
            get
            {
                if (_handle == (IntPtr)(-1))
                    return "";
                byte[] buf = new byte[254];
                if (!NativeHid.HidD_GetManufacturerString(_handle, buf, buf.Length))
                    return "";
                return Encoding.Unicode.GetString(buf).TrimEnd('\0');
            }
        }

        public string Product
        {
            get
            {
                if (_handle == (IntPtr)(-1))
                    return "";
                byte[] buf = new byte[254];
                if (!NativeHid.HidD_GetProductString(_handle, buf, buf.Length))
                    return "";
                return Encoding.Unicode.GetString(buf).TrimEnd('\0');
            }
        }

        public static HidDevice Open(ushort vid, ushort pid, ushort usagePage, ushort usage)
        {
            return Open(vid, pid, usagePage, usage, true);
        }

        public static HidDevice Open(ushort vid, ushort pid, ushort usagePage, ushort usage, bool overlapped)
        {
            Guid hidGuid;
            NativeHid.HidD_GetHidGuid(out hidGuid);
            IntPtr hDevInfo = NativeHid.SetupDiGetClassDevs(ref hidGuid, IntPtr.Zero, IntPtr.Zero,
                NativeHid.DIGCF_PRESENT | NativeHid.DIGCF_DEVICEINTERFACE);
            if (hDevInfo == (IntPtr)(-1))
                return null;

            NativeHid.SP_DEVICE_INTERFACE_DATA did = new NativeHid.SP_DEVICE_INTERFACE_DATA();
            did.cbSize = Marshal.SizeOf(typeof(NativeHid.SP_DEVICE_INTERFACE_DATA));
            int index = 0;
            try
            {
                while (NativeHid.SetupDiEnumDeviceInterfaces(hDevInfo, IntPtr.Zero, ref hidGuid, index, ref did))
                {
                    index++;
                    int required;
                    IntPtr buf = Marshal.AllocHGlobal(2048);
                    string path = null;
                    try
                    {
                        Marshal.WriteInt32(buf, IntPtr.Size == 8 ? 8 : 6);
                        bool got = NativeHid.SetupDiGetDeviceInterfaceDetail(
                            hDevInfo, ref did, buf, 2048, out required, IntPtr.Zero);
                        if (got)
                            path = Marshal.PtrToStringUni(new IntPtr(buf.ToInt64() + 4));
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(buf);
                    }
                    if (string.IsNullOrEmpty(path))
                        continue;

                    IntPtr h = NativeHid.CreateFile(path,
                        NativeHid.GENERIC_READ | NativeHid.GENERIC_WRITE,
                        NativeHid.FILE_SHARE_READ | NativeHid.FILE_SHARE_WRITE,
                        IntPtr.Zero, NativeHid.OPEN_EXISTING,
                        overlapped ? NativeHid.FILE_FLAG_OVERLAPPED : 0, IntPtr.Zero);
                    if (h == (IntPtr)(-1))
                    {
                        // Feature-only collections (e.g. the 0xFF04 battery report) may reject write access.
                        h = NativeHid.CreateFile(path, 0,
                            NativeHid.FILE_SHARE_READ | NativeHid.FILE_SHARE_WRITE,
                            IntPtr.Zero, NativeHid.OPEN_EXISTING,
                            overlapped ? NativeHid.FILE_FLAG_OVERLAPPED : 0, IntPtr.Zero);
                    }
                    if (h == (IntPtr)(-1))
                        continue;

                    try
                    {
                        NativeHid.HIDD_ATTRIBUTES attr = new NativeHid.HIDD_ATTRIBUTES();
                        attr.Size = Marshal.SizeOf(typeof(NativeHid.HIDD_ATTRIBUTES));
                        if (!NativeHid.HidD_GetAttributes(h, ref attr) ||
                            attr.VendorID != vid || attr.ProductID != pid)
                            continue;

                        IntPtr pp;
                        if (!NativeHid.HidD_GetPreparsedData(h, out pp))
                            continue;
                        NativeHid.HIDP_CAPS caps = new NativeHid.HIDP_CAPS();
                        int status;
                        try
                        {
                            status = NativeHid.HidP_GetCaps(pp, ref caps);
                        }
                        finally
                        {
                            NativeHid.HidD_FreePreparsedData(pp);
                        }
                        if (status < 0 || caps.UsagePage != usagePage || caps.Usage != usage)
                            continue;

                        HidDevice dev = new HidDevice();
                        dev._handle = h;
                        h = (IntPtr)(-1);
                        dev.Path = path;
                        dev.VendorId = vid;
                        dev.ProductId = pid;
                        dev.UsagePage = usagePage;
                        dev.Usage = usage;
                        dev.InputLength = caps.InputReportByteLength;
                        dev.OutputLength = caps.OutputReportByteLength;
                        dev.FeatureLength = caps.FeatureReportByteLength;
                        return dev;
                    }
                    finally
                    {
                        if (h != (IntPtr)(-1))
                            NativeHid.CloseHandle(h);
                    }
                }
            }
            finally
            {
                NativeHid.SetupDiDestroyDeviceInfoList(hDevInfo);
            }
            return null;
        }

        public bool Write(byte[] data, int timeoutMs)
        {
            if (_handle == (IntPtr)(-1) || data == null || data.Length != OutputLength)
                return false;

            NativeHid.OVERLAPPED ov = new NativeHid.OVERLAPPED();
            ov.hEvent = NativeHid.CreateEvent(IntPtr.Zero, true, false, IntPtr.Zero);
            try
            {
                int written = 0;
                bool ok = NativeHid.WriteFile(_handle, data, data.Length, IntPtr.Zero, ref ov);
                if (!ok)
                {
                    int err = Marshal.GetLastWin32Error();
                    if (err != NativeHid.ERROR_IO_PENDING)
                        return false;
                    if (NativeHid.WaitForSingleObject(ov.hEvent, (uint)timeoutMs) != 0)
                    {
                        NativeHid.CancelIo(_handle);
                        return false;
                    }
                    if (!NativeHid.GetOverlappedResult(_handle, ref ov, out written, false))
                        return false;
                }
                return true;
            }
            finally
            {
                NativeHid.CloseHandle(ov.hEvent);
            }
        }

        public byte[] Read(int timeoutMs)
        {
            if (_handle == (IntPtr)(-1) || InputLength <= 0)
                return null;

            byte[] buf = new byte[InputLength];
            NativeHid.OVERLAPPED ov = new NativeHid.OVERLAPPED();
            ov.hEvent = NativeHid.CreateEvent(IntPtr.Zero, true, false, IntPtr.Zero);
            try
            {
                int read = 0;
                bool ok = NativeHid.ReadFile(_handle, buf, buf.Length, IntPtr.Zero, ref ov);
                if (!ok)
                {
                    int err = Marshal.GetLastWin32Error();
                    if (err != NativeHid.ERROR_IO_PENDING)
                        return null;
                    if (NativeHid.WaitForSingleObject(ov.hEvent, (uint)timeoutMs) != 0)
                    {
                        NativeHid.CancelIo(_handle);
                        NativeHid.GetOverlappedResult(_handle, ref ov, out read, true);
                        return null;
                    }
                    if (!NativeHid.GetOverlappedResult(_handle, ref ov, out read, false))
                        return null;
                }
                return buf;
            }
            finally
            {
                NativeHid.CloseHandle(ov.hEvent);
            }
        }

        public byte[] GetFeature(int length)
        {
            if (_handle == (IntPtr)(-1) || length <= 0)
                return null;
            byte[] buf = new byte[length];
            if (!NativeHid.HidD_GetFeature(_handle, buf, buf.Length))
                return null;
            return buf;
        }

        public bool GetFeature(byte[] buffer)
        {
            if (_handle == (IntPtr)(-1) || buffer == null || buffer.Length == 0)
                return false;
            return NativeHid.HidD_GetFeature(_handle, buffer, buffer.Length);
        }

        public byte[] GetReportDescriptor()
        {
            if (_handle == (IntPtr)(-1))
                return null;
            byte[] buf = new byte[4096];
            int returned;
            if (!NativeHid.DeviceIoControl(_handle, NativeHid.IOCTL_HID_GET_REPORT_DESCRIPTOR,
                null, 0, buf, buf.Length, out returned, IntPtr.Zero))
                return null;
            if (returned <= 0)
                return null;
            byte[] result = new byte[returned];
            Array.Copy(buf, result, returned);
            return result;
        }

        public void Dispose()
        {
            if (_handle != (IntPtr)(-1))
            {
                NativeHid.CloseHandle(_handle);
                _handle = (IntPtr)(-1);
            }
        }
    }

    public static class HexUtil
    {
        public static string Hex(byte[] data)
        {
            if (data == null)
                return "";
            StringBuilder sb = new StringBuilder(data.Length * 3);
            for (int i = 0; i < data.Length; i++)
            {
                if (i > 0)
                    sb.Append(' ');
                sb.Append(data[i].ToString("X2"));
            }
            return sb.ToString();
        }
    }
}
