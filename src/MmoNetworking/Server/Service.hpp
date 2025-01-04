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

    public:
        Service(size_t nWorkers, 
                TickRate maxTickRate, 
                uint16_t port)
            : ServerServiceBase(nWorkers, maxTickRate, port)
        {}

    protected:
        virtual bool OnSessionCreated(SessionPointer pSession) override
        {
            return true;
        }

        virtual void OnSessionRegistered(SessionPointer pSession) override
        {}

        virtual void OnSessionUnregistered(SessionPointer pSession) override
        {}

        virtual void HandleReceivedMessage(SessionPointer pSession, Message& message) override
        {
            Client::MessageId messageId = static_cast<Client::MessageId>(message.header.id);

            switch (messageId)
            {
            case Client::MessageId::Ping:
                Ping(pSession);
                break;
            default:
                break;
            }
        }

        virtual bool OnReceivedMessagesDispatched() override
        {
            return true;
        }

        virtual void OnTickRateMeasured(const TickRate measuredTickRate) override
        {
            std::cout << "[SERVER] Tick rate: " << measuredTickRate << "hz\n";
        }

    private:
        void Ping(SessionPointer pSession)
        {
            Message message;
            message.header.id = static_cast<NetCommon::MessageId>(MessageId::Ping);

            SendMessageAsync(pSession, message);
        }

    };
}
