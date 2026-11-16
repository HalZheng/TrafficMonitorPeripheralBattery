#include "BatteryGroupItem.h"

#include <windows.h>

#include <algorithm>
#include <utility>

namespace pb
{
    BatteryGroupItem::BatteryGroupItem() = default;

    void BatteryGroupItem::SetEntries(std::vector<BatteryEntry> entries)
    {
        m_entries = std::move(entries);
    }

    void BatteryGroupItem::SetDisplayMode(const std::wstring& mode)
    {
        m_displayMode = mode;
    }

    void BatteryGroupItem::SetDisplayOptions(bool hidePercent,
        COLORREF labelColor, COLORREF valueColor)
    {
        m_hidePercent = hidePercent;
        m_labelColor = labelColor;
        m_valueColor = valueColor;
    }

    void BatteryGroupItem::SetColorRules(const ColorRules& rules)
    {
        m_rules = rules;
    }

    void BatteryGroupItem::SetHybridRows(int rows)
    {
        m_hybridRows = std::max(2, std::min(4, rows));
    }

    void BatteryGroupItem::SetStackedOptions(bool showPercent)
    {
        m_stackedShowPercent = showPercent;
    }

    void BatteryGroupItem::SetSystemUsage(int cpuUsage, int memoryUsage)
    {
        m_cpuUsage = cpuUsage;
        m_memoryUsage = memoryUsage;
    }

    bool BatteryGroupItem::IsCustomDraw() const
    {
        return m_displayMode != L"text";
    }

    int BatteryGroupItem::IsDoubleLineExclusive() const
    {
        // The host only honours this when "horizontal_arrange" is off (or the
        // taskbar is on a screen edge). It hands the item the full 32 px height.
        return (m_displayMode == L"hybrid" || m_displayMode == L"stacked")
            ? 1 : 0;
    }

    const wchar_t* BatteryGroupItem::GetItemValueText() const
    {
        m_valueCache = BatteryItem::PlainText(m_entries, m_hidePercent);
        return m_valueCache.c_str();
    }

    const wchar_t* BatteryGroupItem::GetItemValueSampleText() const
    {
        m_sampleCache = BatteryItem::SampleText(m_entries, m_hidePercent);
        return m_sampleCache.c_str();
    }

    int BatteryGroupItem::GetItemWidthEx(void* hDC) const
    {
        HDC dc = static_cast<HDC>(hDC);
        if (dc == nullptr || !IsCustomDraw())
            return 0;

        // In hybrid mode every row shares one column: report the widest row,
        // not the sum. This is what actually saves taskbar horizontal space.
        if (m_displayMode == L"hybrid")
            return BatteryItem::RowWidth(dc, m_rules);
        if (m_displayMode == L"stacked")
            return BatteryItem::StackedWidth(dc, m_rules, m_entries,
                m_stackedShowPercent);

        if (m_entries.empty())
            return 0;

        int gap = BatteryItem::GroupGap(dc);
        int width = 0;
        for (const BatteryEntry& entry : m_entries)
            width += BatteryItem::MeasureEntry(dc, entry, m_hidePercent) + gap;
        return width > gap ? width - gap : width;
    }

    void BatteryGroupItem::DrawItem(void* hDC, int x, int y, int w, int h,
        bool dark_mode)
    {
        HDC dc = static_cast<HDC>(hDC);
        if (dc == nullptr || w <= 0 || h <= 0)
            return;

        if (m_displayMode == L"hybrid")
        {
            DrawHybrid(dc, x, y, w, h, dark_mode);
            return;
        }
        if (m_displayMode == L"stacked")
        {
            DrawStacked(dc, x, y, w, h, dark_mode);
            return;
        }
        DrawHorizontal(dc, x, y, w, h, dark_mode);
    }

    void BatteryGroupItem::DrawHorizontal(HDC dc, int x, int y, int w, int h,
        bool darkMode) const
    {
        if (m_entries.empty())
            return;

        int gap = BatteryItem::GroupGap(dc);
        int cursor = x;
        for (const BatteryEntry& entry : m_entries)
        {
            int entryWidth = BatteryItem::MeasureEntry(dc, entry, m_hidePercent);
            if (cursor + entryWidth > x + w)
                break;

            BatteryItem::DrawEntry(dc, cursor, y, entryWidth, h, entry,
                darkMode, m_hidePercent, m_labelColor, m_valueColor, m_rules);
            cursor += entryWidth + gap;
        }
    }

