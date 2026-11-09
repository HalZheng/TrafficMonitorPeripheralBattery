#include "ProtocolRegistry.h"

#include "AulaBycomboProtocol.h"
#include "FlydigiPipeProtocol.h"
#include "JuxingProtocol.h"

namespace pb
{
    ProtocolRegistry& ProtocolRegistry::Instance()
    {
        static ProtocolRegistry instance;
        return instance;
    }

    ProtocolRegistry::ProtocolRegistry()
    {
        Register(std::unique_ptr<IProtocol>(new JuxingProtocol()));
        Register(std::unique_ptr<IProtocol>(new AulaBycomboProtocol()));
        Register(std::unique_ptr<IProtocol>(new FlydigiPipeProtocol()));
    }

    void ProtocolRegistry::Register(std::unique_ptr<IProtocol> protocol)
    {
        if (protocol != nullptr)
            m_protocols.push_back(std::move(protocol));
    }

    IProtocol* ProtocolRegistry::Find(const std::wstring& id) const
    {
        for (const std::unique_ptr<IProtocol>& protocol : m_protocols)
        {
            if (protocol != nullptr && id == protocol->Id())
                return protocol.get();
        }
        return nullptr;
    }
}
