#include "plugin/Settings.h"

#include "core/DeviceManager.h"
#include "protocols/IProtocol.h"

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

    struct FakeProtocol : pb::IProtocol
    {
        const wchar_t* Id() const override { return L"fake"; }
        pb::BatteryReading Read(const pb::DeviceDescriptor&) const override
        {
            return {};
        }
    };
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
    Check(settings.maxDevices == 6, "missing file keeps default max devices");
    Check(settings.displayMode == L"bar", "missing file keeps bar display mode");
    Check(settings.presenceGraceSeconds == 300, "missing file keeps default grace");
    Check(settings.keepLastKnown, "missing file keeps last-known readings enabled");
    Check(settings.lowBatteryNotify, "missing file enables low-battery alerts");
    Check(settings.lowBatteryNotifyPercent == 10,
        "missing file keeps the 10 percent alert threshold");
    Check(settings.devices.size() == registry.All().size(),
        "defaults cover every registered device");
    Check(settings.persistedDevices.empty(),
        "missing file persists no device sections");
    Check(settings.ForDevice(L"g03_pro").label == L"G03",
        "g03_pro default label");
    Check(settings.ForDevice(L"ilovbee_b87").label == L"B87",
        "ilovbee_b87 default label");
    Check(settings.ForDevice(L"g03_pro").color == L"auto",
        "battery colour defaults to auto by level");
    Check(settings.hybridRows == 4, "hybrid defaults to four rows");
    Check(settings.rowStyle == L"solid", "rows default to frameless solid fill");
    Check(!settings.stackedShowPercent,
        "stacked hides percentages by default");
    Check(settings.batteryLowPercent == 20, "low battery threshold defaults to 20");
    Check(settings.batteryGoodPercent == 60, "good battery threshold defaults to 60");
    Check(settings.colorHigh == L"#4CAF50", "high colour defaults to green");
    Check(settings.colorCharging == L"#03A9F4", "charging colour defaults to cyan blue");

    settings.refreshIntervalSeconds = 12;
    settings.maxDevices = 2;
    settings.showOffline = false;
    settings.displayMode = L"text";
    settings.presenceGraceSeconds = 90;
    settings.keepLastKnown = false;
    settings.lowBatteryNotify = false;
    settings.lowBatteryNotifyPercent = 8;
    settings.hybridRows = 3;
    settings.rowBarWidth = 64;
    settings.rowStyle = L"framed";
    settings.stackedShowPercent = true;
    settings.batteryLowPercent = 15;
    settings.batteryGoodPercent = 55;
    settings.cpuOrangePercent = 45;
    settings.cpuRedPercent = 85;
    settings.colorHigh = L"#123456";
    settings.colorCharging = L"#654321";
    settings.colorAutoMigrationDone = true;
    Check(settings.NoteDevice(L"g03_pro"), "first discovery is new");
    Check(!settings.NoteDevice(L"g03_pro"), "second discovery is not new");

    DeviceSettings& g03 = settings.devices[L"g03_pro"];
    g03.hidden = true;
    g03.name = L"AUSDOM Mouse";
    g03.label = L"G3";
    g03.priority = 5;
    g03.color = L"#112233";

    settings.Save(file, registry);
    Check(FilePresent(file), "save creates the ini file");
    Check(!FilePresent(file + L".tmp"), "save leaves no temporary file");

    std::wstring text = ReadText(file);
    Check(CountOccurrences(text, L"[plugin]") == 1,
        "one plugin section after save");
    Check(CountOccurrences(text, L"[device:g03_pro]") == 1,
        "one g03_pro section after save");
    Check(CountOccurrences(text, L"[device:ilovbee_b87]") == 0,
        "undiscovered devices are not persisted");
    Check(text.find(L"hidden=true") != std::wstring::npos,
        "hidden flag is written instead of enabled");
    Check(text.find(L"presence_grace_seconds=90") != std::wstring::npos,
        "grace window is written");
    Check(text.find(L"keep_last_known=false") != std::wstring::npos,
        "keep-last-known switch is written");
    Check(text.find(L"low_battery_notify=false") != std::wstring::npos,
        "low-battery alert switch is written");
    Check(text.find(L"low_battery_notify_percent=8") != std::wstring::npos,
        "low-battery threshold is written");

    Settings reloaded;
    reloaded.Load(file, registry);
    Check(reloaded.refreshIntervalSeconds == 12, "interval round-trips");
    Check(reloaded.maxDevices == 2, "max devices round-trips");
    Check(reloaded.showOffline == false, "show offline round-trips");
    Check(reloaded.displayMode == L"text", "display mode round-trips");
    Check(reloaded.presenceGraceSeconds == 90, "grace window round-trips");
    Check(!reloaded.keepLastKnown, "keep-last-known switch round-trips");
    Check(!reloaded.lowBatteryNotify, "low-battery alert switch round-trips");
    Check(reloaded.lowBatteryNotifyPercent == 8,
        "low-battery threshold round-trips");
    Check(reloaded.ForDevice(L"g03_pro").hidden == true, "hidden round-trips");
    Check(reloaded.ForDevice(L"g03_pro").name == L"AUSDOM Mouse", "name round-trips");
    Check(reloaded.ForDevice(L"g03_pro").label == L"G3", "label round-trips");
    Check(reloaded.ForDevice(L"g03_pro").priority == 5, "priority round-trips");
    Check(reloaded.ForDevice(L"g03_pro").color == L"#112233", "color round-trips");
    Check(reloaded.persistedDevices.size() == 1,
        "reload keeps only the persisted device");
    Check(reloaded.KnownDeviceIds(registry).size() == 1,
        "options list only contains known devices");
    Check(reloaded.hybridRows == 3, "hybrid row count round-trips");
    Check(reloaded.rowBarWidth == 64, "row bar width round-trips");
    Check(reloaded.rowStyle == L"framed", "row style round-trips");
    Check(reloaded.stackedShowPercent, "stacked percent flag round-trips");
    Check(reloaded.batteryLowPercent == 15, "low threshold round-trips");
    Check(reloaded.batteryGoodPercent == 55, "good threshold round-trips");
    Check(reloaded.cpuOrangePercent == 45, "cpu orange threshold round-trips");
    Check(reloaded.cpuRedPercent == 85, "cpu red threshold round-trips");
    Check(reloaded.colorHigh == L"#123456", "high colour round-trips");
    Check(reloaded.colorCharging == L"#654321", "charging colour round-trips");
    Check(reloaded.colorAutoMigrationDone, "one-time colour migration flag round-trips");

    reloaded.Save(file, registry);
    text = ReadText(file);
    Check(CountOccurrences(text, L"[plugin]") == 1,
        "second save still has one plugin section");
    Check(CountOccurrences(text, L"[device:g03_pro]") == 1,
        "second save still has one g03_pro section");
    Check(CountOccurrences(text, L"[device:ilovbee_b87]") == 0,
        "second save does not grow the device list");

    // Old INIs used enabled=false; that must become hidden=true.
    std::wstring legacyFile = directory + L"\\legacy.ini";
    DeleteFileW(legacyFile.c_str());
    WritePrivateProfileStringW(L"device:ilovbee_b87", L"enabled", L"false",
        legacyFile.c_str());
    Settings legacy;
    legacy.Load(legacyFile, registry);
    Check(legacy.ForDevice(L"ilovbee_b87").hidden,
        "legacy enabled=false migrates to hidden");
    Check(legacy.IsPersisted(L"ilovbee_b87"),
        "legacy device section stays persisted");
    Check(!legacy.ForDevice(L"g03_pro").hidden,
        "device without a legacy section stays visible");

    std::wstring modeFile = directory + L"\\mode.ini";
    DeleteFileW(modeFile.c_str());
    WritePrivateProfileStringW(L"plugin", L"display_mode", L"something",
        modeFile.c_str());
    Settings unknownMode;
    unknownMode.Load(modeFile, registry);
    Check(unknownMode.displayMode == L"bar",
        "unknown display mode falls back to bar");
    WritePrivateProfileStringW(L"plugin", L"display_mode", L"hybrid",
        modeFile.c_str());
    Settings hybridMode;
    hybridMode.Load(modeFile, registry);
    Check(hybridMode.displayMode == L"hybrid",
        "hybrid display mode round-trips");
    WritePrivateProfileStringW(L"plugin", L"display_mode", L"stacked",
        modeFile.c_str());
    Settings stackedMode;
    stackedMode.Load(modeFile, registry);
    Check(stackedMode.displayMode == L"stacked",
        "stacked display mode round-trips");
    WritePrivateProfileStringW(L"plugin", L"row_style", L"bogus",
        modeFile.c_str());
    Settings bogusStyle;
    bogusStyle.Load(modeFile, registry);
    Check(bogusStyle.rowStyle == L"solid",
        "unknown row style falls back to solid");

    // The advanced dialog and the INI loader share one set of range limits.
    Settings clamped;
    clamped.lowBatteryNotifyPercent = 500;
    clamped.presenceGraceSeconds = -3;
    clamped.hybridRows = 99;
    clamped.rowBarWidth = 1;
    clamped.batteryLowPercent = 200;
    clamped.batteryGoodPercent = 1;
    clamped.cpuOrangePercent = 200;
    clamped.cpuRedPercent = 1;
    clamped.rowStyle = L"bogus";
    clamped.displayMode = L"bogus";
    clamped.ClampValues();
    Check(clamped.lowBatteryNotifyPercent == 100,
        "alert threshold is clamped to 100");
    Check(clamped.presenceGraceSeconds == 0,
        "negative grace window is clamped to zero");
    Check(clamped.hybridRows == 4 && clamped.rowBarWidth == 16,
        "row count and bar width are clamped");
    Check(clamped.batteryLowPercent == 98 &&
        clamped.batteryGoodPercent > clamped.batteryLowPercent,
        "good threshold stays above the low threshold");
    Check(clamped.cpuRedPercent > clamped.cpuOrangePercent,
        "cpu red threshold stays above orange");
    Check(clamped.rowStyle == L"solid" && clamped.displayMode == L"bar",
        "invalid style and display mode fall back");

    // A cached reading stays fully usable: the offline badge is added on top
    // of a normal level, not instead of it.
    BatteryReading cached;
    cached.online = true;
    cached.percent = 84;
    cached.stale = true;
    Check(cached.HasPercent(), "cached last-known reading keeps its percent");
    Check(!BatteryReading{}.HasPercent(), "empty reading has no percent");
    Check(!BatteryReading{}.stale, "fresh default reading is not stale");

    // Grace window is pure logic, so it is testable without a worker thread.
    Check(DeviceManager::WithinGrace(1000, 1000 + 299000, 300),
        "device stays visible inside grace window");
    Check(!DeviceManager::WithinGrace(1000, 1000 + 301000, 300),
        "device disappears after grace window");
    Check(!DeviceManager::WithinGrace(0, 5000, 300),
        "never-seen device is not visible");
    Check(!DeviceManager::WithinGrace(1000, 5000, 0),
        "zero grace window removes device immediately");

    // Low-battery alerts: one notification per episode, never for cached data.
    LowBatteryNotifier notifier;
    notifier.Configure(true, 10);
    std::vector<LowBatterySample> samples{ { L"mouse", L"鼠标", 12, true } };
    Check(notifier.Update(samples).empty(),
        "above threshold does not notify");
    samples[0].percent = 9;
    Check(notifier.Update(samples).size() == 1,
        "crossing the threshold notifies once");
    Check(notifier.Update(samples).empty(),
        "a repeated low level does not notify again");
    samples[0].percent = 12;
    Check(notifier.Update(samples).empty(),
        "hysteresis keeps the latch just above threshold");
    samples[0].percent = 16;
    Check(notifier.Update(samples).empty(),
        "recovering to threshold + 5 resets the latch");
    samples[0].percent = 8;
    Check(notifier.Update(samples).size() == 1,
        "a later drop notifies again");
    notifier.Reset();
    samples[0].percent = 3;
    samples[0].live = false;
    Check(notifier.Update(samples).empty(),
        "stale readings never raise an alert");
    notifier.Configure(false, 10);
    samples[0].live = true;
    Check(notifier.Update(samples).empty(),
        "disabling alerts keeps the notifier silent");
    notifier.Configure(true, 10);
    Check(notifier.Update(samples).size() == 1,
        "re-enabling alerts allows a fresh episode");

    // Default presence detection matches a freshly enumerated HID interface.
    FakeProtocol protocol;
    std::vector<HidInterfaceInfo> interfaces;
    HidInterfaceInfo info = {};
    info.vid = 0x363C;
    info.pid = 0xFF99;
    info.usagePage = 0xFFA0;
    info.usage = 0x0002;
    interfaces.push_back(info);
    const DeviceDescriptor descriptor = *registry.FindById(L"g03_pro");
    Check(protocol.IsPresent(descriptor, interfaces),
        "matching HID interface is detected");
    interfaces[0].usage = 0x0003;
    Check(!protocol.IsPresent(descriptor, interfaces),
        "usage mismatch is not detected");
    interfaces.clear();
    Check(!protocol.IsPresent(descriptor, interfaces),
        "empty interface list means absent");

    DeleteFileW(file.c_str());
    DeleteFileW(legacyFile.c_str());
    DeleteFileW(modeFile.c_str());
    RemoveDirectoryW(directory.c_str());

    if (failures != 0)
    {
        std::printf("\n%d settings test(s) failed\n", failures);
        return 1;
    }

    std::printf("\nall settings tests passed\n");
    return 0;
}
