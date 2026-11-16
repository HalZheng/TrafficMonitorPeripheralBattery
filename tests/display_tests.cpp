#include "plugin/BatteryGroupItem.h"
#include "plugin/BatteryItem.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
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

    pb::BatteryEntry MakeEntry(const wchar_t* label, const wchar_t* value,
        int percent, bool online)
    {
        pb::BatteryEntry entry;
        entry.id = label;
        entry.label = label;
        entry.value = value;
        entry.percent = percent;
        entry.online = online;
        return entry;
    }

    pb::BatteryEntry MakeStaleEntry(const wchar_t* label, const wchar_t* value,
        int percent, bool charging = false)
    {
        pb::BatteryEntry entry = MakeEntry(label, value, percent, true);
        entry.stale = true;
        entry.charging = charging;
        return entry;
    }

    /** Optional visual regression aid: display_tests --render <path.bmp>. */
    void RenderPreview(HDC screen, const char* path, const pb::ColorRules& rules)
    {
        std::vector<pb::BatteryEntry> entries;
        entries.push_back(MakeEntry(L"G03", L"84%", 84, true));
        pb::BatteryEntry low = MakeEntry(L"A9+", L"15%+", 15, true);
        low.charging = true;
        entries.push_back(low);
        entries.push_back(MakeStaleEntry(L"DW4", L"62%", 62));

        // Segments 7 and 8 put the stale badge into stacked / hybrid battery
        // rows as well (those modes only draw the first two devices).
        std::vector<pb::BatteryEntry> staleEntries;
        staleEntries.push_back(MakeStaleEntry(L"G03", L"84%", 84));
        staleEntries.push_back(MakeStaleEntry(L"A9+", L"15%+", 15, true));

        // Wide enough for the whole horizontal bar row too, so the third
        // (offline) device is not clipped out of the preview.
        int barRowWidth = 0;
        for (const pb::BatteryEntry& entry : entries)
        {
            barRowWidth += pb::BatteryItem::MeasureEntry(screen, entry, false)
                + pb::BatteryItem::GroupGap(screen);
        }
        const int width = std::max({
            pb::BatteryItem::RowWidth(screen, rules),
            pb::BatteryItem::StackedWidth(screen, rules, entries, true),
            barRowWidth }) + 16;
        const int height = 4 * (32 + 8) + 21 + 8 + 21 + 10 + 3 * (32 + 8);

        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HDC mem = CreateCompatibleDC(screen);
        HBITMAP dib = CreateDIBSection(mem, &info, DIB_RGB_COLORS,
            &bits, nullptr, 0);
        if (dib == nullptr || bits == nullptr)
        {
            DeleteDC(mem);
            return;
        }
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(mem, dib));

        RECT all = { 0, 0, width, height };
        HBRUSH background = CreateSolidBrush(RGB(28, 28, 30));
        FillRect(mem, &all, background);
        DeleteObject(background);

        pb::BatteryGroupItem item;
        item.SetSystemUsage(42, 76);
        item.SetEntries(entries);
        item.SetColorRules(rules);
        // 1: stacked, no percentages. 2: stacked with percentages.
        // 3: solid hybrid. 4: framed hybrid. 5: 21 px fallback. 6: bar.
        int cursor = 8;
        item.SetDisplayMode(L"stacked");
        item.SetStackedOptions(false);
        item.DrawItem(mem, 8, cursor, width - 16, 32, true);
        cursor += 32 + 8;

        item.SetStackedOptions(true);
        item.DrawItem(mem, 8, cursor, width - 16, 32, true);
        cursor += 32 + 8;

        item.SetDisplayMode(L"hybrid");
        item.DrawItem(mem, 8, cursor, width - 16, 32, true);
        cursor += 32 + 8;

        pb::ColorRules framedRules = rules;
        framedRules.frameless = false;
        item.SetColorRules(framedRules);
        item.DrawItem(mem, 8, cursor, width - 16, 32, true);
        cursor += 32 + 8;

        item.SetColorRules(rules);
        item.DrawItem(mem, 8, cursor, width - 16, 21, true);
        cursor += 21 + 8;

        item.SetDisplayMode(L"bar");
        item.DrawItem(mem, 8, cursor, width - 16, 21, true);
        cursor += 21 + 8;

        // 7: stacked with both batteries asleep. 8: hybrid with the same.
        item.SetEntries(staleEntries);
        item.SetDisplayMode(L"stacked");
        item.SetStackedOptions(true);
        item.DrawItem(mem, 8, cursor, width - 16, 32, true);
        cursor += 32 + 8;

        item.SetDisplayMode(L"hybrid");
        item.DrawItem(mem, 8, cursor, width - 16, 32, true);
        cursor += 32 + 8;

        // 9: a custom host text colour tints the track and offline badge.
        pb::ColorRules tintedRules = rules;
        tintedRules.hostTextColor = RGB(0, 120, 255);
        item.SetColorRules(tintedRules);
        item.SetDisplayMode(L"stacked");
        item.DrawItem(mem, 8, cursor, width - 16, 32, true);

        GdiFlush();
        BITMAPFILEHEADER file = {};
        file.bfType = 0x4D42;
        file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        DWORD imageBytes = static_cast<DWORD>(width) * height * 4;
        file.bfSize = file.bfOffBits + imageBytes;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(&file), sizeof(file));
        out.write(reinterpret_cast<const char*>(&info.bmiHeader),
            sizeof(info.bmiHeader));
        out.write(static_cast<const char*>(bits), imageBytes);
        out.close();

        SelectObject(mem, oldBitmap);
        DeleteObject(dib);
        DeleteDC(mem);
    }
}

