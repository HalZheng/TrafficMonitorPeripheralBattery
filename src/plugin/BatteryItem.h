#pragma once

#include "../core/BatteryReading.h"

#include <windows.h>

#include <string>
#include <vector>

namespace pb
{
    /** Colours and thresholds shared by the horizontal bar and the hybrid rows. */
    struct ColorRules
    {
        int lowPercent = 20;          // battery: <= this is red
        int goodPercent = 60;         // battery: >= this is green
        int cpuRedPercent = 80;       // system usage: >= this is red
        int cpuOrangePercent = 50;    // system usage: >= this is orange
        int rowBarWidth = 48;         // hybrid row bar width at 96 DPI
        // "solid" fills the whole row height: no outline, no inset, no
        // vertical padding. "framed" keeps the old 1 px stroke + 2 px inset.
        bool frameless = true;
        COLORREF high = RGB(76, 175, 80);       // green
        COLORREF medium = RGB(255, 152, 0);     // orange
        COLORREF low = RGB(229, 57, 53);        // red
        COLORREF charging = RGB(3, 169, 244);   // cyan blue
        COLORREF offline = RGB(150, 150, 150);  // grey
        // Host-provided value text colour. CLR_INVALID (or one of the plugin's
        // own defaults) leaves every neutral tone exactly as verified; a
        // custom colour tints the track / offline badge towards it.
        COLORREF hostTextColor = CLR_INVALID;
    };

    /** One device's render state. Plain data, replaced on every DataRequired. */
    struct BatteryEntry
    {
        std::wstring id;
        std::wstring name;
        std::wstring label;
        std::wstring value;        // "84%", "84%+" or "--"
        int percent = -1;
        bool online = false;
        bool charging = false;
        // Cached last-known level: draw exactly like a live reading, plus a
        // small offline badge. Colours, value text and fill width are unchanged.
        bool stale = false;
        // CLR_INVALID means "colour by percentage"; a real colour overrides it.
        COLORREF color = CLR_INVALID;
    };

    /** One line of the hybrid layout (CPU, memory or one battery). */
    struct RowEntry
    {
        std::wstring label;
        std::wstring value;
        int percent = -1;
        bool online = false;
        bool charging = false;
        bool stale = false;
        bool battery = false;       // mini battery outline instead of a plain bar
        COLORREF fixedColor = CLR_INVALID;
    };

    /**
     * One grid for all stacked rows. Everything is derived from the measured
     * label column, so the system bar and the first battery icon share the
     * exact same left edge, and the last icon/value ends at the exact right
     * edge. No column is ever stretched, so no trailing blank space appears.
     */
    struct StackedLayout
    {
        int labelWidth = 0;   // CPU / MEM / device label column
        int gap = 0;          // label -> bar or battery icon
        int iconWidth = 0;    // battery body plus its 2 px cap
        int valueGap = 0;     // icon/bar -> percentage (0 when hidden)
        int valueWidth = 0;   // percentage column (0 when hidden)
        int cellGap = 0;      // between the two battery cells
        int cellWidth = 0;    // one battery cell
        int width = 0;        // total, exactly what GetItemWidthEx reports
    };

    /**
     * Drawing helpers for a single battery group.
     *
     * TrafficMonitor caches IPluginItem pointers at startup, so the plugin now
     * exposes exactly one item (BatteryGroupItem) that draws a variable number
     * of these groups. This class is deliberately not an IPluginItem any more.
     */
    class BatteryItem
    {
    public:
        static COLORREF ParseColor(const std::wstring& text, COLORREF fallback);

        /** Tint a neutral tone towards a custom host text colour; unchanged by default. */
        static COLORREF NeutralColor(COLORREF fallback, COLORREF hostTextColor);

        static COLORREF AdaptForTheme(COLORREF color, bool darkMode);
        static COLORREF LevelColor(int percent, bool charging, bool online,
            bool darkMode, const ColorRules& rules);
        static COLORREF SystemColor(int percent, bool darkMode,
            const ColorRules& rules);

        static int MeasureEntry(HDC dc, const BatteryEntry& entry, bool hidePercent);
        static void DrawEntry(HDC dc, int x, int y, int w, int h,
            const BatteryEntry& entry, bool darkMode, bool hidePercent,
            COLORREF labelColor, COLORREF valueColor, const ColorRules& rules);
        static int GroupGap(HDC dc);

        static int RowWidth(HDC dc, const ColorRules& rules);
        static StackedLayout MakeStackedLayout(HDC dc, const ColorRules& rules,
            const std::vector<BatteryEntry>& entries, bool showPercent);
        static int StackedWidth(HDC dc, const ColorRules& rules,
            const std::vector<BatteryEntry>& entries, bool showPercent);
        static void DrawBatteryIcon(HDC dc, int x, int y, int w, int h,
            const BatteryEntry& entry, bool darkMode, COLORREF textColor,
            const ColorRules& rules);
        static int HybridMinHeight(HDC dc);
        static void DrawRow(HDC dc, int x, int y, int w, int h,
            const RowEntry& row, bool compact, bool darkMode,
            COLORREF labelColor, COLORREF valueColor, const ColorRules& rules);

        static std::wstring PlainText(const std::vector<BatteryEntry>& entries,
            bool hidePercent);
        static std::wstring SampleText(const std::vector<BatteryEntry>& entries,
            bool hidePercent);
    };
}
