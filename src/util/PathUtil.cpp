#include "PathUtil.h"

#include <vector>

namespace pb
{
    std::wstring GetModuleDirectory(HMODULE module)
    {
        std::vector<wchar_t> buffer(1024);
        DWORD length = GetModuleFileNameW(module, buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return L"";

        std::wstring path(buffer.data(), length);
        size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            return L"";
        return path.substr(0, slash);
    }

    std::wstring JoinPath(const std::wstring& directory, const std::wstring& file)
    {
        if (directory.empty())
            return file;
        wchar_t last = directory.back();
        if (last == L'\\' || last == L'/')
            return directory + file;
        return directory + L"\\" + file;
    }

    bool FileExists(const std::wstring& path)
    {
        DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
}
