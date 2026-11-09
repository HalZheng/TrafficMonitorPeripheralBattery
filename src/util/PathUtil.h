#pragma once

#include <windows.h>

#include <string>

namespace pb
{
    std::wstring GetModuleDirectory(HMODULE module = nullptr);
    std::wstring JoinPath(const std::wstring& directory, const std::wstring& file);
    bool FileExists(const std::wstring& path);
}
