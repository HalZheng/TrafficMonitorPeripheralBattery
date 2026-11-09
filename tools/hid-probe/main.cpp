#include "core/HidTransport.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

namespace
{
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
        wprintf(L"  hid-probe --feature <vid> <pid> <usagePage> <usage> <reportId> [length]\n\n");
        wprintf(L"All numeric IDs are hexadecimal. --send pads the frame to the\n");
        wprintf(L"interface's output report length; the first byte is the report ID.\n\n");
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
        if (!device.Open(vid, pid, usagePage, usage))
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
        if (!device.Open(vid, pid, usagePage, usage))
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
        if (!device.Open(vid, pid, usagePage, usage))
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

    PrintUsage();
    return 1;
}
