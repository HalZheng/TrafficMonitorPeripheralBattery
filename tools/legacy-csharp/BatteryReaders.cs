using System;
using System.Collections.Generic;
using System.Text;

namespace MouseKeyboardBattery
{
    public sealed class G03Reading
    {
        public bool Found;
        public bool Online;
        public int Percent = -1;
        public bool Charging;
        public bool Wireless;
        public string Raw = "";
        public string Note = "";

        public override string ToString()
        {
            if (!Found)
                return "未找到设备 (" + Note + ")";
            if (!Online)
                return "接收器已找到，但设备无应答 (" + Note + ")";
            return string.Format("{0}%{1}{2}  raw=[{3}]",
                Percent,
                Charging ? " 充电中" : "",
                Wireless ? " 无线" : "",
                Raw);
        }
    }

    public sealed class GenericReading
    {
        public bool Found;
        public bool Online;
        public int Percent = -1;
        public int Status;
        public string FrameType = "";
        public string FeatureRaw = "";
        public string FeatureNote = "";
        public List<string> RawPackets = new List<string>();
        public string Note = "";

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();
            if (!Found)
                return "未找到设备 (" + Note + ")";
            if (Online)
                sb.AppendFormat("{0}% (帧 {1}, 状态 0x{2:X2})", Percent, FrameType, Status);
            else
                sb.Append("接收器已找到，但没有收到电量帧");
            if (!string.IsNullOrEmpty(FeatureRaw))
                sb.AppendFormat("  feature0x06=[{0}]", FeatureRaw);
            if (!string.IsNullOrEmpty(FeatureNote))
                sb.AppendFormat("  feature: {0}", FeatureNote);
            if (RawPackets.Count > 0)
            {
                sb.Append("  packets=");
                for (int i = 0; i < RawPackets.Count; i++)
                {
                    if (i > 0)
                        sb.Append(" | ");
                    sb.Append(RawPackets[i]);
                }
            }
            return sb.ToString();
        }
    }

    public static class BatteryReaders
    {
        public const ushort G03Vid = 0x363C;
        public const ushort G03Pid = 0xFF99;
        public const ushort G03UsagePage = 0xFFA0;
        public const ushort G03Usage = 0x0002;

        public const ushort GenericVid = 0x3554;
        public const ushort GenericPid = 0xFA09;
        public const ushort GenericUsagePage = 0xFF02;
        public const ushort GenericUsage = 0x0002;
        public const ushort GenericFeatureUsagePage = 0xFF04;

        public static G03Reading ReadG03()
        {
            G03Reading result = new G03Reading();
            using (HidDevice dev = HidDevice.Open(G03Vid, G03Pid, G03UsagePage, G03Usage))
            {
                if (dev == null)
                {
                    result.Note = "HID 接口未枚举到";
                    return result;
                }
                result.Found = true;

                // AUSDOM JUXING: output report 11, command 0x13 asks for device status.
                byte[] req = new byte[dev.OutputLength];
                req[0] = 0x0B;
                req[1] = 0x13;
                req[2] = 0x00;
                req[3] = 0x00;

                for (int attempt = 0; attempt < 3; attempt++)
                {
                    if (!dev.Write(req, 1000))
                        continue;

                    DateTime end = DateTime.Now.AddMilliseconds(1200);
                    while (DateTime.Now < end)
                    {
                        byte[] resp = dev.Read(300);
                        if (resp == null)
                            continue;
                        result.Raw = HexUtil.Hex(resp);
                        // 0A 13 xx 00 ... [17]=charging [18]=percent
                        if (resp.Length > 19 && resp[0] == 0x0A && resp[1] == 0x13 && resp[3] == 0x00)
                        {
                            result.Online = true;
                            result.Percent = resp[18];
                            result.Charging = resp[17] != 0;
                            result.Wireless = resp[19] != 0;
                            result.Note = "";
                            return result;
                        }
                    }
                }
                result.Note = "3 次查询无匹配回包";
                return result;
            }
        }

        public static GenericReading ReadGeneric()
        {
            GenericReading result = new GenericReading();
            bool anyInterface = false;

            using (HidDevice dev = HidDevice.Open(GenericVid, GenericPid, GenericUsagePage, GenericUsage))
            {
                if (dev != null)
                {
                    anyInterface = true;
                    result.Found = true;

                    // AULA / Compx 20-byte frame: 13 CA 00... checksum.
                    byte[] req = MakeAulaFrame(0xCA);
                    for (int attempt = 0; attempt < 3 && !result.Online; attempt++)
                    {
                        if (!dev.Write(req, 1000))
                            continue;

                        DateTime end = DateTime.Now.AddMilliseconds(1500);
                        while (DateTime.Now < end)
                        {
                            byte[] resp = dev.Read(300);
                            if (resp == null)
                                continue;
                            result.RawPackets.Add(HexUtil.Hex(resp));
                            if (TryParseAulaFrame(resp, result))
                                break;
                        }
                    }
                }
            }

            if (!result.Online)
            {
                using (HidDevice feat = HidDevice.Open(GenericVid, GenericPid, GenericFeatureUsagePage, 0x0002, false))
                {
                    if (feat != null)
                    {
                        anyInterface = true;
                        result.Found = true;
                        int len = feat.FeatureLength > 0 ? feat.FeatureLength : 8;
                        byte[] data = new byte[len];
                        data[0] = 0x06;
                        if (feat.GetFeature(data))
                            result.FeatureRaw = HexUtil.Hex(data);
                        else
                            result.FeatureNote = "feature report 读取失败";
                    }
                    else
                    {
                        result.FeatureNote = "FF04/0002 接口未打开";
                    }
                }
            }

            if (!result.Found)
                result.Note = "HID 接口未枚举到";
            else if (!anyInterface)
                result.Note = "没有可用的厂商接口";
            return result;
        }

        public static string ReadGenericFeature()
        {
            using (HidDevice feat = HidDevice.Open(GenericVid, GenericPid, GenericFeatureUsagePage, 0x0002, false))
            {
                if (feat == null)
                    return "接口未找到";
                int len = feat.FeatureLength > 0 ? feat.FeatureLength : 8;
                byte[] data = new byte[len];
                data[0] = 0x06;
                if (!feat.GetFeature(data))
                    return "读取失败";
                return HexUtil.Hex(data);
            }
        }

        public static string ReadRedragonLink()
        {
            using (HidDevice dev = HidDevice.Open(GenericVid, GenericPid, GenericUsagePage, GenericUsage))
            {
                if (dev == null)
                    return "接口未找到";

                // BYCOMBO / "3632" 20-byte frame: 13 07 01 00 ... checksum
                byte[] req = new byte[dev.OutputLength];
                req[0] = 0x13;
                req[1] = 0x07;
                req[2] = 0x01;
                int sum = 0;
                for (int i = 0; i < 19; i++)
                    sum += req[i];
                req[19] = (byte)(sum & 0xFF);

                for (int attempt = 0; attempt < 3; attempt++)
                {
                    if (!dev.Write(req, 1000))
                        continue;
                    DateTime end = DateTime.Now.AddMilliseconds(800);
                    while (DateTime.Now < end)
                    {
                        byte[] resp = dev.Read(250);
                        if (resp == null)
                            continue;
                        if (resp.Length >= 6 && resp[0] == 0x13 && resp[1] == 0x07)
                            return string.Format("link={0} raw=[{1}]",
                                resp[5] == 1 ? "已连接" : "未连接(0x" + resp[5].ToString("X2") + ")",
                                HexUtil.Hex(resp));
                    }
                }
                return "无应答";
            }
        }

        public static List<string> ProbeBycombo(byte[] commands)
        {
            List<string> results = new List<string>();
            using (HidDevice dev = HidDevice.Open(GenericVid, GenericPid, GenericUsagePage, GenericUsage))
            {
                if (dev == null)
                {
                    results.Add("接口未找到");
                    return results;
                }

                for (int c = 0; c < commands.Length; c++)
                {
                    byte cmd = commands[c];
                    byte[] req = new byte[dev.OutputLength];
                    req[0] = 0x13;
                    req[1] = cmd;
                    req[2] = 0x01;
                    int sum = 0;
                    for (int i = 0; i < 19; i++)
                        sum += req[i];
                    req[19] = (byte)(sum & 0xFF);

                    bool got = false;
                    for (int attempt = 0; attempt < 2 && !got; attempt++)
                    {
                        if (!dev.Write(req, 1000))
                            continue;
                        DateTime end = DateTime.Now.AddMilliseconds(700);
                        while (DateTime.Now < end)
                        {
                            byte[] resp = dev.Read(200);
                            if (resp == null)
                                continue;
                            if (resp.Length > 1 && resp[0] == 0x13 && resp[1] == cmd)
                            {
                                results.Add(string.Format("cmd 0x{0:X2} -> {1}", cmd, HexUtil.Hex(resp)));
                                got = true;
                                break;
                            }
                        }
                    }
                    if (!got)
                        results.Add(string.Format("cmd 0x{0:X2} -> 无应答", cmd));
                }
            }
            return results;
        }

        private static byte[] MakeAulaFrame(byte cmd)
        {
            byte[] req = new byte[20];
            req[0] = 0x13;
            req[1] = cmd;
            int sum = 0;
            for (int i = 0; i < 19; i++)
                sum += req[i];
            req[19] = (byte)(sum & 0xFF);
            return req;
        }

        private static bool TryParseAulaFrame(byte[] resp, GenericReading result)
        {
            if (resp == null || resp.Length < 8 || resp[0] != 0x13)
                return false;

            // 13 4A 01 00 02 <pct> <status> ...
            if (resp[1] == 0x4A && resp[2] == 0x01 && resp[3] == 0x00 && resp[4] == 0x02)
            {
                result.Percent = resp[5];
                result.Status = resp[6];
                result.FrameType = "4A";
                result.Online = result.Percent >= 1 && result.Percent <= 100;
                return result.Online;
            }

            // 13 0A 01 00 04 05 <pct> <status> ...
            if (resp[1] == 0x0A && resp[2] == 0x01 && resp[3] == 0x00 && resp[4] == 0x04 && resp[5] == 0x05)
            {
                result.Percent = resp[6];
                result.Status = resp[7];
                result.FrameType = "0A";
                result.Online = result.Percent >= 1 && result.Percent <= 100;
                return result.Online;
            }

            return false;
        }
    }
}
