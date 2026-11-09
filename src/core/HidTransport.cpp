#include "HidTransport.h"

#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <cstdint>
#include <cstring>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace pb
{
    namespace
    {
        std::wstring ReadHidString(HANDLE handle,
            BOOLEAN(WINAPI* getter)(HANDLE, PVOID, ULONG))
        {
            wchar_t buffer[256] = {};
            if (getter(handle, buffer, sizeof(buffer)))
                return buffer;
            return L"";
        }

        bool WaitForOverlapped(HANDLE handle, OVERLAPPED& overlapped, DWORD timeoutMs)
        {
            if (WaitForSingleObject(overlapped.hEvent, timeoutMs) != WAIT_OBJECT_0)
            {
                CancelIo(handle);
                DWORD transferred = 0;
                GetOverlappedResult(handle, &overlapped, &transferred, TRUE);
                return false;
            }
            return true;
        }
    }

    HidDevice::~HidDevice()
    {
        Close();
    }

    HidDevice::HidDevice(HidDevice&& other) noexcept
        : m_handle(other.m_handle), m_info(std::move(other.m_info))
    {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    HidDevice& HidDevice::operator=(HidDevice&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_handle = other.m_handle;
            m_info = std::move(other.m_info);
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    void HidDevice::Close()
    {
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
        m_info = HidInterfaceInfo{};
    }

    bool HidDevice::Open(unsigned short vid, unsigned short pid,
        unsigned short usagePage, unsigned short usage, bool overlapped)
    {
        std::vector<HidInterfaceInfo> interfaces = EnumerateHidInterfaces();
        for (const HidInterfaceInfo& info : interfaces)
        {
            if (info.vid == vid && info.pid == pid &&
                info.usagePage == usagePage && info.usage == usage)
            {
                return OpenPath(info.path, overlapped);
            }
        }
        return false;
    }

    bool HidDevice::OpenPath(const std::wstring& path, bool overlapped)
    {
        Close();
        if (!OpenHandle(path, overlapped))
            return false;
        LoadInfo();
        return true;
    }

    bool HidDevice::OpenHandle(const std::wstring& path, bool overlapped)
    {
        DWORD flags = overlapped ? FILE_FLAG_OVERLAPPED : 0;
        m_handle = CreateFileW(path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, flags, nullptr);

        if (m_handle == INVALID_HANDLE_VALUE)
        {
            // Feature-only collections may reject write access.
            m_handle = CreateFileW(path.c_str(), 0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, flags, nullptr);
        }

        return m_handle != INVALID_HANDLE_VALUE;
    }

    void HidDevice::LoadInfo()
    {
        m_info = HidInterfaceInfo{};
        if (m_handle == INVALID_HANDLE_VALUE)
            return;

        HIDD_ATTRIBUTES attributes = {};
        attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(m_handle, &attributes))
            return;

        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(m_handle, &preparsed))
            return;

        HIDP_CAPS caps = {};
        NTSTATUS status = HidP_GetCaps(preparsed, &caps);
        HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS)
            return;

        m_info.vid = attributes.VendorID;
        m_info.pid = attributes.ProductID;
        m_info.usagePage = caps.UsagePage;
        m_info.usage = caps.Usage;
        m_info.inputLength = caps.InputReportByteLength;
        m_info.outputLength = caps.OutputReportByteLength;
        m_info.featureLength = caps.FeatureReportByteLength;
        m_info.product = ReadHidString(m_handle, HidD_GetProductString);
        m_info.manufacturer = ReadHidString(m_handle, HidD_GetManufacturerString);
    }

    bool HidDevice::Write(const std::vector<unsigned char>& data, unsigned int timeoutMs) const
    {
        if (m_handle == INVALID_HANDLE_VALUE || data.empty() ||
            data.size() != static_cast<size_t>(m_info.outputLength))
            return false;

        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (overlapped.hEvent == nullptr)
            return false;

        DWORD written = 0;
        BOOL ok = WriteFile(m_handle, data.data(), static_cast<DWORD>(data.size()),
            nullptr, &overlapped);
        if (!ok)
        {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING)
            {
                CloseHandle(overlapped.hEvent);
                return false;
            }
            if (!WaitForOverlapped(m_handle, overlapped, timeoutMs) ||
                !GetOverlappedResult(m_handle, &overlapped, &written, FALSE))
            {
                CloseHandle(overlapped.hEvent);
                return false;
            }
        }

        CloseHandle(overlapped.hEvent);
        return true;
    }

    bool HidDevice::Read(std::vector<unsigned char>& data, unsigned int timeoutMs) const
    {
        data.clear();
        if (m_handle == INVALID_HANDLE_VALUE || m_info.inputLength == 0)
            return false;

        data.resize(m_info.inputLength);
        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (overlapped.hEvent == nullptr)
            return false;

        DWORD read = 0;
        BOOL ok = ReadFile(m_handle, data.data(), static_cast<DWORD>(data.size()),
            nullptr, &overlapped);
        if (!ok)
        {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING)
            {
                CloseHandle(overlapped.hEvent);
                return false;
            }
            if (!WaitForOverlapped(m_handle, overlapped, timeoutMs) ||
                !GetOverlappedResult(m_handle, &overlapped, &read, FALSE))
            {
                CloseHandle(overlapped.hEvent);
                return false;
            }
        }

        CloseHandle(overlapped.hEvent);
        return true;
    }

    bool HidDevice::GetFeature(std::vector<unsigned char>& data) const
    {
        if (m_handle == INVALID_HANDLE_VALUE || data.empty())
            return false;
        return HidD_GetFeature(m_handle, data.data(), static_cast<ULONG>(data.size())) != FALSE;
    }

    std::vector<HidInterfaceInfo> EnumerateHidInterfaces()
    {
        std::vector<HidInterfaceInfo> result;

        GUID hidGuid;
        HidD_GetHidGuid(&hidGuid);
        HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (deviceInfoSet == INVALID_HANDLE_VALUE)
            return result;

        SP_DEVICE_INTERFACE_DATA interfaceData = {};
        interfaceData.cbSize = sizeof(interfaceData);

        for (DWORD index = 0;
            SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid, index, &interfaceData);
            ++index)
        {
            DWORD required = 0;
            SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData,
                nullptr, 0, &required, nullptr);
            if (required == 0)
                continue;

            std::vector<BYTE> buffer(required + 16);
            uintptr_t address = reinterpret_cast<uintptr_t>(buffer.data());
            address = (address + 7) & ~static_cast<uintptr_t>(7);
            SP_DEVICE_INTERFACE_DETAIL_DATA_W* detail =
                reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(address);
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            if (!SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData,
                detail, required, nullptr, nullptr))
                continue;

            HidDevice device;
            if (device.OpenPath(detail->DevicePath, false))
            {
                HidInterfaceInfo info = device.Info();
                info.path = detail->DevicePath;
                result.push_back(std::move(info));
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return result;
    }
}
