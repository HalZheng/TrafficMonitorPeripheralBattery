#include "BatteryItem.h"

#include <algorithm>
#include <cwchar>

namespace pb
{
    namespace
    {
        int HexDigit(wchar_t ch)
        {
            if (ch >= L'0' && ch <= L'9')
                return ch - L'0';
            if (ch >= L'a' && ch <= L'f')
                return ch - L'a' + 10;
            if (ch >= L'A' && ch <= L'F')
                return ch - L'A' + 10;
            return -1;
        }

        std::wstring StripPercent(const std::wstring& value, bool hidePercent)
        {
            std::wstring text = value;
            if (!hidePercent)
                return text;
            size_t percent = text.find(L'%');
            if (percent != std::wstring::npos)
                text.erase(percent, 1);
            return text;
        }

        int DpiScale(HDC dc, int value)
        {
            int dpi = dc != nullptr ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
            if (dpi <= 0)
                dpi = 96;
            return MulDiv(value, dpi, 96);
        }

        /**
         * Small high-contrast "x" badge for a cached (offline) reading.
         * The solid plate keeps it legible over a green/red/blue fill and on
         * both light and dark taskbar backgrounds; nothing else about the
         * battery changes, so the level and its colour stay recognisable.
         */
        void DrawOfflineMark(HDC dc, int x, int y, int size, bool darkMode,
            const ColorRules& rules)
        {
            if (dc == nullptr || size < 3)
                return;

            COLORREF plate = BatteryItem::NeutralColor(
                darkMode ? RGB(238, 238, 238) : RGB(32, 32, 34),
                rules.hostTextColor);
            COLORREF ink = BatteryItem::NeutralColor(
                darkMode ? RGB(32, 32, 34) : RGB(238, 238, 238),
                rules.hostTextColor);
            RECT rect = { x, y, x + size, y + size };
            HBRUSH brush = CreateSolidBrush(plate);
            FillRect(dc, &rect, brush);
            DeleteObject(brush);

            for (int i = 1; i < size - 1; ++i)
            {
                SetPixel(dc, x + i, y + i, ink);
                SetPixel(dc, x + size - 1 - i, y + i, ink);
            }
        }

        /** Fit the badge inside a bar/icon without spilling into the next row. */
        int BadgeSize(int scaleUnit, int height, int width)
        {
            int size = std::max(3, std::min(scaleUnit, height - 1));
            if (size > width - 2)
                size = width - 2;
            return size;
        }
    }

    COLORREF BatteryItem::ParseColor(const std::wstring& text, COLORREF fallback)
    {
        if (text.size() != 7 || text[0] != L'#')
            return fallback;

        int r = HexDigit(text[1]) * 16 + HexDigit(text[2]);
        int g = HexDigit(text[3]) * 16 + HexDigit(text[4]);
        int b = HexDigit(text[5]) * 16 + HexDigit(text[6]);
        if (r < 0 || g < 0 || b < 0)
            return fallback;
        return RGB(r, g, b);
    }

    COLORREF BatteryItem::NeutralColor(COLORREF fallback,
        COLORREF hostTextColor)
    {
        if (hostTextColor == CLR_INVALID)
            return fallback;

        // The plugin's own defaults and the host's plain black / white are the
        // verified look. Only a genuinely custom colour changes the tones, so
        // enabling this feature never alters what the user already approved.
        const bool isDefault =
            hostTextColor == RGB(235, 235, 235) ||
            hostTextColor == RGB(30, 30, 30) ||
            hostTextColor == RGB(255, 255, 255) ||
            hostTextColor == RGB(0, 0, 0);
        if (isDefault)
            return fallback;

        auto tint = [](int base, int host)
        {
            return base + (host - base) / 5;   // 20 % towards the host colour
        };
        return RGB(tint(GetRValue(fallback), GetRValue(hostTextColor)),
            tint(GetGValue(fallback), GetGValue(hostTextColor)),
            tint(GetBValue(fallback), GetBValue(hostTextColor)));
    }

