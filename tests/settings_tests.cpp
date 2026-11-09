#include "plugin/Settings.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void Check(bool condition, const char* message)
    {
        if (condition)
        {
            std::printf("ok   %s\n", message);
        }
        else
        {
            std::printf("FAIL %s\n", message);
            ++failures;
        }
    }

    std::wstring ReadText(const std::wstring& path)
    {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return L"";

        LARGE_INTEGER size = {};
        std::vector<char> bytes;
        if (GetFileSizeEx(file, &size) && size.QuadPart > 0)
        {
            bytes.resize(static_cast<size_t>(size.QuadPart));
            DWORD read = 0;
            if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                &read, nullptr))
                bytes.clear();
            else
                bytes.resize(read);
        }
        CloseHandle(file);

        // WritePrivateProfileStringW creates a UTF-16LE file with a BOM.
        if (bytes.size() >= 2 &&
            static_cast<unsigned char>(bytes[0]) == 0xFF &&
            static_cast<unsigned char>(bytes[1]) == 0xFE)
        {
            size_t count = (bytes.size() - 2) / sizeof(wchar_t);
            std::wstring text(count, L'\0');
            if (count > 0)
                std::memcpy(&text[0], bytes.data() + 2, count * sizeof(wchar_t));
            return text;
        }

        const char* data = bytes.data();
        size_t length = bytes.size();
        if (length >= 3 &&
            static_cast<unsigned char>(data[0]) == 0xEF &&
            static_cast<unsigned char>(data[1]) == 0xBB &&
            static_cast<unsigned char>(data[2]) == 0xBF)
        {
            data += 3;
            length -= 3;
        }

        int wideLength = MultiByteToWideChar(CP_UTF8, 0, data,
            static_cast<int>(length), nullptr, 0);
        if (wideLength <= 0)
            return L"";
        std::wstring text(static_cast<size_t>(wideLength), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length),
            &text[0], wideLength);
        return text;
    }

    int CountOccurrences(const std::wstring& text, const std::wstring& needle)
    {
        if (needle.empty())
            return 0;

        int count = 0;
        size_t position = 0;
        while ((position = text.find(needle, position)) != std::wstring::npos)
        {
            ++count;
            position += needle.size();
        }
        return count;
    }

    bool FilePresent(const std::wstring& path)
    {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
}

int main()
{
    using namespace pb;

    wchar_t tempPath[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, tempPath) == 0)
    {
        std::printf("FAIL cannot resolve temp directory\n");
        return 1;
    }

    std::wstring directory = std::wstring(tempPath) +
        L"PeripheralBatterySettingsTest-" + std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(directory.c_str(), nullptr);
    std::wstring file = directory + L"\\settings.ini";
    DeleteFileW(file.c_str());

    DeviceRegistry registry;
    Settings settings;

    settings.Load(file, registry);
    Check(settings.refreshIntervalSeconds == 30, "missing file keeps default interval");
    Check(settings.maxDevices == 4, "missing file keeps default max devices");
    Check(settings.displayMode == L"bar", "missing file keeps bar display mode");
    Check(settings.devices.size() == registry.All().size(),
        "defaults cover every registered device");
    Check(settings.ForDevice(L"g03_pro").label == L"G03",
        "g03_pro default label");
    Check(settings.ForDevice(L"ilovbee_b87").label == L"B87",
        "ilovbee_b87 default label");

    settings.refreshIntervalSeconds = 12;
    settings.maxDevices = 2;
    settings.showOffline = false;
    settings.displayMode = L"text";
    DeviceSettings& g03 = settings.devices[L"g03_pro"];
    g03.enabled = false;
    g03.name = L"AUSDOM Mouse";
    g03.label = L"G3";
    g03.priority = 5;
    g03.color = L"#112233";

    settings.Save(file, registry);
    Check(FilePresent(file), "save creates the ini file");

    std::wstring text = ReadText(file);
    Check(CountOccurrences(text, L"[plugin]") == 1,
        "one plugin section after save");
    Check(CountOccurrences(text, L"[device:g03_pro]") == 1,
        "one g03_pro section after save");

    Settings reloaded;
    reloaded.Load(file, registry);
    Check(reloaded.refreshIntervalSeconds == 12, "interval round-trips");
    Check(reloaded.maxDevices == 2, "max devices round-trips");
    Check(reloaded.showOffline == false, "show offline round-trips");
    Check(reloaded.displayMode == L"text", "display mode round-trips");
    Check(reloaded.ForDevice(L"g03_pro").enabled == false, "enabled round-trips");
    Check(reloaded.ForDevice(L"g03_pro").name == L"AUSDOM Mouse", "name round-trips");
    Check(reloaded.ForDevice(L"g03_pro").label == L"G3", "label round-trips");
    Check(reloaded.ForDevice(L"g03_pro").priority == 5, "priority round-trips");
    Check(reloaded.ForDevice(L"g03_pro").color == L"#112233", "color round-trips");

    reloaded.Save(file, registry);
    text = ReadText(file);
    Check(CountOccurrences(text, L"[plugin]") == 1,
        "second save still has one plugin section");
    Check(CountOccurrences(text, L"[device:g03_pro]") == 1,
        "second save still has one g03_pro section");

    DeleteFileW(file.c_str());
    RemoveDirectoryW(directory.c_str());

    if (failures != 0)
    {
        std::printf("\n%d settings test(s) failed\n", failures);
        return 1;
    }

    std::printf("\nall settings tests passed\n");
    return 0;
}
