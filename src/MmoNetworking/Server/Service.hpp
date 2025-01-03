#pragma once

#include <NetCommon/ServerServiceBase.hpp>
#include <Server/MessageId.hpp>
#include <Client/MessageId.hpp>

namespace Server
{
    class Service : public NetCommon::ServerServiceBase
    {
    private:
        using Message       = NetCommon::Message;
        using TimePoint     = std::chrono::system_clock::time_point;

    public:
        Service(uint16_t port)
            : ServerServiceBase(port)
        {}

    protected:
        virtual bool OnSessionCreated(SessionPointer pSession) override
        {
            return true;
        }

        virtual void OnSessionRegistered(SessionPointer pSession) override
        {}

        virtual void OnSessionDisconnected(SessionPointer pSession) override
        {
            std::cout << "[" << pSession->GetId() << "] Session disconnected\n";
        }

        virtual void OnMessageReceived(SessionPointer pSession, Message& message) override
        {
            Client::MessageId messageId = static_cast<Client::MessageId>(message.header.id);

            switch (messageId)
            {
            case Client::MessageId::Ping:
                HandlePing(pSession);
                break;
            default:
                break;
            }
        }

        virtual bool OnUpdateStarted() override
        {
            return true;
        }

    private:
        void HandlePing(SessionPointer pSession)
        {
            Message message;
            message.header.id = static_cast<NetCommon::MessageId>(MessageId::Ping);

            TimePoint now = std::chrono::system_clock::now();
            message << now;

            SendMessageAsync(pSession, message);
        }

    };
}
