#include "FlydigiPipeProtocol.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace pb
{
    namespace
    {
        const wchar_t* kPipeName = L"\\\\.\\pipe\\fcs.sock";
        const unsigned char kRequest[] = { 0x08, 0x81, 0x20, 0x1A, 0x00 };

        bool ReadVarint(const std::vector<unsigned char>& data, size_t& position,
            uint64_t& value)
        {
            value = 0;
            int shift = 0;
            while (position < data.size() && shift <= 63)
            {
                unsigned char byte = data[position++];
                value |= static_cast<uint64_t>(byte & 0x7F) << shift;
                if ((byte & 0x80) == 0)
                    return true;
                shift += 7;
            }
            return false;
        }

        bool ReadLengthDelimited(const std::vector<unsigned char>& data, size_t& position,
            std::vector<unsigned char>& out)
        {
            uint64_t length = 0;
            if (!ReadVarint(data, position, length))
                return false;
            if (length > data.size() - position)
                return false;
            size_t count = static_cast<size_t>(length);
            out.assign(data.begin() + position, data.begin() + position + count);
            position += count;
            return true;
        }

        bool SkipField(const std::vector<unsigned char>& data, size_t& position,
            int wireType)
        {
            if (wireType == 0)
            {
                uint64_t ignored = 0;
                return ReadVarint(data, position, ignored);
            }
            if (wireType == 1)
            {
                if (data.size() - position < 8)
                    return false;
                position += 8;
                return true;
            }
            if (wireType == 2)
            {
                std::vector<unsigned char> ignored;
                return ReadLengthDelimited(data, position, ignored);
            }
            if (wireType == 5)
            {
                if (data.size() - position < 4)
                    return false;
                position += 4;
                return true;
            }
            return false;
        }

        std::wstring Utf8ToWide(const std::vector<unsigned char>& bytes)
        {
            if (bytes.empty())
                return L"";
            int length = MultiByteToWideChar(CP_UTF8, 0,
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<int>(bytes.size()), nullptr, 0);
            if (length <= 0)
                return L"";
            std::wstring result(length, L'\0');
            MultiByteToWideChar(CP_UTF8, 0,
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<int>(bytes.size()), &result[0], length);
            return result;
        }

        bool ParseDeviceInfo(const std::vector<unsigned char>& data,
            FlydigiPipeProtocol::DeviceInfo& info)
        {
            size_t position = 0;
            while (position < data.size())
            {
                uint64_t tag = 0;
                if (!ReadVarint(data, position, tag))
                    return false;
                int field = static_cast<int>(tag >> 3);
                int wire = static_cast<int>(tag & 0x07);

                if (field == 1 && wire == 2)
                {
                    std::vector<unsigned char> value;
                    if (!ReadLengthDelimited(data, position, value))
                        return false;
                    info.productName = Utf8ToWide(value);
                }
                else if (field == 2 && wire == 0)
                {
                    uint64_t value = 0;
                    if (!ReadVarint(data, position, value))
                        return false;
                    info.category = static_cast<int>(value);
                }
                else if (field == 3 && wire == 0)
                {
                    uint64_t value = 0;
                    if (!ReadVarint(data, position, value))
                        return false;
                    info.connectType = static_cast<int>(value);
                }
                else if (field == 8 && wire == 0)
                {
                    uint64_t value = 0;
                    if (!ReadVarint(data, position, value))
                        return false;
                    info.connected = value != 0;
                }
                else if (field == 9 && wire == 0)
                {
                    uint64_t value = 0;
                    if (!ReadVarint(data, position, value))
                        return false;
                    info.batteryLevel = static_cast<int>(value);
                }
                else if (!SkipField(data, position, wire))
                {
                    return false;
                }
            }
            return true;
        }

        bool ParseDeviceList(const std::vector<unsigned char>& data,
            std::vector<FlydigiPipeProtocol::DeviceInfo>& devices)
        {
            size_t position = 0;
            while (position < data.size())
            {
                uint64_t tag = 0;
                if (!ReadVarint(data, position, tag))
                    return false;
                int field = static_cast<int>(tag >> 3);
                int wire = static_cast<int>(tag & 0x07);

                if (field == 1 && wire == 2)
                {
                    std::vector<unsigned char> value;
                    if (!ReadLengthDelimited(data, position, value))
                        return false;
                    FlydigiPipeProtocol::DeviceInfo info;
                    if (!ParseDeviceInfo(value, info))
                        return false;
                    devices.push_back(std::move(info));
                }
                else if (!SkipField(data, position, wire))
                {
                    return false;
                }
            }
            return true;
        }

        bool ReadExact(HANDLE pipe, void* buffer, DWORD size, DWORD timeoutMs)
        {
            DWORD total = 0;
            while (total < size)
            {
                OVERLAPPED overlapped = {};
                overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (overlapped.hEvent == nullptr)
                    return false;

                DWORD read = 0;
                BOOL ok = ReadFile(pipe, static_cast<unsigned char*>(buffer) + total,
                    size - total, nullptr, &overlapped);
                if (!ok)
                {
                    DWORD error = GetLastError();
                    if (error != ERROR_IO_PENDING)
                    {
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    if (WaitForSingleObject(overlapped.hEvent, timeoutMs) != WAIT_OBJECT_0)
                    {
                        CancelIo(pipe);
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    if (!GetOverlappedResult(pipe, &overlapped, &read, FALSE))
                    {
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                }

                CloseHandle(overlapped.hEvent);
                if (read == 0)
                    return false;
                total += read;
            }
            return true;
        }

        bool WriteExact(HANDLE pipe, const void* buffer, DWORD size, DWORD timeoutMs)
        {
            DWORD total = 0;
            while (total < size)
            {
                OVERLAPPED overlapped = {};
                overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (overlapped.hEvent == nullptr)
                    return false;

                DWORD written = 0;
                BOOL ok = WriteFile(pipe,
                    static_cast<const unsigned char*>(buffer) + total,
                    size - total, nullptr, &overlapped);
                if (!ok)
                {
                    DWORD error = GetLastError();
                    if (error != ERROR_IO_PENDING)
                    {
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    if (WaitForSingleObject(overlapped.hEvent, timeoutMs) != WAIT_OBJECT_0)
                    {
                        CancelIo(pipe);
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    if (!GetOverlappedResult(pipe, &overlapped, &written, FALSE))
                    {
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                }

                CloseHandle(overlapped.hEvent);
                if (written == 0)
                    return false;
                total += written;
            }
            return true;
        }

        bool ReadHeaderLine(HANDLE pipe, std::string& header, DWORD timeoutMs)
        {
            header.clear();
            ULONGLONG deadline = GetTickCount64() + timeoutMs;
            while (header.size() < 64)
            {
                ULONGLONG now = GetTickCount64();
                if (now >= deadline)
                    return false;
                char ch = 0;
                if (!ReadExact(pipe, &ch, 1, static_cast<DWORD>(deadline - now)))
                    return false;
                if (ch == '\n')
                    return true;
                if (ch != '\r')
                    header.push_back(ch);
            }
            return false;
        }

        bool ReadInt32(HANDLE pipe, int32_t& value)
        {
            unsigned char bytes[4] = {};
            if (!ReadExact(pipe, bytes, 4, 1000))
                return false;
            value = static_cast<int32_t>(
                static_cast<uint32_t>(bytes[0]) |
                (static_cast<uint32_t>(bytes[1]) << 8) |
                (static_cast<uint32_t>(bytes[2]) << 16) |
                (static_cast<uint32_t>(bytes[3]) << 24));
            return true;
        }

        bool ReadFdgPayload(HANDLE pipe, std::vector<unsigned char>& payload,
            std::wstring& error)
        {
            std::string header;
            if (!ReadHeaderLine(pipe, header, 1200))
            {
                error = L"读取飞智空间站协议头超时";
                return false;
            }

            if (header == "FDG_PROTOCOL")
            {
                int32_t length = 0;
                if (!ReadInt32(pipe, length) || length < 0 || length > 32 * 1024 * 1024)
                {
                    error = L"飞智空间站响应长度非法";
                    return false;
                }
                payload.resize(static_cast<size_t>(length));
                if (!payload.empty() && !ReadExact(pipe, payload.data(),
                    static_cast<DWORD>(payload.size()), 1500))
                {
                    error = L"读取飞智空间站响应超时";
                    return false;
                }
                return true;
            }

            if (header == "FDG_CHUNK")
            {
                int32_t total = 0;
                int32_t count = 0;
                if (!ReadInt32(pipe, total) || !ReadInt32(pipe, count) ||
                    total < 0 || total > 32 * 1024 * 1024 || count < 1 || count > 65535)
                {
                    error = L"飞智空间站分块响应头非法";
                    return false;
                }

                payload.clear();
                payload.reserve(static_cast<size_t>(total));
                for (int32_t i = 0; i < count; ++i)
                {
                    int32_t length = 0;
                    if (!ReadInt32(pipe, length) || length < 0)
                    {
                        error = L"飞智空间站分块长度非法";
                        return false;
                    }
                    std::vector<unsigned char> chunk(static_cast<size_t>(length));
                    if (!chunk.empty() && !ReadExact(pipe, chunk.data(),
                        static_cast<DWORD>(chunk.size()), 1500))
                    {
                        error = L"读取飞智空间站分块超时";
                        return false;
                    }
                    payload.insert(payload.end(), chunk.begin(), chunk.end());
                }
                return true;
            }

            if (header == "FDG_COMPRESSED")
            {
                error = L"飞智空间站返回了压缩响应（暂不支持）";
                return false;
            }

            error = L"未知的飞智空间站协议头";
            return false;
        }
    }

    bool FlydigiPipeProtocol::ParseConnectedDevices(
        const std::vector<unsigned char>& payload,
        std::vector<DeviceInfo>& devices, int* errorCode)
    {
        devices.clear();
        if (errorCode != nullptr)
            *errorCode = 0;

        size_t position = 0;
        std::vector<unsigned char> deviceList;
        bool hasDeviceList = false;

        while (position < payload.size())
        {
            uint64_t tag = 0;
            if (!ReadVarint(payload, position, tag))
                return false;
            int field = static_cast<int>(tag >> 3);
            int wire = static_cast<int>(tag & 0x07);

            if (field == 3 && wire == 0)
            {
                uint64_t code = 0;
                if (!ReadVarint(payload, position, code))
                    return false;
                if (errorCode != nullptr)
                    *errorCode = static_cast<int>(code);
            }
            else if (field == 5 && wire == 2)
            {
                if (!ReadLengthDelimited(payload, position, deviceList))
                    return false;
                hasDeviceList = true;
            }
            else if (!SkipField(payload, position, wire))
            {
                return false;
            }
        }

        if (errorCode != nullptr && *errorCode != 0)
            return false;
        if (!hasDeviceList)
            return false;
        return ParseDeviceList(deviceList, devices);
    }

    BatteryReading FlydigiPipeProtocol::Read(const DeviceDescriptor&) const
    {
        BatteryReading reading;
        reading.protocol = Id();

        HANDLE pipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0,
            nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                reading.note = L"飞智空间站服务未运行";
            else if (error == ERROR_PIPE_BUSY)
                reading.note = L"飞智空间站 App 占用了管道";
            else if (error == ERROR_ACCESS_DENIED)
                reading.note = L"飞智空间站管道拒绝访问";
            else
                reading.note = L"无法连接飞智空间站管道 (" +
                    std::to_wstring(error) + L")";
            return reading;
        }

        reading.found = true;

        bool ok = WriteExact(pipe, kRequest, sizeof(kRequest), 1000);
        std::vector<unsigned char> payload;
        std::wstring errorText;
        if (ok)
            ok = ReadFdgPayload(pipe, payload, errorText);
        CloseHandle(pipe);

        if (!ok)
        {
            reading.note = errorText.empty() ? L"读取飞智空间站响应失败" : errorText;
            return reading;
        }

        int errorCode = 0;
        std::vector<DeviceInfo> devices;
        if (!ParseConnectedDevices(payload, devices, &errorCode))
        {
            reading.note = errorCode != 0
                ? L"飞智空间站返回错误码 " + std::to_wstring(errorCode)
                : L"无法解析飞智空间站响应";
            return reading;
        }

        const DeviceInfo* selected = nullptr;
        for (const DeviceInfo& device : devices)
        {
            if (device.category != 1 || !device.connected ||
                device.batteryLevel < 0 || device.batteryLevel > 6)
                continue;

            if (selected == nullptr)
                selected = &device;
            if (device.productName.find(L"Direwolf") != std::wstring::npos)
            {
                selected = &device;
                break;
            }
        }

        if (selected == nullptr)
        {
            reading.note = L"未找到已连接的飞智手柄";
            return reading;
        }

        reading.online = true;
        reading.percent = (selected->batteryLevel * 100 + 3) / 6;
        if (reading.percent > 100)
            reading.percent = 100;

        if (selected->connectType == 1)
            reading.charge = ChargeState::Charging;
        else
            reading.charge = ChargeState::Discharging;
        if (selected->batteryLevel >= 6 && selected->connectType == 1)
            reading.charge = ChargeState::Full;

        reading.note = selected->productName + L" " +
            std::to_wstring(selected->batteryLevel) + L"/6";
        return reading;
    }
}
