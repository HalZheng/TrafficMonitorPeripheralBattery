#pragma once

#include "Settings.h"
#include "../core/DeviceRegistry.h"

#include <windows.h>

namespace pb
{
    class OptionsDialog
    {
    public:
        static bool Show(HWND parent, Settings& settings, const DeviceRegistry& registry);
    };
}
