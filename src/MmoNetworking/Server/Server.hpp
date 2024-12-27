#pragma once

#include <NetCommon/ServerBase.hpp>

enum class ServerMessageId : NetCommon::MessageId
{
    Accept,
    Deny,
    Ping,
    Send,
    Broadcast,
};

class Server : public NetCommon::ServerBase<ServerMessageId>
{
public:
    Server(uint16_t port)
        : NetCommon::ServerBase<ServerMessageId>(port)
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
