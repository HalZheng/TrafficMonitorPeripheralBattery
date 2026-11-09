using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace MouseKeyboardBattery
{
    internal static class Program
    {
        private static int Main(string[] args)
        {
            bool watch = false;
            bool featureWatch = false;
            bool redragon = false;
            bool probe = false;
            int seconds = 0;

            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "--watch")
                    watch = true;
                else if (args[i] == "--feature-watch")
                    featureWatch = true;
                else if (args[i] == "--redragon")
                    redragon = true;
                else if (args[i] == "--probe")
                    probe = true;
                else if (args[i] == "--seconds" && i + 1 < args.Length)
                {
                    int.TryParse(args[i + 1], out seconds);
                    i++;
                }
            }

            Console.WriteLine("MouseKeyboardBattery HID test");
            Console.WriteLine("G03 Pro   : {0:X4}:{1:X4} usage {2:X4}/{3:X4}",
                BatteryReaders.G03Vid, BatteryReaders.G03Pid,
                BatteryReaders.G03UsagePage, BatteryReaders.G03Usage);
            Console.WriteLine("2.4G combo: {0:X4}:{1:X4} usage {2:X4}/{3:X4}",
                BatteryReaders.GenericVid, BatteryReaders.GenericPid,
                BatteryReaders.GenericUsagePage, BatteryReaders.GenericUsage);
            DumpIdentity("G03", BatteryReaders.G03Vid, BatteryReaders.G03Pid, BatteryReaders.G03UsagePage, BatteryReaders.G03Usage);
            DumpIdentity("2.4G-FF02", BatteryReaders.GenericVid, BatteryReaders.GenericPid, BatteryReaders.GenericUsagePage, BatteryReaders.GenericUsage);
            DumpIdentity("2.4G-FF04", BatteryReaders.GenericVid, BatteryReaders.GenericPid, BatteryReaders.GenericFeatureUsagePage, 0x0002);
            Console.WriteLine();

            if (redragon)
            {
                Console.WriteLine("Redragon link: " + BatteryReaders.ReadRedragonLink());
                return 0;
            }

            if (probe)
            {
                byte[] commands = new byte[] { 0x07, 0x0A, 0x4A, 0x47, 0x05, 0x45, 0x06, 0x46, 0x08, 0x48 };
                foreach (string line in BatteryReaders.ProbeBycombo(commands))
                    Console.WriteLine(line);
                return 0;
            }

            if (featureWatch)
            {
                DateTime featureStart = DateTime.Now;
                while (true)
                {
                    Console.WriteLine(DateTime.Now.ToString("HH:mm:ss") + "  feature0x06: " +
                        BatteryReaders.ReadGenericFeature());
                    if (seconds > 0 && (DateTime.Now - featureStart).TotalSeconds >= seconds)
                        break;
                    Thread.Sleep(1000);
                }
                return 0;
            }

            DateTime start = DateTime.Now;
            while (true)
            {
                Console.WriteLine("=== " + DateTime.Now.ToString("HH:mm:ss") + " ===");

                try
                {
                    G03Reading g03 = BatteryReaders.ReadG03();
                    Console.WriteLine("G03 Pro    : " + g03);
                }
                catch (Exception ex)
                {
                    Console.WriteLine("G03 Pro    : 异常 " + ex.Message);
                }

                try
                {
                    GenericReading generic = BatteryReaders.ReadGeneric();
                    Console.WriteLine("2.4G combo : " + generic);
                }
                catch (Exception ex)
                {
                    Console.WriteLine("2.4G combo : 异常 " + ex.Message);
                }

                Console.WriteLine();
                if (!watch)
                    break;
                if (seconds > 0 && (DateTime.Now - start).TotalSeconds >= seconds)
                    break;
                Thread.Sleep(2000);
            }
            return 0;
        }

        private static void DumpIdentity(string tag, ushort vid, ushort pid, ushort usagePage, ushort usage)
        {
            using (HidDevice dev = HidDevice.Open(vid, pid, usagePage, usage))
            {
                if (dev == null)
                {
                    Console.WriteLine(tag + " identity: <interface not found>");
                    return;
                }
                Console.WriteLine(string.Format(
                    "{0} identity: product='{1}' manufacturer='{2}' in={3} out={4} feature={5}",
                    tag, dev.Product, dev.Manufacturer, dev.InputLength, dev.OutputLength, dev.FeatureLength));
                byte[] desc = dev.GetReportDescriptor();
                if (desc != null)
                    Console.WriteLine(tag + " desc: " + HexUtil.Hex(desc));
                else
                    Console.WriteLine(tag + " desc: <failed err=" + Marshal.GetLastWin32Error() + ">");
            }
        }
    }
}
