#pragma once

#include "BatteryReading.h"
#include "DeviceDescriptor.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace pb
{
    struct DeviceState
    {
        DeviceDescriptor device;
        BatteryReading reading;
        // Last successful measurement. Kept in memory for the whole process so
        // a sleeping device keeps showing its level; never written to disk.
        BatteryReading lastGood;
        unsigned long long lastGoodMs = 0;
        bool hardwarePresent = false;   // receiver/service detected this cycle
        bool present = false;           // visible: hardware, grace or lastGood
        int failedPolls = 0;            // consecutive failed reads (diagnostics)
        unsigned long long lastSeenMs = 0;
    };

    /**
     * Owns the polling worker. The TrafficMonitor UI thread only reads a
     * snapshot; all HID I/O and device discovery happens here and can never
     * block the host UI.
     *
     * The device list is the full compiled-in registry, not a filtered subset.
     * Presence is recomputed every cycle, so hot-plug and removal show up in
     * the taskbar without rebuilding any IPluginItem objects.
     */
    class DeviceManager
    {
    public:
        DeviceManager() = default;
        ~DeviceManager();

        void Configure(const std::vector<DeviceDescriptor>& devices, int refreshSeconds);
        void Start();
        void Stop();
        void RequestRefresh();
        void SetRefreshSeconds(int seconds);
        void SetGraceSeconds(int seconds);
        void SetKeepLastKnown(bool keep);
        void SetHiddenIds(const std::vector<std::wstring>& ids);
        void Snapshot(std::vector<DeviceState>& out) const;

        /** A device stays visible for graceSeconds after its receiver vanishes. */
        static bool WithinGrace(unsigned long long lastSeenMs,
            unsigned long long nowMs, int graceSeconds);

    private:
        void WorkerLoop();
        bool ShouldSkip(const std::wstring& id, unsigned long long now) const;
        void RecordResult(const std::wstring& id, bool ok,
            unsigned long long now, bool quickRetry);

        mutable std::mutex m_mutex;
        std::vector<DeviceState> m_states;
        std::set<std::wstring> m_hidden;
        std::map<std::wstring, unsigned long long> m_nextAttempt;
        std::map<std::wstring, int> m_failedPolls;
        std::thread m_thread;
        std::condition_variable m_cv;
        std::atomic<bool> m_stop{ false };
        std::atomic<bool> m_started{ false };
        std::atomic<bool> m_refresh{ false };
        std::atomic<bool> m_resetBackoff{ false };
        std::atomic<int> m_refreshSeconds{ 30 };
        std::atomic<int> m_graceSeconds{ 300 };
        std::atomic<bool> m_keepLastKnown{ true };
    };
}
