#pragma once

#include "IProtocol.h"

#include <memory>
#include <string>
#include <vector>

namespace pb
{
    class ProtocolRegistry
    {
    public:
        static ProtocolRegistry& Instance();

        void Register(std::unique_ptr<IProtocol> protocol);
        IProtocol* Find(const std::wstring& id) const;

    private:
        ProtocolRegistry();

        std::vector<std::unique_ptr<IProtocol>> m_protocols;
    };
}
