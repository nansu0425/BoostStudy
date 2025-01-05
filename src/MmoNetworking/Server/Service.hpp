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
                size_t nMaxReceivedMessages,
                uint16_t port)
            : ServerServiceBase(nWorkers, nMaxReceivedMessages, port)
        {}

    protected:
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

        virtual void OnTickRateMeasured(const TickRate measured) override
        {
            std::cout << "[SERVER] Tick rate: " << measured << "hz\n";
        }

    private:
        void Ping(SessionPointer pSession)
        {
            Message message;
            message.header.id = static_cast<NetCommon::Message::Id>(MessageId::Ping);

            SendMessageAsync(pSession, message);
        }

    };
}