int main(int argc, char** argv)
{
    using namespace pb;

    std::vector<BatteryEntry> empty;
    Check(BatteryItem::PlainText(empty, false).empty(),
        "empty group has empty text");

    BatteryEntry g03 = MakeEntry(L"G03", L"84%", 84, true);
    BatteryEntry b87 = MakeEntry(L"B87", L"--", -1, false);
    std::vector<BatteryEntry> one{ g03 };
    std::vector<BatteryEntry> two{ g03, b87 };

    Check(BatteryItem::PlainText(one, false) == L"G03 84%",
        "one device renders label and value");
    Check(BatteryItem::PlainText(two, false) == L"G03 84%  B87 --",
        "two devices are concatenated in text mode");
    Check(BatteryItem::PlainText(one, true) == L"G03 84",
        "hide percent removes the percent sign");

    BatteryEntry staleG03 = MakeStaleEntry(L"G03", L"84%", 84);
    std::vector<BatteryEntry> staleOne{ staleG03 };
    Check(BatteryItem::PlainText(staleOne, false) == L"G03 84%",
        "text mode keeps the cached level without widening the value");
    Check(BatteryItem::PlainText(staleOne, false) ==
        BatteryItem::PlainText(one, false),
        "cached text is byte-for-byte the live text");
    Check(BatteryItem::PlainText(staleOne, true) == L"G03 84",
        "hide percent still works for cached readings");
    Check(BatteryItem::SampleText(staleOne, false) ==
        BatteryItem::SampleText(one, false),
        "sample width is identical for cached and live readings");
    Check(!BatteryItem::SampleText(two, false).empty(),
        "sample text is generated for host width calculation");

    ColorRules rules;
    Check(rules.frameless, "solid rows are frameless by default");
    Check(BatteryItem::LevelColor(staleG03.percent, false,
        staleG03.online, false, rules) == rules.high,
        "cached reading keeps its level colour");
    Check(rules.hostTextColor == CLR_INVALID,
        "host text colour is unset by default");
    Check(BatteryItem::NeutralColor(RGB(62, 62, 66), CLR_INVALID) ==
        RGB(62, 62, 66), "missing host colour keeps the verified neutral tone");
    Check(BatteryItem::NeutralColor(RGB(62, 62, 66), RGB(255, 255, 255)) ==
        RGB(62, 62, 66), "default white text leaves neutral tones unchanged");
    Check(BatteryItem::NeutralColor(RGB(62, 62, 66), RGB(30, 30, 30)) ==
        RGB(62, 62, 66), "default dark text leaves neutral tones unchanged");
    COLORREF tinted = BatteryItem::NeutralColor(RGB(62, 62, 66),
        RGB(0, 120, 255));
    Check(tinted != RGB(62, 62, 66) && GetBValue(tinted) > 66,
        "a custom host colour tints neutral tones towards its hue");
    Check(BatteryItem::LevelColor(85, false, true, false, rules) == rules.high,
        "high battery is green");
    Check(BatteryItem::LevelColor(40, false, true, false, rules) == rules.medium,
        "medium battery is orange");
    Check(BatteryItem::LevelColor(10, false, true, false, rules) == rules.low,
        "low battery is red");
    Check(BatteryItem::LevelColor(10, true, true, false, rules) == rules.charging,
        "charging overrides the discharge level");
    Check(BatteryItem::LevelColor(85, false, false, false, rules) == rules.offline,
        "offline battery is grey");
    Check(BatteryItem::SystemColor(90, false, rules) == rules.low,
        "high cpu usage is red");
    Check(BatteryItem::SystemColor(60, false, rules) == rules.medium,
        "medium cpu usage is orange");
    Check(BatteryItem::SystemColor(10, false, rules) == rules.high,
        "low cpu usage is green");
    Check(BatteryItem::AdaptForTheme(rules.high, false) == rules.high,
        "light theme keeps the configured colour");
    Check(BatteryItem::AdaptForTheme(rules.high, true) != rules.high,
        "dark theme lifts the configured colour");
    Check(BatteryItem::ParseColor(L"auto", CLR_INVALID) == CLR_INVALID,
        "auto colour resolves to level-based colouring");
    Check(BatteryItem::ParseColor(L"#112233", CLR_INVALID) == RGB(0x11, 0x22, 0x33),
        "explicit hex colour still overrides auto");

    BatteryGroupItem item;
    Check(std::wstring(item.GetItemId()) == L"peripheral_battery",
        "single stable item id");
    Check(std::wstring(item.GetItemName()) == L"无线外设电量",
        "single stable item name");
    Check(item.IsCustomDraw(), "bar mode is custom drawn");
    Check(item.IsDoubleLineExclusive() == 0,
        "bar mode does not request the double-line slot");

    item.SetEntries(two);
    Check(std::wstring(item.GetItemValueText()) == L"G03 84%  B87 --",
        "group value text matches entries");
    Check(std::wstring(item.GetItemValueSampleText()).find(L"B87") !=
        std::wstring::npos, "sample text follows discovered devices");

    HDC dc = GetDC(nullptr);
    Check(dc != nullptr, "screen DC is available");
    if (dc != nullptr)
    {
        item.SetEntries(empty);
        int emptyWidth = item.GetItemWidthEx(dc);
        item.SetEntries(one);
        int oneWidth = item.GetItemWidthEx(dc);
        item.SetEntries(two);
        int twoWidth = item.GetItemWidthEx(dc);

        Check(emptyWidth == 0, "empty group needs no width");
        Check(oneWidth > 0, "one device needs width");
        Check(twoWidth > oneWidth,
            "width grows when a second device is discovered");
        Check(twoWidth - oneWidth > oneWidth / 4,
            "each device reserves its own label and battery");
        item.SetEntries(staleOne);
        Check(item.GetItemWidthEx(dc) == oneWidth,
            "offline badge does not change bar width");
        item.SetEntries(two);

        BatteryGroupItem hybrid;
        hybrid.SetDisplayMode(L"hybrid");
        hybrid.SetEntries(two);
        hybrid.SetSystemUsage(42, 61);
        Check(hybrid.IsCustomDraw(), "hybrid mode is custom drawn");
        Check(hybrid.IsDoubleLineExclusive() == 1,
            "hybrid mode requests the full double-line height");
        int hybridWidth = hybrid.GetItemWidthEx(dc);
        Check(hybridWidth > 0, "hybrid needs one column width");
        Check(hybridWidth < twoWidth,
            "hybrid is narrower than the horizontal group");
        hybrid.SetEntries(one);
        Check(hybrid.GetItemWidthEx(dc) == hybridWidth,
            "hybrid width is the widest row, not the sum");
        hybrid.SetDisplayMode(L"bar");
        Check(hybrid.IsDoubleLineExclusive() == 0,
            "leaving hybrid releases the double-line slot");

        int stackedWidth = BatteryItem::StackedWidth(dc, rules, two, false);
        int stackedPercentWidth = BatteryItem::StackedWidth(
            dc, rules, two, true);
        Check(stackedWidth < 130,
            "stacked without percentages is compact");
        Check(stackedWidth < twoWidth,
            "stacked is narrower than the old horizontal group");
        Check(stackedPercentWidth > stackedWidth,
            "showing percentages widens the shared grid");
        BatteryGroupItem stacked;
        stacked.SetDisplayMode(L"stacked");
        stacked.SetEntries(two);
        stacked.SetSystemUsage(42, 76);
        Check(stacked.IsCustomDraw(), "stacked mode is custom drawn");
        Check(stacked.IsDoubleLineExclusive() == 1,
            "stacked mode requests the full double-line height");
        Check(stacked.GetItemWidthEx(dc) == stackedWidth,
            "stacked hides percentages by default");
        stacked.SetStackedOptions(true);
        Check(stacked.GetItemWidthEx(dc) == stackedPercentWidth,
            "enabling percentages grows the reported width");
        stacked.SetStackedOptions(false);
        stacked.SetEntries(one);
        Check(stacked.GetItemWidthEx(dc) < stackedWidth,
            "one device shrinks the grid instead of leaving a blank column");
        stacked.SetEntries(two);

        StackedLayout layout = BatteryItem::MakeStackedLayout(
            dc, rules, two, false);
        Check(layout.width == stackedWidth,
            "layout width matches the reported width");
        Check(layout.width == 2 * layout.cellWidth + layout.cellGap,
            "grid width is exactly two cells plus one gap");
        Check(layout.cellWidth == layout.labelWidth + layout.gap
            + layout.iconWidth,
            "battery cell holds only label, gap and icon by default");

        std::vector<BatteryEntry> staleTwo{ staleG03,
            MakeStaleEntry(L"A9+", L"15%+", 15, true) };
        Check(BatteryItem::StackedWidth(dc, rules, staleTwo, false) == stackedWidth,
            "offline badge does not change the stacked grid width");

        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, 200, 64);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(mem, bitmap));
        hybrid.SetDisplayMode(L"hybrid");
        hybrid.SetEntries(two);
        hybrid.DrawItem(mem, 0, 0, hybridWidth, 32, true);
        hybrid.DrawItem(mem, 0, 0, hybridWidth, 20, false);
        ColorRules framed = rules;
        framed.frameless = false;
        Check(BatteryItem::RowWidth(dc, rules) <
            BatteryItem::RowWidth(dc, framed),
            "frameless rows need less horizontal gap");
        hybrid.SetColorRules(framed);
        hybrid.DrawItem(mem, 0, 0, BatteryItem::RowWidth(dc, framed), 32, true);
        Check(true, "framed rollback style still draws");
        stacked.DrawItem(mem, 0, 0, stackedWidth, 32, true);
        stacked.SetStackedOptions(true);
        stacked.DrawItem(mem, 0, 0, stackedPercentWidth, 32, true);
        stacked.SetStackedOptions(false);
        stacked.DrawItem(mem, 0, 0, stackedWidth, 21, true);
        Check(true, "stacked drawing paths complete without GDI errors");

        item.SetEntries(staleTwo);
        item.SetDisplayMode(L"bar");
        item.DrawItem(mem, 0, 0, twoWidth, 20, false);
        stacked.SetEntries(staleTwo);
        stacked.DrawItem(mem, 0, 0, stackedWidth, 32, true);
        hybrid.SetEntries(staleTwo);
        hybrid.DrawItem(mem, 0, 0, BatteryItem::RowWidth(dc, framed), 32, true);
        Check(true, "offline badge draws in bar, stacked and hybrid paths");

        hybrid.SetDisplayMode(L"bar");
        hybrid.DrawItem(mem, 0, 0, twoWidth, 20, false);
        Check(true, "hybrid and bar drawing paths complete without GDI errors");
        SelectObject(mem, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(mem);

        item.SetDisplayMode(L"text");
        Check(!item.IsCustomDraw(), "text mode uses host rendering");
        Check(item.GetItemWidthEx(dc) == 0,
            "text mode does not report a custom width");

        if (argc >= 3 && std::string(argv[1]) == "--render")
            RenderPreview(dc, argv[2], rules);
        ReleaseDC(nullptr, dc);
    }

    if (failures != 0)
    {
        std::printf("\n%d display test(s) failed\n", failures);
        return 1;
    }

    std::printf("\nall display tests passed\n");
    return 0;
}
