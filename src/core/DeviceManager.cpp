#include "DeviceManager.h"

#include "../protocols/ProtocolRegistry.h"

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

    void DeviceManager::WorkerLoop()
    {
        while (!m_stop)
        {
            std::vector<DeviceState> local;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                local = m_states;
            }

            for (DeviceState& state : local)
            {
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
