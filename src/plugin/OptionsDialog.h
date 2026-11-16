#pragma once

#include "Settings.h"
#include "../core/DeviceRegistry.h"

#include <windows.h>

#include <functional>
#include <vector>

namespace pb
{
    class OptionsDialog
    {
    public:
        /**
         * presentProvider returns the currently discovered device ids (UI
         * thread, cheap). onRescan asks the polling thread for a fresh scan.
         */
        static bool Show(HWND parent, Settings& settings,
            const DeviceRegistry& registry,
            const std::function<std::vector<std::wstring>()>& presentProvider,
            const std::function<void()>& onRescan);
    };
}