    COLORREF BatteryItem::AdaptForTheme(COLORREF color, bool darkMode)
    {
        if (!darkMode)
            return color;
        auto lift = [](int channel)
        {
            return channel + (255 - channel) / 4;
        };
        return RGB(lift(GetRValue(color)), lift(GetGValue(color)),
            lift(GetBValue(color)));
    }

    COLORREF BatteryItem::LevelColor(int percent, bool charging, bool online,
        bool darkMode, const ColorRules& rules)
    {
        COLORREF color = rules.offline;
        if (online && percent >= 0)
        {
            if (charging)
                color = rules.charging;
            else if (percent <= rules.lowPercent)
                color = rules.low;
            else if (percent < rules.goodPercent)
                color = rules.medium;
            else
                color = rules.high;
        }
        return AdaptForTheme(color, darkMode);
    }

    COLORREF BatteryItem::SystemColor(int percent, bool darkMode,
        const ColorRules& rules)
    {
        COLORREF color = rules.offline;
        if (percent >= 0)
        {
            if (percent >= rules.cpuRedPercent)
                color = rules.low;
            else if (percent >= rules.cpuOrangePercent)
                color = rules.medium;
            else
                color = rules.high;
        }
        return AdaptForTheme(color, darkMode);
    }

    int BatteryItem::GroupGap(HDC dc)
    {
        return DpiScale(dc, 10);
    }

    int BatteryItem::MeasureEntry(HDC dc, const BatteryEntry& entry, bool hidePercent)
    {
        if (dc == nullptr)
            return 0;

        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };

        SIZE labelSize = {};
        if (!entry.label.empty())
            GetTextExtentPoint32W(dc, entry.label.c_str(),
                static_cast<int>(entry.label.size()), &labelSize);

        SIZE valueSize = {};
        const wchar_t* sample = hidePercent ? L"100+" : L"100%+";
        GetTextExtentPoint32W(dc, sample,
            static_cast<int>(wcslen(sample)), &valueSize);

