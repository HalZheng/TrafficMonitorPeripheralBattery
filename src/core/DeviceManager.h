#pragma once

#include "BatteryReading.h"
#include "DeviceDescriptor.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace pb
{
    struct DeviceState
    {
        DeviceDescriptor device;
        BatteryReading reading;
    };

    /**
     * Owns the polling worker. The TrafficMonitor UI thread only reads a
     * snapshot; all HID I/O happens here and can never block the host UI.
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
        void Snapshot(std::vector<DeviceState>& out) const;

    private:
        void WorkerLoop();
        bool ShouldSkip(const std::wstring& id, unsigned long long now) const;
        void RecordResult(const std::wstring& id, bool ok,
            unsigned long long now);

        mutable std::mutex m_mutex;
        std::vector<DeviceState> m_states;
        std::map<std::wstring, unsigned long long> m_nextAttempt;
        std::map<std::wstring, int> m_failedPolls;
        std::thread m_thread;
        std::condition_variable m_cv;
        std::atomic<bool> m_stop{ false };
        std::atomic<bool> m_started{ false };
        std::atomic<bool> m_refresh{ false };
        std::atomic<bool> m_resetBackoff{ false };
        std::atomic<int> m_refreshSeconds{ 30 };
    };
}