    RowEntry BatteryGroupItem::MakeSystemRow(const wchar_t* label, int usage) const
    {
        RowEntry row;
        row.label = label;
        row.online = usage >= 0 && usage <= 100;
        row.percent = row.online ? usage : -1;
        row.value = row.online ? std::to_wstring(usage) + L"%" : L"--";
        row.battery = false;
        return row;
    }

    RowEntry BatteryGroupItem::MakeBatteryRow(const BatteryEntry& entry) const
    {
        RowEntry row;
        row.label = entry.label;
        row.value = entry.value;
        if (m_hidePercent)
        {
            size_t percent = row.value.find(L'%');
            if (percent != std::wstring::npos)
                row.value.erase(percent, 1);
        }
        row.percent = entry.percent;
        row.online = entry.online;
        row.charging = entry.charging;
        row.stale = entry.stale;
        row.battery = true;
        row.fixedColor = entry.color;
        return row;
    }

    void BatteryGroupItem::DrawHybrid(HDC dc, int x, int y, int w, int h,
        bool darkMode) const
    {
        const int batterySlots = std::min(m_hybridRows - 2, 2);
        const size_t batteryCount = std::min(
            static_cast<size_t>(std::max(0, batterySlots)), m_entries.size());
        const int rowCount = 2 + static_cast<int>(batteryCount);
        const int rowHeight = h / rowCount;
        if (rowHeight <= 0)
            return;

        // If the host still uses horizontal_arrange (21 px), drop the tiny
        // labels but keep the four bars readable as pure level indicators.
        const bool compact = h < BatteryItem::HybridMinHeight(dc);

        std::vector<RowEntry> rows;
        rows.reserve(rowCount);
        rows.push_back(MakeSystemRow(L"CPU", m_cpuUsage));
        rows.push_back(MakeSystemRow(L"MEM", m_memoryUsage));
        for (size_t i = 0; i < batteryCount; ++i)
            rows.push_back(MakeBatteryRow(m_entries[i]));

        for (int i = 0; i < rowCount; ++i)
        {
            BatteryItem::DrawRow(dc, x, y + i * rowHeight, w, rowHeight,
                rows[i], compact, darkMode, m_labelColor, m_valueColor, m_rules);
        }
    }

