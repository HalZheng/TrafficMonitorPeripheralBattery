#include "core/HidTransport.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    // Optional global "--len N": pin the collection by report byte length when
    // a receiver exposes more than one collection with the same usage.
    unsigned short g_reportLength = 0;

    std::wstring HexString(const std::vector<unsigned char>& data)
    {
        std::wstring result;
        wchar_t buffer[4] = {};
        for (size_t i = 0; i < data.size(); ++i)
        {
            if (i > 0)
                result += L' ';
            swprintf_s(buffer, L"%02X", data[i]);
            result += buffer;
        }
        return result;
    }

    bool ParseHexBytes(const std::wstring& text, std::vector<unsigned char>& out)
    {
        out.clear();
        int high = -1;
        for (wchar_t ch : text)
        {
            if (ch == L'x' || ch == L'X')
            {
                // Cancel a leading 0 from a 0x prefix.
                if (high == 0)
                    high = -1;
                continue;
            }
            if (ch == L' ' || ch == L'\t' || ch == L',' || ch == L':' ||
                ch == L'-')
                continue;

            int value = -1;
            if (ch >= L'0' && ch <= L'9')
                value = ch - L'0';
            else if (ch >= L'a' && ch <= L'f')
                value = ch - L'a' + 10;
            else if (ch >= L'A' && ch <= L'F')
                value = ch - L'A' + 10;
            else
                return false;

            if (high < 0)
            {
                high = value;
            }
            else
            {
                out.push_back(static_cast<unsigned char>((high << 4) | value));
                high = -1;
            }
        }
        return high < 0;
    }

    unsigned short ParseHexNumber(const wchar_t* text)
    {
        return static_cast<unsigned short>(wcstoul(text, nullptr, 16));
    }

    std::wstring FormatInfo(const pb::HidInterfaceInfo& info)
    {
        wchar_t buffer[512] = {};
        swprintf_s(buffer, L"%04X:%04X  usage %04X/%04X  in=%u out=%u feat=%u  %s  [%s]",
            info.vid, info.pid, info.usagePage, info.usage,
            info.inputLength, info.outputLength, info.featureLength,
            info.product.c_str(), info.path.c_str());
        return buffer;
    }

    void PrintUsage()
    {
        wprintf(L"hid-probe - generic HID inspection tool\n\n");
        wprintf(L"  hid-probe --list\n");
        wprintf(L"  hid-probe --send <vid> <pid> <usagePage> <usage> <hex bytes> [seconds]\n");
        wprintf(L"  hid-probe --read <vid> <pid> <usagePage> <usage> [seconds]\n");
        wprintf(L"  hid-probe --feature <vid> <pid> <usagePage> <usage> <reportId> [length]\n");
        wprintf(L"  hid-probe --set-feature <vid> <pid> <usagePage> <usage> <hex bytes> [seconds]\n");
        wprintf(L"  hid-probe --get-input <vid> <pid> <usagePage> <usage> <reportId> [length]\n");
        wprintf(L"  hid-probe --exchange <vid> <pid> <featPage> <featUsage> <inPage> <inUsage> <hex bytes> <seconds>\n");
        wprintf(L"  hid-probe --listen-all <vid> <pid> <seconds>\n\n");
        wprintf(L"All numeric IDs are hexadecimal. --send pads the frame to the\n");
        wprintf(L"interface's output report length; the first byte is the report ID.\n");
        wprintf(L"Append --len <decimal bytes> to any mode to select one of several\n");
        wprintf(L"collections sharing the same usage (e.g. --len 17).\n\n");
        wprintf(L"Examples:\n");
        wprintf(L"  hid-probe --list\n");
        wprintf(L"  hid-probe --send 3554 FA09 FF02 0002 \"13 05 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 18\" 3\n");
        wprintf(L"  hid-probe --feature 3554 FA09 FF04 0002 06 8\n");
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    for (int i = 2; i + 1 < argc; ++i)
    {
        if (wcscmp(argv[i], L"--len") == 0)
            g_reportLength = static_cast<unsigned short>(_wtoi(argv[i + 1]));
    }

    std::wstring mode = argv[1];
    if (mode == L"--list")
    {
        std::vector<pb::HidInterfaceInfo> interfaces = pb::EnumerateHidInterfaces();
        std::sort(interfaces.begin(), interfaces.end(),
            [](const pb::HidInterfaceInfo& left, const pb::HidInterfaceInfo& right)
            {
                if (left.vid != right.vid) return left.vid < right.vid;
                if (left.pid != right.pid) return left.pid < right.pid;
                if (left.usagePage != right.usagePage) return left.usagePage < right.usagePage;
                return left.usage < right.usage;
            });

        for (const pb::HidInterfaceInfo& info : interfaces)
            wprintf(L"%s\n", FormatInfo(info).c_str());
        return 0;
    }

    if (mode == L"--send")
    {
        if (argc < 7)
        {
            PrintUsage();
            return 1;
        }

        unsigned short vid = ParseHexNumber(argv[2]);
        unsigned short pid = ParseHexNumber(argv[3]);
        unsigned short usagePage = ParseHexNumber(argv[4]);
        unsigned short usage = ParseHexNumber(argv[5]);

        pb::HidDevice device;
        if (!device.Open(vid, pid, usagePage, usage, g_reportLength))
        {
            wprintf(L"interface not found\n");
            return 2;
        }

        wprintf(L"%s\n", FormatInfo(device.Info()).c_str());
        std::vector<unsigned char> frame;
        if (!ParseHexBytes(argv[6], frame))
        {
            wprintf(L"invalid hex data\n");
            return 3;
        }
        if (frame.size() > device.OutputLength())
        {
            wprintf(L"frame is longer than output report length (%u)\n",
                device.OutputLength());
            return 4;
        }
        frame.resize(device.OutputLength(), 0);

        int seconds = argc > 7 ? _wtoi(argv[7]) : 2;
        if (!device.Write(frame, 1000))
        {
            wprintf(L"write failed\n");
            return 5;
        }
        wprintf(L"write ok: %s\n", HexString(frame).c_str());

        ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
        while (GetTickCount64() < end)
        {
            std::vector<unsigned char> response;
            if (!device.Read(response, 250))
                continue;
            wprintf(L"IN: %s\n", HexString(response).c_str());
        }
        return 0;
    }

    if (mode == L"--read")
    {
        if (argc < 6)
        {
            PrintUsage();
            return 1;
        }

        unsigned short vid = ParseHexNumber(argv[2]);
        unsigned short pid = ParseHexNumber(argv[3]);
        unsigned short usagePage = ParseHexNumber(argv[4]);
        unsigned short usage = ParseHexNumber(argv[5]);

        pb::HidDevice device;
        if (!device.Open(vid, pid, usagePage, usage, g_reportLength))
        {
            wprintf(L"interface not found\n");
            return 2;
        }

        wprintf(L"%s\n", FormatInfo(device.Info()).c_str());
        int seconds = argc > 6 ? _wtoi(argv[6]) : 5;
        ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
        while (GetTickCount64() < end)
        {
            std::vector<unsigned char> response;
            if (!device.Read(response, 250))
                continue;
            wprintf(L"IN: %s\n", HexString(response).c_str());
        }
        return 0;
    }

    if (mode == L"--feature")
    {
        if (argc < 7)
        {
            PrintUsage();
            return 1;
        }

        unsigned short vid = ParseHexNumber(argv[2]);
        unsigned short pid = ParseHexNumber(argv[3]);
        unsigned short usagePage = ParseHexNumber(argv[4]);
        unsigned short usage = ParseHexNumber(argv[5]);
        int reportId = static_cast<int>(wcstoul(argv[6], nullptr, 16));

        pb::HidDevice device;
        if (!device.Open(vid, pid, usagePage, usage, g_reportLength))
        {
            wprintf(L"interface not found\n");
            return 2;
        }

        wprintf(L"%s\n", FormatInfo(device.Info()).c_str());
        int length = argc > 7 ? _wtoi(argv[7]) : device.FeatureLength();
        if (length <= 0)
        {
            wprintf(L"interface has no feature report\n");
            return 3;
        }

        std::vector<unsigned char> data(length, 0);
        data[0] = static_cast<unsigned char>(reportId);
        if (!device.GetFeature(data))
        {
            wprintf(L"get feature failed\n");
            return 4;
        }
        wprintf(L"FEATURE: %s\n", HexString(data).c_str());
        return 0;
    }

    if (mode == L"--set-feature")
    {
        if (argc < 7)
        {
            PrintUsage();
            return 1;
        }

        unsigned short vid = ParseHexNumber(argv[2]);
        unsigned short pid = ParseHexNumber(argv[3]);
        unsigned short usagePage = ParseHexNumber(argv[4]);
        unsigned short usage = ParseHexNumber(argv[5]);

        pb::HidDevice device;
        if (!device.Open(vid, pid, usagePage, usage, g_reportLength))
        {
            wprintf(L"interface not found\n");
            return 2;
        }

        wprintf(L"%s\n", FormatInfo(device.Info()).c_str());
        std::vector<unsigned char> frame;
        if (!ParseHexBytes(argv[6], frame))
        {
            wprintf(L"invalid hex data\n");
            return 3;
        }
        if (device.FeatureLength() == 0 ||
            frame.size() > device.FeatureLength())
        {
            wprintf(L"frame is longer than feature report length (%u)\n",
                device.FeatureLength());
            return 4;
        }
        frame.resize(device.FeatureLength(), 0);

        if (!device.SetFeature(frame))
        {
            wprintf(L"set feature failed\n");
            return 5;
        }
        wprintf(L"set feature ok: %s\n", HexString(frame).c_str());

        int seconds = argc > 7 ? _wtoi(argv[7]) : 2;
        ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
        while (GetTickCount64() < end)
        {
            std::vector<unsigned char> response;
            if (!device.Read(response, 250))
                continue;
            wprintf(L"IN: %s\n", HexString(response).c_str());
        }
        return 0;
    }

    if (mode == L"--get-input")
    {
        if (argc < 7)
        {
            PrintUsage();
            return 1;
        }

        unsigned short vid = ParseHexNumber(argv[2]);
        unsigned short pid = ParseHexNumber(argv[3]);
        unsigned short usagePage = ParseHexNumber(argv[4]);
        unsigned short usage = ParseHexNumber(argv[5]);
        int reportId = static_cast<int>(wcstoul(argv[6], nullptr, 16));

        pb::HidDevice device;
        if (!device.Open(vid, pid, usagePage, usage, g_reportLength))
        {
            wprintf(L"interface not found\n");
            return 2;
        }

        wprintf(L"%s\n", FormatInfo(device.Info()).c_str());
        int length = argc > 7 ? _wtoi(argv[7]) : device.InputLength();
        if (length <= 0)
        {
            wprintf(L"interface has no input report\n");
            return 3;
        }

        std::vector<unsigned char> data(length, 0);
        data[0] = static_cast<unsigned char>(reportId);
        if (!device.GetInputReport(data))
        {
            wprintf(L"get input report failed\n");
            return 4;
        }
        wprintf(L"INPUT: %s\n", HexString(data).c_str());
        return 0;
    }

    if (mode == L"--exchange")
    {
        // Some receivers reply to a FEATURE query with an INPUT report on a
        // different collection. Keep both handles open in this process so the
        // reply cannot be consumed by a separate listener.
        if (argc < 10)
        {
            PrintUsage();
            return 1;
        }

        unsigned short vid = ParseHexNumber(argv[2]);
        unsigned short pid = ParseHexNumber(argv[3]);
        unsigned short featPage = ParseHexNumber(argv[4]);
        unsigned short featUsage = ParseHexNumber(argv[5]);
        unsigned short inPage = ParseHexNumber(argv[6]);
        unsigned short inUsage = ParseHexNumber(argv[7]);

        pb::HidDevice featureDevice;
        pb::HidDevice inputDevice;
        if (!featureDevice.Open(vid, pid, featPage, featUsage, 0))
        {
            wprintf(L"feature interface not found\n");
            return 2;
        }
        if (!inputDevice.Open(vid, pid, inPage, inUsage, 0))
        {
            wprintf(L"input interface not found\n");
            return 2;
        }

        wprintf(L"FEATURE: %s\n", FormatInfo(featureDevice.Info()).c_str());
        wprintf(L"INPUT  : %s\n", FormatInfo(inputDevice.Info()).c_str());

        std::vector<unsigned char> frame;
        if (!ParseHexBytes(argv[8], frame))
        {
            wprintf(L"invalid hex data\n");
            return 3;
        }
        if (featureDevice.FeatureLength() == 0 ||
            frame.size() > featureDevice.FeatureLength())
        {
            wprintf(L"frame is longer than feature report length (%u)\n",
                featureDevice.FeatureLength());
            return 4;
        }
        frame.resize(featureDevice.FeatureLength(), 0);

        int seconds = _wtoi(argv[9]);
        if (seconds < 1)
            seconds = 1;

        std::vector<unsigned char> before(frame);
        if (featureDevice.GetFeature(before))
            wprintf(L"GET before: %s\n", HexString(before).c_str());

        std::vector<unsigned char> junk;
        while (inputDevice.Read(junk, 60))
            wprintf(L"DRAIN: %s\n", HexString(junk).c_str());

        if (!featureDevice.SetFeature(frame))
        {
            wprintf(L"set feature failed\n");
            return 5;
        }
        wprintf(L"SET: %s\n", HexString(frame).c_str());

        std::vector<unsigned char> after(frame);
        if (featureDevice.GetFeature(after))
            wprintf(L"GET after : %s\n", HexString(after).c_str());

        ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
        while (GetTickCount64() < end)
        {
            std::vector<unsigned char> response;
            if (!inputDevice.Read(response, 250))
                continue;
            wprintf(L"IN: %s\n", HexString(response).c_str());
        }
        return 0;
    }

    if (mode == L"--listen-all")
    {
        // Listen on every input collection of one receiver at once. Useful
        // when it is unclear which collection carries unsolicited frames.
        if (argc < 5)
        {
            PrintUsage();
            return 1;
        }

        unsigned short vid = ParseHexNumber(argv[2]);
        unsigned short pid = ParseHexNumber(argv[3]);
        int seconds = _wtoi(argv[4]);
        if (seconds < 1)
            seconds = 1;

        std::vector<pb::HidInterfaceInfo> targets;
        for (const pb::HidInterfaceInfo& info : pb::EnumerateHidInterfaces())
        {
            if (info.vid == vid && info.pid == pid && info.inputLength > 0)
                targets.push_back(info);
        }

        wprintf(L"listening on %u input collections for %d s\n",
            static_cast<unsigned int>(targets.size()), seconds);
        const ULONGLONG deadline =
            GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
        std::mutex printMutex;
        std::vector<std::thread> threads;

        for (const pb::HidInterfaceInfo& info : targets)
        {
            threads.emplace_back([info, deadline, &printMutex]()
            {
                pb::HidDevice device;
                if (!device.OpenPath(info.path, true))
                {
                    std::lock_guard<std::mutex> lock(printMutex);
                    wprintf(L"OPEN FAILED %04X/%04X\n",
                        info.usagePage, info.usage);
                    return;
                }

                while (GetTickCount64() < deadline)
                {
                    std::vector<unsigned char> data;
                    if (!device.Read(data, 200))
                        continue;
                    std::lock_guard<std::mutex> lock(printMutex);
                    wprintf(L"IN %04X/%04X len=%u  %s\n",
                        info.usagePage, info.usage,
                        static_cast<unsigned int>(data.size()),
                        HexString(data).c_str());
                }
            });
        }

        for (std::thread& thread : threads)
        {
            if (thread.joinable())
                thread.join();
        }
        return 0;
    }

    PrintUsage();
    return 1;
}
