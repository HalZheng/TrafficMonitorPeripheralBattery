#include "plugin/Plugin.h"

#include <windows.h>

namespace
{
    pb::Plugin g_plugin;
}

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

ITMPlugin* TMPluginGetInstance()
{
    return &g_plugin;
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
        g_plugin.Stop();
    return TRUE;
}