    void BatteryGroupItem::DrawStacked(HDC dc, int x, int y, int w, int h,
        bool darkMode) const
    {
        const int batteryCount = static_cast<int>(
            std::min<size_t>(2u, m_entries.size()));
        const int rowCount = batteryCount > 0 ? 3 : 2;
        const int systemHeight = h / rowCount;
        if (systemHeight <= 0)
            return;
        const int lastHeight = h - systemHeight * (rowCount - 1);
        const bool compact = h < BatteryItem::HybridMinHeight(dc);

        if (compact)
        {
            // Not enough height for text: two system bars plus two distinct
            // battery silhouettes, sharing the available width evenly.
            RowEntry cpu = MakeSystemRow(L"CPU", m_cpuUsage);
            RowEntry mem = MakeSystemRow(L"MEM", m_memoryUsage);
            BatteryItem::DrawRow(dc, x, y, w, systemHeight, cpu, true,
                darkMode, m_labelColor, m_valueColor, m_rules);
            BatteryItem::DrawRow(dc, x, y + systemHeight, w, systemHeight,
                mem, true, darkMode, m_labelColor, m_valueColor, m_rules);
            if (batteryCount == 0)
                return;
            int gap = MulDiv(8, GetDeviceCaps(dc, LOGPIXELSY), 96);
            int cellWidth = (w - gap * (batteryCount - 1)) / batteryCount;
            for (int i = 0; i < batteryCount; ++i)
            {
                RowEntry row = MakeBatteryRow(m_entries[i]);
                BatteryItem::DrawRow(dc, x + i * (cellWidth + gap),
                    y + systemHeight * 2, cellWidth, lastHeight, row, true,
                    darkMode, m_labelColor, m_valueColor, m_rules);
            }
            return;
        }

        // One shared grid for all three rows. The system bar and the first
        // battery icon start at exactly barX; the last icon (or value) ends
        // at exactly x + layout.width. No column is stretched, so there is no
        // trailing blank space after either battery group.
        const bool showPercent = m_stackedShowPercent;
        const StackedLayout layout = BatteryItem::MakeStackedLayout(
            dc, m_rules, m_entries, showPercent);

        const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        const int fontHeight = std::max(7,
            std::min(MulDiv(9, dpi, 96), systemHeight - 1));
        HFONT font = CreateFontW(-fontHeight, 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = font != nullptr
            ? static_cast<HFONT>(SelectObject(dc, font)) : nullptr;

        COLORREF defaultText = darkMode ? RGB(235, 235, 235) : RGB(30, 30, 30);
        COLORREF labelColor = m_labelColor != CLR_INVALID
            ? m_labelColor : defaultText;
        COLORREF valueColor = m_valueColor != CLR_INVALID
            ? m_valueColor : defaultText;
        int oldBkMode = SetBkMode(dc, TRANSPARENT);
        COLORREF oldTextColor = SetTextColor(dc, labelColor);

        const int barX = x + layout.labelWidth + layout.gap;
        const int barRight = x + layout.width
            - (showPercent ? layout.valueGap + layout.valueWidth : 0);
        const int barWidth = std::max(0, barRight - barX);

        for (int i = 0; i < 2; ++i)
        {
            const int rowY = y + i * systemHeight;
            const int rowH = (batteryCount == 0 && i == 1)
                ? lastHeight : systemHeight;
            int usage = i == 0 ? m_cpuUsage : m_memoryUsage;
            if (usage < 0 || usage > 100)
                usage = -1;

            SetTextColor(dc, labelColor);
            RECT labelRect = { x, rowY, x + layout.labelWidth, rowY + rowH };
            DrawTextW(dc, i == 0 ? L"CPU" : L"MEM", -1, &labelRect,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT);

            if (usage >= 0 && barWidth > 0)
            {
                int bodyHeight = std::max(4, rowH - 1);
                int bodyY = rowY + (rowH - bodyHeight) / 2;
                int fillWidth = barWidth * usage / 100;
                if (fillWidth > 0)
                {
                    RECT fillRect = { barX, bodyY, barX + fillWidth,
                        bodyY + bodyHeight };
                    HBRUSH brush = CreateSolidBrush(
                        BatteryItem::SystemColor(usage, darkMode, m_rules));
                    FillRect(dc, &fillRect, brush);
                    DeleteObject(brush);
                }
            }

            if (showPercent)
            {
                std::wstring value = usage >= 0
                    ? std::to_wstring(usage) + (m_hidePercent ? L"" : L"%")
                    : std::wstring(L"--");
                RECT valueRect = { x + layout.width - layout.valueWidth,
                    rowY, x + layout.width, rowY + rowH };
                SetTextColor(dc, valueColor);
                DrawTextW(dc, value.c_str(), -1, &valueRect,
                    DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RIGHT);
            }
        }

        if (batteryCount > 0)
        {
            const int rowY = y + systemHeight * 2;
            const int rowH = lastHeight;
            for (int i = 0; i < batteryCount; ++i)
            {
                const int cellX = x + i * (layout.cellWidth + layout.cellGap);
                SetTextColor(dc, labelColor);
                RECT labelRect = { cellX, rowY,
                    cellX + layout.labelWidth, rowY + rowH };
                DrawTextW(dc, m_entries[i].label.c_str(), -1, &labelRect,
                    DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT |
                    DT_END_ELLIPSIS);

                const int iconX = cellX + layout.labelWidth + layout.gap;
                BatteryItem::DrawBatteryIcon(dc, iconX, rowY,
                    layout.iconWidth, rowH, m_entries[i], darkMode,
                    valueColor, m_rules);

                if (showPercent)
                {
                    std::wstring value = m_entries[i].value;
                    if (m_hidePercent)
                    {
                        size_t percent = value.find(L'%');
                        if (percent != std::wstring::npos)
                            value.erase(percent, 1);
                    }
                    RECT valueRect = {
                        iconX + layout.iconWidth + layout.valueGap, rowY,
                        cellX + layout.cellWidth, rowY + rowH };
                    SetTextColor(dc, valueColor);
                    DrawTextW(dc, value.c_str(), -1, &valueRect,
                        DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RIGHT |
                        DT_END_ELLIPSIS);
                }
            }
        }

        SetTextColor(dc, oldTextColor);
        SetBkMode(dc, oldBkMode);
        if (oldFont != nullptr)
            SelectObject(dc, oldFont);
        if (font != nullptr)
            DeleteObject(font);
    }
}
