#pragma once

#include <NetCommon/ServerBase.hpp>
#include <Server/MessageId.hpp>

namespace Server
{
    class Service : public NetCommon::ServerBase<MessageId>
    {
    public:
        Service(uint16_t port)
            : NetCommon::ServerBase<MessageId>(port)
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
