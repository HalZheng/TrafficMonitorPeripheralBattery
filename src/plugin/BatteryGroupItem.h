#pragma once

#include "PluginInterface.h"

#include "BatteryItem.h"

#include <string>
#include <vector>

namespace pb
{
    /**
     * The plugin's single, permanent display item.
     *
     * TrafficMonitor enumerates GetItem() once at startup and caches the raw
     * pointers, and keys the host's display settings off GetItemId(). Both must
     * stay constant for the whole process. The number of batteries shown is
     * therefore a property of the entries we draw, not of the item list.
     *
     * The host recalculates custom-draw widths every second, so changing the
     * entries (hot-plug, settings, hide, order) takes effect without a restart.
     */
    class BatteryGroupItem : public IPluginItem
    {
    public:
        BatteryGroupItem();

        void SetEntries(std::vector<BatteryEntry> entries);
        void SetDisplayMode(const std::wstring& mode);
        void SetDisplayOptions(bool hidePercent, COLORREF labelColor,
            COLORREF valueColor);
        void SetColorRules(const ColorRules& rules);
        void SetHybridRows(int rows);
        void SetStackedOptions(bool showPercent);
        void SetSystemUsage(int cpuUsage, int memoryUsage);

        const wchar_t* GetItemName() const override { return m_itemName.c_str(); }
        const wchar_t* GetItemId() const override { return m_itemId.c_str(); }
        const wchar_t* GetItemLableText() const override { return L""; }
        const wchar_t* GetItemValueText() const override;
        const wchar_t* GetItemValueSampleText() const override;

        bool IsCustomDraw() const override;
        int GetItemWidthEx(void* hDC) const override;
        void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
        int IsDoubleLineExclusive() const override;

    private:
        void DrawHybrid(HDC dc, int x, int y, int w, int h, bool darkMode) const;
        void DrawStacked(HDC dc, int x, int y, int w, int h, bool darkMode) const;
        void DrawHorizontal(HDC dc, int x, int y, int w, int h, bool darkMode) const;
        RowEntry MakeSystemRow(const wchar_t* label, int usage) const;
        RowEntry MakeBatteryRow(const BatteryEntry& entry) const;

        std::vector<BatteryEntry> m_entries;
        std::wstring m_itemName = L"无线外设电量";
        std::wstring m_itemId = L"peripheral_battery";
        std::wstring m_displayMode = L"bar";
        mutable std::wstring m_valueCache;
        mutable std::wstring m_sampleCache;
        bool m_hidePercent = false;
        int m_cpuUsage = -1;
        int m_memoryUsage = -1;
        int m_hybridRows = 4;
        bool m_stackedShowPercent = false;
        ColorRules m_rules;
        COLORREF m_labelColor = CLR_INVALID;
        COLORREF m_valueColor = CLR_INVALID;
    };
}
