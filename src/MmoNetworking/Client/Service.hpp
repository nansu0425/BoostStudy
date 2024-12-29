#pragma once

#include <NetCommon/ClientServiceBase.hpp>
#include <Client/MessageId.hpp>

namespace Client
{
    class Service : public NetCommon::ClientServiceBase
    {
    protected:
        using Message       = NetCommon::Message;

    protected:
        virtual void OnSessionConnected() override
        {}

        virtual void OnSessionDisconnected(SessionPointer pSession) override
        {}

        virtual void OnMessageReceived(SessionPointer pSession, Message& message) override
        {}

        virtual bool OnUpdateStarted() override
        {
            return true;
        }

    public:
        void PingServer()
        {
            Message message;
            message.header.id = static_cast<NetCommon::MessageId>(MessageId::Ping);

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            message << now;

            SendAsync(message);
        }
    };
}
