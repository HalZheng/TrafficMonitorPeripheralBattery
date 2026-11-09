#pragma once

#include "PluginInterface.h"

#include "../core/BatteryReading.h"
#include "../core/DeviceDescriptor.h"

#include <windows.h>

#include <string>

namespace pb
{
    class BatteryItem : public IPluginItem
    {
    public:
        BatteryItem(const DeviceDescriptor& device, std::wstring itemName,
            std::wstring label, bool customDraw, COLORREF color);

        void SetReading(const BatteryReading& reading, const std::wstring& value);
        void SetLabel(const std::wstring& label) { m_label = label; }
        void SetItemName(const std::wstring& name) { m_itemName = name; }
        void SetDisplayOptions(bool hidePercent, COLORREF labelColor,
            COLORREF valueColor);

        const std::wstring& DeviceId() const { return m_device.id; }

        static COLORREF ParseColor(const std::wstring& text, COLORREF fallback);

        const wchar_t* GetItemName() const override { return m_itemName.c_str(); }
        const wchar_t* GetItemId() const override { return m_device.id.c_str(); }
        const wchar_t* GetItemLableText() const override { return m_label.c_str(); }
        const wchar_t* GetItemValueText() const override;
        const wchar_t* GetItemValueSampleText() const override { return L"100%+"; }

        bool IsCustomDraw() const override { return m_customDraw; }
        int GetItemWidthEx(void* hDC) const override;
        void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;

    private:
        std::wstring BuildValue() const;

        DeviceDescriptor m_device;
        std::wstring m_itemName;
        std::wstring m_label;
        std::wstring m_value = L"--";
        mutable std::wstring m_valueCache;
        bool m_customDraw = false;
        COLORREF m_color = RGB(79, 195, 247);
        COLORREF m_labelColor = CLR_INVALID;
        COLORREF m_valueColor = CLR_INVALID;
        bool m_hidePercent = false;
        bool m_online = false;
        bool m_charging = false;
        int m_percent = -1;
    };
}
