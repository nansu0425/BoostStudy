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
        virtual bool OnSessionConnected(SessionPointer pSession) override
        {
            return true;
        }

        virtual void OnSessionDisconnected(SessionPointer pSession) override
        {}

        virtual void OnMessageReceived(SessionPointer pSession, Message& message) override
        {}

        virtual bool OnUpdateStarted() override
        {
            return true;
        }

    public:
        void Ping(SessionPointer pSession)
        {
            Message message;
            message.header.id = static_cast<NetCommon::MessageId>(MessageId::Ping);

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            message << now;

            SendAsync(pSession, message);
        }
    };
}
