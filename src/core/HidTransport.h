#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace pb
{
    struct HidInterfaceInfo
    {
        std::wstring path;
        std::wstring product;
        std::wstring manufacturer;
        unsigned short vid = 0;
        unsigned short pid = 0;
        unsigned short usagePage = 0;
        unsigned short usage = 0;
        unsigned short inputLength = 0;
        unsigned short outputLength = 0;
        unsigned short featureLength = 0;
    };

    /**
     * Thin RAII wrapper around a HID collection.
     *
     * All reads/writes use overlapped I/O with an explicit timeout so a sleeping
     * wireless device can never block the plugin's worker thread indefinitely.
     */
    class HidDevice
    {
    public:
        HidDevice() = default;
        ~HidDevice();

        HidDevice(HidDevice&& other) noexcept;
        HidDevice& operator=(HidDevice&& other) noexcept;
        HidDevice(const HidDevice&) = delete;
        HidDevice& operator=(const HidDevice&) = delete;

        bool Open(unsigned short vid, unsigned short pid,
            unsigned short usagePage, unsigned short usage, bool overlapped = true);
        bool OpenPath(const std::wstring& path, bool overlapped = true);
        void Close();

        bool IsOpen() const { return m_handle != INVALID_HANDLE_VALUE; }

        bool Write(const std::vector<unsigned char>& data, unsigned int timeoutMs) const;
        bool Read(std::vector<unsigned char>& data, unsigned int timeoutMs) const;
        bool GetFeature(std::vector<unsigned char>& data) const;

        const HidInterfaceInfo& Info() const { return m_info; }
        unsigned short InputLength() const { return m_info.inputLength; }
        unsigned short OutputLength() const { return m_info.outputLength; }
        unsigned short FeatureLength() const { return m_info.featureLength; }

    private:
        bool OpenHandle(const std::wstring& path, bool overlapped);
        void LoadInfo();

        HANDLE m_handle = INVALID_HANDLE_VALUE;
        HidInterfaceInfo m_info;
    };

    std::vector<HidInterfaceInfo> EnumerateHidInterfaces();
}
