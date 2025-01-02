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
            std::cout << "[" << pSession->GetId() << "] Session connected\n";
            return true;
        }

        virtual void OnSessionRegistered(SessionPointer pSession) override
        {
            std::cout << "[" << pSession->GetId() << "] Session registered\n";
        }

        virtual void OnSessionDisconnected(SessionPointer pSession) override
        {
            std::cout << "[" << pSession->GetId() << "] Session disconnected\n";
        }

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

            SendMessageAsync(pSession, message);
        }
    };
}
