#include "DeviceManager.h"

#include "../protocols/ProtocolRegistry.h"

#include <windows.h>

#include <chrono>

namespace pb
{
    DeviceManager::~DeviceManager()
    {
        Stop();
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

    void DeviceManager::Snapshot(std::vector<DeviceState>& out) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        out = m_states;
    }

    bool DeviceManager::ShouldSkip(const std::wstring& id,
        unsigned long long now) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto iter = m_nextAttempt.find(id);
        return iter != m_nextAttempt.end() && iter->second > now;
    }

    void DeviceManager::RecordResult(const std::wstring& id, bool ok,
        unsigned long long now)
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

        // 30s -> 60s -> 120s -> 240s -> 300s for the default interval.
        int exponent = failures < 4 ? failures : 4;
        unsigned long long delay = static_cast<unsigned long long>(
            m_refreshSeconds.load()) << exponent;
        if (delay > 300)
            delay = 300;
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

            std::vector<DeviceState> local;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                local = m_states;
            }

            for (DeviceState& state : local)
            {
                if (ShouldSkip(state.device.id, GetTickCount64()))
                    continue;

                IProtocol* protocol = ProtocolRegistry::Instance().Find(state.device.protocolId);
                if (protocol != nullptr)
                {
                    state.reading = protocol->Read(state.device);
                }
                else
                {
                    state.reading = BatteryReading{};
                    state.reading.note = L"protocol not registered";
                }

                RecordResult(state.device.id, state.reading.HasPercent(),
                    GetTickCount64());
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (size_t i = 0; i < local.size() && i < m_states.size(); ++i)
                {
                    if (m_states[i].device.id == local[i].device.id)
                        m_states[i].reading = local[i].reading;
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
