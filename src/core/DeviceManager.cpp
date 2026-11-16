#include "DeviceManager.h"

#include "../core/HidTransport.h"
#include "../protocols/ProtocolRegistry.h"

#include <windows.h>

#include <chrono>

namespace pb
{
    namespace
    {
        /** Reuse the last good measurement when a device stops answering. */
        void UseLastKnown(DeviceState& state, bool keep)
        {
            if (keep && state.lastGood.HasPercent())
            {
                state.reading = state.lastGood;
                state.reading.stale = true;
            }
            else
            {
                state.reading = BatteryReading{};
            }
        }
    }

    DeviceManager::~DeviceManager()
    {
        Stop();
    }

    bool DeviceManager::WithinGrace(unsigned long long lastSeenMs,
        unsigned long long nowMs, int graceSeconds)
    {
        if (lastSeenMs == 0 || graceSeconds <= 0 || nowMs < lastSeenMs)
            return false;
        unsigned long long window =
            static_cast<unsigned long long>(graceSeconds) * 1000ULL;
        return nowMs - lastSeenMs <= window;
    }

    void DeviceManager::Configure(const std::vector<DeviceDescriptor>& devices, int refreshSeconds)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_states.clear();
            m_states.reserve(devices.size());
            for (const DeviceDescriptor& device : devices)
            {
                DeviceState state;
                state.device = device;
                m_states.push_back(std::move(state));
            }
            m_nextAttempt.clear();
            m_failedPolls.clear();
        }
        SetRefreshSeconds(refreshSeconds);
    }

    void DeviceManager::Start()
    {
        if (m_started.exchange(true))
            return;
        m_stop = false;
        m_thread = std::thread(&DeviceManager::WorkerLoop, this);
    }

    void DeviceManager::Stop()
    {
        if (!m_started.exchange(false))
            return;

        m_stop = true;
        m_cv.notify_all();
        if (m_thread.joinable())
            m_thread.join();
    }

    void DeviceManager::RequestRefresh()
    {
        m_refresh = true;
        m_resetBackoff = true;
        m_cv.notify_all();
    }

    void DeviceManager::SetRefreshSeconds(int seconds)
    {
        if (seconds < 5)
            seconds = 5;
        if (seconds > 3600)
            seconds = 3600;
        m_refreshSeconds = seconds;
        m_cv.notify_all();
    }

    void DeviceManager::SetGraceSeconds(int seconds)
    {
        if (seconds < 0)
            seconds = 0;
        if (seconds > 3600)
            seconds = 3600;
        m_graceSeconds = seconds;
    }

    void DeviceManager::SetKeepLastKnown(bool keep)
    {
        m_keepLastKnown = keep;
    }

    void DeviceManager::SetHiddenIds(const std::vector<std::wstring>& ids)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_hidden.clear();
        m_hidden.insert(ids.begin(), ids.end());
    }

    void DeviceManager::Snapshot(std::vector<DeviceState>& out) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        out = m_states;
        for (DeviceState& state : out)
        {
            auto iter = m_failedPolls.find(state.device.id);
            state.failedPolls = iter != m_failedPolls.end() ? iter->second : 0;
        }
    }

    bool DeviceManager::ShouldSkip(const std::wstring& id,
        unsigned long long now) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto iter = m_nextAttempt.find(id);
        return iter != m_nextAttempt.end() && iter->second > now;
    }

    void DeviceManager::RecordResult(const std::wstring& id, bool ok,
        unsigned long long now, bool quickRetry)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (ok)
        {
            m_nextAttempt.erase(id);
            m_failedPolls.erase(id);
            return;
        }

        int& failures = m_failedPolls[id];
        if (failures < 8)
            ++failures;

        unsigned long long delay;
        if (quickRetry)
        {
            // This device has worked before, so a failed read most likely means
            // it is asleep. Retry every cycle rather than backing off for up to
            // five minutes, so waking it clears the stale marker promptly.
            delay = static_cast<unsigned long long>(m_refreshSeconds.load());
        }
        else
        {
            // 30s -> 60s -> 120s -> 240s -> 300s for the default interval.
            int exponent = failures < 4 ? failures : 4;
            delay = static_cast<unsigned long long>(
                m_refreshSeconds.load()) << exponent;
            if (delay > 300)
                delay = 300;
        }
        m_nextAttempt[id] = now + delay * 1000;
    }

    void DeviceManager::WorkerLoop()
    {
        while (!m_stop)
        {
            if (m_resetBackoff.exchange(false))
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_nextAttempt.clear();
                m_failedPolls.clear();
            }

            // One SetupAPI enumeration per cycle on the worker thread. Presence
            // is cheap to evaluate afterwards and keeps sleeping devices from
            // being polled again and again.
            std::vector<HidInterfaceInfo> interfaces = EnumerateHidInterfaces();

            std::vector<DeviceState> local;
            std::set<std::wstring> hidden;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                local = m_states;
                hidden = m_hidden;
            }

            const unsigned long long now = GetTickCount64();
            const int graceSeconds = m_graceSeconds.load();
            const bool keepLastKnown = m_keepLastKnown.load();

            for (DeviceState& state : local)
            {
                IProtocol* protocol =
                    ProtocolRegistry::Instance().Find(state.device.protocolId);
                const bool hardware = protocol != nullptr &&
                    protocol->IsPresent(state.device, interfaces);
                const bool hiddenDevice = hidden.count(state.device.id) != 0;

                // A receiver that just came back must be polled immediately.
                // Otherwise a backoff left over from before the sleep could
                // delay the live reading by up to five minutes.
                if (hardware && !state.hardwarePresent)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_nextAttempt.erase(state.device.id);
                    m_failedPolls.erase(state.device.id);
                }

                state.hardwarePresent = hardware;
                if (hardware)
                    state.lastSeenMs = now;

                // A device that once reported a valid level stays visible with
                // that level until it answers again, however long it sleeps.
                // Devices that never answered still use the short grace window
                // and show "--" while it lasts.
                state.present = hardware ||
                    WithinGrace(state.lastSeenMs, now, graceSeconds) ||
                    (keepLastKnown && !hiddenDevice &&
                        state.lastGood.HasPercent());

                if (hiddenDevice)
                {
                    state.reading = BatteryReading{};
                    continue;
                }

                if (!hardware)
                {
                    UseLastKnown(state, keepLastKnown);
                    continue;
                }
                if (ShouldSkip(state.device.id, now))
                    continue;

                state.reading = protocol->Read(state.device);
                const bool ok = state.reading.HasPercent();
                if (ok)
                {
                    state.lastGood = state.reading;
                    state.lastGood.stale = false;
                    state.lastGoodMs = GetTickCount64();
                }
                else
                {
                    // The receiver is still attached but the device is asleep
                    // or not answering: keep the level, flag it as stale.
                    UseLastKnown(state, keepLastKnown);
                }
                RecordResult(state.device.id, ok, GetTickCount64(),
                    state.lastGood.HasPercent());
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const DeviceState& updated : local)
                {
                    for (DeviceState& target : m_states)
                    {
                        if (target.device.id != updated.device.id)
                            continue;
                        target.reading = updated.reading;
                        target.lastGood = updated.lastGood;
                        target.lastGoodMs = updated.lastGoodMs;
                        target.hardwarePresent = updated.hardwarePresent;
                        target.present = updated.present;
                        target.lastSeenMs = updated.lastSeenMs;
                        break;
                    }
                }
            }

            std::unique_lock<std::mutex> lock(m_mutex);
            int seconds = m_refreshSeconds.load();
            m_cv.wait_for(lock, std::chrono::seconds(seconds), [this]()
            {
                return m_stop.load() || m_refresh.load();
            });
            m_refresh = false;
        }
    }
}
