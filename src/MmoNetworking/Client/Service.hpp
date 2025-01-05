﻿#pragma once

#include <NetCommon/ClientServiceBase.hpp>
#include <Client/MessageId.hpp>
#include <Server/MessageId.hpp>

namespace Client
{
    class Service : public NetCommon::ClientServiceBase
    {
    private:
        using Message       = NetCommon::Message;
        using TimePoint     = std::chrono::steady_clock::time_point;

    public:
        Service(size_t nWorkers, 
                size_t nMaxReceivedMessages)
            : ClientServiceBase(nWorkers, nMaxReceivedMessages)
            , _pingTimer(_workers)
        {}

    protected:
        virtual void OnSessionRegistered(SessionPointer pSession) override
        {
            PingAsync(pSession);
        }

        virtual void HandleReceivedMessage(SessionPointer pSession, Message& message) override
        {
            Server::MessageId messageId = static_cast<Server::MessageId>(message.header.id);

            switch (messageId)
            {
            case Server::MessageId::Ping:
                OnPingCompleted(pSession, message);
                break;
            default:
                break;
            }
        }

    private:
        void PingAsync(SessionPointer pSession)
        {
            _start = std::chrono::steady_clock::now();

            Message message;
            message.header.id = static_cast<NetCommon::Message::Id>(MessageId::Ping);

            SendMessageAsync(pSession, std::move(message));
        }

        void OnPingCompleted(SessionPointer pSession, Message& message)
        {
            TimePoint end = std::chrono::steady_clock::now();

            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - _start);;
            std::cout << pSession << " Ping: " << elapsed.count() << "us \n";

            _pingTimer.expires_after(std::chrono::seconds(1));
            _pingTimer.async_wait([this, pSession](const ErrorCode& error)
                                  {
                                      PingAsync(pSession);
                                  });
        }

    private:
        TimePoint       _start;
        Timer           _pingTimer;
    
    };
}