        int width = labelSize.cx;
        if (!entry.label.empty())
            width += scale(4);
        width += scale(18) + scale(4) + valueSize.cx + scale(2);
        return width;
    }

    void BatteryItem::DrawEntry(HDC dc, int x, int y, int w, int h,
        const BatteryEntry& entry, bool darkMode, bool hidePercent,
        COLORREF labelColor, COLORREF valueColor, const ColorRules& rules)
    {
        if (dc == nullptr)
            return;

        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };

        int oldBkMode = SetBkMode(dc, TRANSPARENT);
        COLORREF defaultTextColor = darkMode ? RGB(235, 235, 235) : RGB(30, 30, 30);
        COLORREF effectiveLabel = labelColor != CLR_INVALID
            ? labelColor : defaultTextColor;
        COLORREF effectiveValue = valueColor != CLR_INVALID
            ? valueColor : defaultTextColor;
        COLORREF oldTextColor = SetTextColor(dc, effectiveLabel);

        SIZE labelSize = {};
        if (!entry.label.empty())
            GetTextExtentPoint32W(dc, entry.label.c_str(),
                static_cast<int>(entry.label.size()), &labelSize);

        int contentX = x;
        if (!entry.label.empty())
        {
            RECT labelRect = { x, y, x + labelSize.cx, y + h };
            DrawTextW(dc, entry.label.c_str(), -1, &labelRect,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT);
            contentX += labelSize.cx + scale(4);
        }

        const int iconWidth = scale(16) + scale(2);  // body + cap
        const int iconX = contentX;
        DrawBatteryIcon(dc, iconX, y, iconWidth, h, entry, darkMode,
            effectiveValue, rules);

        int valueX = iconX + iconWidth + scale(4);
        RECT valueRect = { valueX, y, x + w, y + h };
        SetTextColor(dc, effectiveValue);
        std::wstring value = StripPercent(entry.value, hidePercent);
        DrawTextW(dc, value.c_str(), -1, &valueRect,
            DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT | DT_END_ELLIPSIS);

        SetTextColor(dc, oldTextColor);
        SetBkMode(dc, oldBkMode);
    }

    void BatteryItem::DrawBatteryIcon(HDC dc, int x, int y, int w, int h,
        const BatteryEntry& entry, bool darkMode, COLORREF textColor,
        const ColorRules& rules)
    {
        if (dc == nullptr || w <= 0 || h <= 0)
            return;

        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };

        int capWidth = scale(2);
        int bodyWidth = w - capWidth;
        if (bodyWidth < scale(8))
            bodyWidth = scale(8);
        int iconHeight = std::min(scale(10), h - 1);
        if (iconHeight < 4)
            iconHeight = 4;
        int iconY = y + (h - iconHeight) / 2;

        COLORREF borderColor = entry.online
            ? textColor
            : NeutralColor(darkMode ? RGB(130, 130, 130) : RGB(150, 150, 150),
                rules.hostTextColor);
        // A charging device must be recognisable even at 15%: tint the outline.
        if (entry.online && entry.charging)
            borderColor = LevelColor(entry.percent, true, true, darkMode, rules);

        COLORREF fillColor;
        if (entry.color != CLR_INVALID && entry.online && !entry.charging)
            fillColor = AdaptForTheme(entry.color, darkMode);
        else
            fillColor = LevelColor(entry.percent, entry.charging, entry.online,
                darkMode, rules);

        HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
        HBRUSH oldBrush = static_cast<HBRUSH>(
            SelectObject(dc, GetStockObject(NULL_BRUSH)));

        RoundRect(dc, x, iconY, x + bodyWidth, iconY + iconHeight,
            scale(2), scale(2));
        RECT capRect = { x + bodyWidth, iconY + scale(3),
            x + bodyWidth + capWidth, iconY + iconHeight - scale(3) };
        HBRUSH capBrush = CreateSolidBrush(borderColor);
        FillRect(dc, &capRect, capBrush);
        DeleteObject(capBrush);

        if (entry.online && entry.percent > 0)
        {
            int innerWidth = bodyWidth - scale(4);
            int innerHeight = iconHeight - scale(4);
            if (innerWidth > 0 && innerHeight > 0)
            {
                int fillWidth =
                    (innerWidth * std::min(entry.percent, 100) + 99) / 100;
                if (fillWidth < 2)
                    fillWidth = 2;
                if (fillWidth > innerWidth)
                    fillWidth = innerWidth;
                RECT fillRect = { x + scale(2), iconY + scale(2),
                    x + scale(2) + fillWidth,
                    iconY + scale(2) + innerHeight };
                HBRUSH fillBrush = CreateSolidBrush(fillColor);
                FillRect(dc, &fillRect, fillBrush);
                DeleteObject(fillBrush);
            }
        }

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);

        if (entry.stale)
        {
            int size = BadgeSize(scale(5), iconHeight, bodyWidth);
            DrawOfflineMark(dc, x + bodyWidth - size - 1, iconY + 1,
                size, darkMode, rules);
        }
    }

    int BatteryItem::RowWidth(HDC dc, const ColorRules& rules)
    {
        int barWidth = std::max(8, rules.rowBarWidth);
        int gap = DpiScale(dc, rules.frameless ? 1 : 3);
        return DpiScale(dc, 26) + gap + DpiScale(dc, barWidth)
            + DpiScale(dc, 2) + gap + DpiScale(dc, 22);
    }

    StackedLayout BatteryItem::MakeStackedLayout(HDC dc,
        const ColorRules& rules, const std::vector<BatteryEntry>& entries,
        bool showPercent)
    {
        // Measure with the same 9 px font used for drawing, so the reported
        // width is exactly the drawn width and no cell needs stretching.
        HFONT font = CreateFontW(-DpiScale(dc, 9), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = nullptr;
        if (font != nullptr)
            oldFont = static_cast<HFONT>(SelectObject(dc, font));

        auto textWidth = [dc](const std::wstring& text) -> int
        {
            if (text.empty())
                return 0;
            SIZE size = {};
            GetTextExtentPoint32W(dc, text.c_str(),
                static_cast<int>(text.size()), &size);
            return size.cx;
        };

        int labelWidth = std::max(textWidth(L"CPU"), textWidth(L"MEM"));
        for (const BatteryEntry& entry : entries)
            labelWidth = std::max(labelWidth, textWidth(entry.label));
        labelWidth += DpiScale(dc, 2);
        if (labelWidth < DpiScale(dc, 18))
            labelWidth = DpiScale(dc, 18);

        int valueWidth = showPercent
            ? textWidth(L"100%") + DpiScale(dc, 2) : 0;

        if (oldFont != nullptr)
            SelectObject(dc, oldFont);
        if (font != nullptr)
            DeleteObject(font);

        StackedLayout layout;
        layout.labelWidth = labelWidth;
        layout.gap = DpiScale(dc, 4);
        layout.iconWidth = DpiScale(dc, 16) + DpiScale(dc, 2);
        layout.valueGap = showPercent ? DpiScale(dc, 3) : 0;
        layout.valueWidth = valueWidth;
        layout.cellGap = DpiScale(dc, 8);
        layout.cellWidth = labelWidth + layout.gap + layout.iconWidth
            + layout.valueGap + layout.valueWidth;

        // Width comes from the actual number of cells. Nothing is stretched,
        // so a single device does not leave a blank second column either.
        int cells = static_cast<int>(std::min<size_t>(2u, entries.size()));
        if (cells == 0)
        {
            layout.width = labelWidth + layout.gap
                + DpiScale(dc, std::max(8, rules.rowBarWidth))
                + layout.valueGap + layout.valueWidth;
        }
        else
        {
            layout.width = cells * layout.cellWidth
                + (cells - 1) * layout.cellGap;
        }
        return layout;
    }

    int BatteryItem::StackedWidth(HDC dc, const ColorRules& rules,
        const std::vector<BatteryEntry>& entries, bool showPercent)
    {
        return MakeStackedLayout(dc, rules, entries, showPercent).width;
    }

    int BatteryItem::HybridMinHeight(HDC dc)
    {
        return DpiScale(dc, 28);
    }

    void BatteryItem::DrawRow(HDC dc, int x, int y, int w, int h,
        const RowEntry& row, bool compact, bool darkMode,
        COLORREF labelColor, COLORREF valueColor, const ColorRules& rules)
    {
        if (dc == nullptr || w <= 0 || h <= 0)
            return;

        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };

        COLORREF defaultTextColor = darkMode ? RGB(235, 235, 235) : RGB(30, 30, 30);
        COLORREF effectiveLabel = labelColor != CLR_INVALID
            ? labelColor : defaultTextColor;
        COLORREF effectiveValue = valueColor != CLR_INVALID
            ? valueColor : defaultTextColor;

        COLORREF fillColor;
        if (row.fixedColor != CLR_INVALID && row.online && !row.charging)
            fillColor = AdaptForTheme(row.fixedColor, darkMode);
        else if (row.battery)
            fillColor = LevelColor(row.percent, row.charging, row.online,
                darkMode, rules);
        else
            fillColor = SystemColor(row.percent, darkMode, rules);

        COLORREF borderColor = row.online
            ? effectiveValue
            : NeutralColor(darkMode ? RGB(110, 110, 110) : RGB(150, 150, 150),
                rules.hostTextColor);

        int labelWidth = compact ? 0 : scale(26);
        int valueWidth = compact ? 0 : scale(22);
        int gap = scale(rules.frameless ? 1 : 3);
        int barX = x + (compact ? 0 : labelWidth + gap);
        int barWidth = (x + w) - barX - (compact ? 0 : valueWidth + gap);
        if (barWidth < scale(6))
            barWidth = scale(6);

        int barHeight = row.battery
            ? std::max(scale(6), 4) : std::max(scale(4), 3);
        if (barHeight > h)
            barHeight = h;
        int barY = y + (h - barHeight) / 2;
        // The frameless branch uses a taller body; keep its geometry for the
        // badge so it never spills into the neighbouring row.
        int badgeBaseY = barY;
        int badgeHeight = barHeight;
        int bodyWidth = barWidth - (row.battery ? scale(2) : 0);
        if (bodyWidth < 4)
            bodyWidth = 4;

        HFONT smallFont = nullptr;
        HFONT oldFont = nullptr;
        if (!compact)
        {
            // Four rows in 32 px means ~8 px per row. A 7 px ClearType font
            // keeps the digit shapes intact; greyscale AA smudges them and
            // non-AA drops strokes. Grow towards 9 px with fewer rows.
            int fontHeight = std::max(7,
                std::min(DpiScale(dc, 9), h - 1));
            smallFont = CreateFontW(-fontHeight, 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            if (smallFont != nullptr)
                oldFont = static_cast<HFONT>(SelectObject(dc, smallFont));
        }

        int oldBkMode = SetBkMode(dc, TRANSPARENT);
        COLORREF oldTextColor = SetTextColor(dc, effectiveLabel);

        if (!compact)
        {
            RECT labelRect = { x, y, x + labelWidth, y + h };
            DrawTextW(dc, row.label.c_str(), -1, &labelRect,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT |
                DT_END_ELLIPSIS);
            SetTextColor(dc, effectiveValue);
            RECT valueRect = { x + w - valueWidth, y, x + w, y + h };
            DrawTextW(dc, row.value.c_str(), -1, &valueRect,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RIGHT |
                DT_END_ELLIPSIS);
        }

        if (rules.frameless)
        {
            // Spend the whole row on colour: no stroke, no inset, no vertical
            // padding. One pixel is kept between rows so they stay distinct.
            const int bodyHeight = std::max(4, h - 1);
            const int bodyY = y + (h - bodyHeight) / 2;
            badgeBaseY = bodyY;
            badgeHeight = bodyHeight;
            int fillWidth = 0;
            if (row.online && row.percent > 0)
                fillWidth = bodyWidth * std::min(row.percent, 100) / 100;

            if (row.battery)
            {
                // A same-height dark silhouette keeps the battery shape
                // without costing a 1 px outline.
                COLORREF track = NeutralColor(
                    darkMode ? RGB(62, 62, 66) : RGB(202, 202, 202),
                    rules.hostTextColor);
                HPEN trackPen = CreatePen(PS_SOLID, 1, track);
                HBRUSH trackBrush = CreateSolidBrush(track);
                HPEN oldPen = static_cast<HPEN>(SelectObject(dc, trackPen));
                HBRUSH oldBrush = static_cast<HBRUSH>(
                    SelectObject(dc, trackBrush));
                RoundRect(dc, barX, bodyY, barX + bodyWidth,
                    bodyY + bodyHeight, scale(1), scale(1));
                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(trackBrush);
                DeleteObject(trackPen);

                RECT capRect = { barX + bodyWidth, bodyY + bodyHeight / 3,
                    barX + bodyWidth + scale(2),
                    bodyY + bodyHeight - bodyHeight / 3 };
                HBRUSH capBrush = CreateSolidBrush(track);
                FillRect(dc, &capRect, capBrush);
                DeleteObject(capBrush);
            }

            if (fillWidth > 0)
            {
                if (row.battery)
                {
                    HPEN fillPen = CreatePen(PS_SOLID, 1, fillColor);
                    HBRUSH fillBrush = CreateSolidBrush(fillColor);
                    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, fillPen));
                    HBRUSH oldBrush = static_cast<HBRUSH>(
                        SelectObject(dc, fillBrush));
                    RoundRect(dc, barX, bodyY, barX + fillWidth,
                        bodyY + bodyHeight, scale(1), scale(1));
                    SelectObject(dc, oldBrush);
                    SelectObject(dc, oldPen);
                    DeleteObject(fillBrush);
                    DeleteObject(fillPen);
                }
                else
                {
                    RECT fillRect = { barX, bodyY, barX + fillWidth,
                        bodyY + bodyHeight };
                    HBRUSH fillBrush = CreateSolidBrush(fillColor);
                    FillRect(dc, &fillRect, fillBrush);
                    DeleteObject(fillBrush);
                }
            }
        }
        else
        {
            HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
            HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
            HBRUSH oldBrush = static_cast<HBRUSH>(
                SelectObject(dc, GetStockObject(NULL_BRUSH)));

            if (row.battery)
            {
                RoundRect(dc, barX, barY, barX + bodyWidth, barY + barHeight,
                    scale(2), scale(2));
                RECT capRect = { barX + bodyWidth, barY + barHeight / 3,
                    barX + bodyWidth + scale(2),
                    barY + barHeight - barHeight / 3 };
                HBRUSH capBrush = CreateSolidBrush(borderColor);
                FillRect(dc, &capRect, capBrush);
                DeleteObject(capBrush);
            }
            else
            {
                Rectangle(dc, barX, barY, barX + bodyWidth, barY + barHeight);
            }

            if (row.online && row.percent > 0)
            {
                int inset = row.battery ? scale(2) : 1;
                if (inset < 1)
                    inset = 1;
                int innerWidth = bodyWidth - inset * 2;
                int innerHeight = barHeight - inset * 2;
                if (innerWidth > 0 && innerHeight > 0)
                {
                    int fillWidth = innerWidth * std::min(row.percent, 100) / 100;
                    if (fillWidth > 0)
                    {
                        RECT fillRect = { barX + inset, barY + inset,
                            barX + inset + fillWidth, barY + inset + innerHeight };
                        HBRUSH fillBrush = CreateSolidBrush(fillColor);
                        FillRect(dc, &fillRect, fillBrush);
                        DeleteObject(fillBrush);
                    }
                }
            }

            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }
        if (oldFont != nullptr)
            SelectObject(dc, oldFont);
        if (smallFont != nullptr)
            DeleteObject(smallFont);
        if (row.stale)
        {
            int size = BadgeSize(scale(5), badgeHeight, bodyWidth);
            DrawOfflineMark(dc, barX + bodyWidth - size - 1,
                badgeBaseY + 1, size, darkMode, rules);
        }

        SetTextColor(dc, oldTextColor);
        SetBkMode(dc, oldBkMode);
    }

    std::wstring BatteryItem::PlainText(const std::vector<BatteryEntry>& entries,
        bool hidePercent)
    {
        std::wstring text;
        for (const BatteryEntry& entry : entries)
        {
            if (!text.empty())
                text += L"  ";
            if (!entry.label.empty())
                text += entry.label + L" ";
            // Deliberately no suffix for a cached reading: the host sizes this
            // text column from GetItemValueSampleText(), so extra characters
            // would be clipped. The offline state lives in the tooltip instead.
            text += StripPercent(entry.value, hidePercent);
        }
        return text;
    }

    std::wstring BatteryItem::SampleText(const std::vector<BatteryEntry>& entries,
        bool hidePercent)
    {
        const wchar_t* sample = hidePercent ? L"100+" : L"100%+";
        std::wstring text;
        for (const BatteryEntry& entry : entries)
        {
            if (!text.empty())
                text += L"  ";
            if (!entry.label.empty())
                text += entry.label + L" ";
            text += sample;
        }
        return text;
    }
}
