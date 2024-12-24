﻿#pragma once

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
    virtual bool OnConnect(ConnectionPointer pClient) override
    {
        return true;
    }

    virtual void OnDisconnect(ConnectionPointer pClient) override
    {}
    virtual void OnReceive(ConnectionPointer pClient, Message& message) override
    {}
};
