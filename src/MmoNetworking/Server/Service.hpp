#pragma once

#include <NetCommon/ServerServiceBase.hpp>
#include <Server/MessageId.hpp>

namespace Server
{
    class Service : public NetCommon::ServerServiceBase<MessageId>
    {
    public:
        Service(uint16_t port)
            : NetCommon::ServerServiceBase<MessageId>(port)
        {}

    protected:
        virtual bool OnClientConnected(ClientPointer pClient) override
        {
            return true;
        }

        virtual void OnClientDisconnected(ClientPointer pClient) override
        {}

        virtual void OnMessageReceived(ClientPointer pClient, Message& message) override
        {}
    };
}
