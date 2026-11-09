#include "PluginInterface.h"

#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <string>

typedef ITMPlugin* (*TMPluginGetInstanceFunc)();

static void PrintItems(ITMPlugin* plugin, const wchar_t* tag)
{
    wprintf(L"--- %s ---\n", tag);
    for (int i = 0; i < 8; ++i)
    {
        IPluginItem* item = plugin->GetItem(i);
        if (item == nullptr)
            break;
        wprintf(L"item %d: %s = %s\n", i, item->GetItemName(), item->GetItemValueText());
    }
}

int wmain(int argc, wchar_t** argv)
{
    std::wstring dllPath;
    if (argc > 1 && wcscmp(argv[1], L"--options") != 0)
    {
        dllPath = argv[1];
    }
    else
    {
        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        std::wstring path = modulePath;
        size_t slash = path.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            dllPath = path.substr(0, slash + 1);
        dllPath += L"PeripheralBatteryPlugin.dll";
    }

    HMODULE module = LoadLibraryW(dllPath.c_str());
    if (module == nullptr)
    {
        wprintf(L"LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }

    TMPluginGetInstanceFunc getInstance =
        reinterpret_cast<TMPluginGetInstanceFunc>(GetProcAddress(module, "TMPluginGetInstance"));
    if (getInstance == nullptr)
    {
        wprintf(L"GetProcAddress(TMPluginGetInstance) failed\n");
        FreeLibrary(module);
        return 1;
    }

    ITMPlugin* plugin = getInstance();
    if (plugin == nullptr)
    {
        wprintf(L"TMPluginGetInstance returned null\n");
        FreeLibrary(module);
        return 1;
    }

    wprintf(L"plugin: %s %s\n",
        plugin->GetInfo(ITMPlugin::TMI_NAME),
        plugin->GetInfo(ITMPlugin::TMI_VERSION));

    if (argc > 1 && wcscmp(argv[1], L"--options") == 0)
    {
        plugin->OnInitialize(nullptr);
        plugin->ShowOptionsDialog(nullptr);
        FreeLibrary(module);
        return 0;
    }

    plugin->OnInitialize(nullptr);
    PrintItems(plugin, L"initial");

    for (int i = 0; i < 20; ++i)
    {
        plugin->DataRequired();
        Sleep(500);
    }

    PrintItems(plugin, L"after 10s");
    FreeLibrary(module);
    return 0;
}
