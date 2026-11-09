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
    }

    BatteryItem::BatteryItem(const DeviceDescriptor& device, std::wstring itemName,
        std::wstring label, bool customDraw, COLORREF color)
        : m_device(device)
        , m_itemName(std::move(itemName))
        , m_label(std::move(label))
        , m_customDraw(customDraw)
        , m_color(color)
    {
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

    void BatteryItem::SetReading(const BatteryReading& reading,
        const std::wstring& value)
    {
        m_value = value;
        m_online = reading.HasPercent();
        m_percent = reading.percent;
        m_charging = reading.charge == ChargeState::Charging;
    }

    void BatteryItem::SetDisplayOptions(bool hidePercent, COLORREF labelColor,
        COLORREF valueColor)
    {
        m_hidePercent = hidePercent;
        m_labelColor = labelColor;
        m_valueColor = valueColor;
    }

    std::wstring BatteryItem::BuildValue() const
    {
        std::wstring value = m_value;
        if (m_hidePercent)
        {
            size_t percent = value.find(L'%');
            if (percent != std::wstring::npos)
                value.erase(percent, 1);
        }
        return value;
    }

    const wchar_t* BatteryItem::GetItemValueText() const
    {
        m_valueCache = BuildValue();
        return m_valueCache.c_str();
    }

    int BatteryItem::GetItemWidthEx(void* hDC) const
    {
        HDC dc = static_cast<HDC>(hDC);
        if (dc == nullptr || !m_customDraw)
            return 0;

        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };

        SIZE labelSize = {};
        if (!m_label.empty())
            GetTextExtentPoint32W(dc, m_label.c_str(),
                static_cast<int>(m_label.size()), &labelSize);

        SIZE valueSize = {};
        const wchar_t* sample = m_hidePercent ? L"100+" : L"100%+";
        GetTextExtentPoint32W(dc, sample,
            static_cast<int>(wcslen(sample)), &valueSize);

        int width = labelSize.cx;
        if (!m_label.empty())
            width += scale(4);
        width += scale(18) + scale(4) + valueSize.cx + scale(2);
        return width;
    }

    void BatteryItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
    {
        HDC dc = static_cast<HDC>(hDC);
        if (dc == nullptr)
            return;

        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };

        int oldBkMode = SetBkMode(dc, TRANSPARENT);
        COLORREF defaultTextColor = dark_mode ? RGB(235, 235, 235) : RGB(30, 30, 30);
        COLORREF labelColor = m_labelColor != CLR_INVALID
            ? m_labelColor : defaultTextColor;
        COLORREF valueColor = m_valueColor != CLR_INVALID
            ? m_valueColor : defaultTextColor;
        COLORREF oldTextColor = SetTextColor(dc, labelColor);

        SIZE labelSize = {};
        if (!m_label.empty())
            GetTextExtentPoint32W(dc, m_label.c_str(),
                static_cast<int>(m_label.size()), &labelSize);

        int contentX = x;
        if (!m_label.empty())
        {
            RECT labelRect = { x, y, x + labelSize.cx, y + h };
            DrawTextW(dc, m_label.c_str(), -1, &labelRect,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT);
            contentX += labelSize.cx + scale(4);
        }

        const int iconWidth = scale(16);
        const int iconHeight = scale(10);
        const int iconX = contentX;
        const int iconY = y + (h - iconHeight) / 2;

        COLORREF borderColor = m_online
            ? valueColor
            : (dark_mode ? RGB(130, 130, 130) : RGB(150, 150, 150));

        COLORREF fillColor = m_color;
        if (!m_online)
            fillColor = dark_mode ? RGB(90, 90, 90) : RGB(190, 190, 190);
        else if (m_charging)
            fillColor = RGB(76, 175, 80);
        else if (m_percent >= 0 && m_percent <= 20)
            fillColor = RGB(229, 57, 53);

        HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
        HBRUSH oldBrush = static_cast<HBRUSH>(
            SelectObject(dc, GetStockObject(NULL_BRUSH)));

        RoundRect(dc, iconX, iconY, iconX + iconWidth, iconY + iconHeight,
            scale(3), scale(3));

        HBRUSH capBrush = CreateSolidBrush(borderColor);
        SelectObject(dc, capBrush);
        Rectangle(dc, iconX + iconWidth, iconY + scale(3),
            iconX + iconWidth + scale(2), iconY + iconHeight - scale(3));

        SelectObject(dc, oldBrush);

        if (m_online && m_percent > 0)
        {
            int innerX = iconX + scale(2);
            int innerY = iconY + scale(2);
            int innerWidth = iconWidth - scale(4);
            int innerHeight = iconHeight - scale(4);
            int fillWidth = innerWidth * std::min(m_percent, 100) / 100;
            if (fillWidth > 0)
            {
                RECT fillRect = { innerX, innerY,
                    innerX + fillWidth, innerY + innerHeight };
                HBRUSH fillBrush = CreateSolidBrush(fillColor);
                FillRect(dc, &fillRect, fillBrush);
                DeleteObject(fillBrush);
            }
        }

        SelectObject(dc, oldPen);
        DeleteObject(pen);
        DeleteObject(capBrush);

        int valueX = iconX + iconWidth + scale(2) + scale(4);
        RECT valueRect = { valueX, y, x + w, y + h };
        SetTextColor(dc, valueColor);
        std::wstring value = BuildValue();
        DrawTextW(dc, value.c_str(), -1, &valueRect,
            DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT | DT_END_ELLIPSIS);

        SetTextColor(dc, oldTextColor);
        SetBkMode(dc, oldBkMode);
    }
}
