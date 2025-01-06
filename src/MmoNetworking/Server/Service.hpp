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
        virtual void HandleReceivedMessage(OwnedMessage receivedMessage) override
        {
            Client::MessageId messageId = 
                static_cast<Client::MessageId>(receivedMessage.message.header.id);

            switch (messageId)
            {
            case Client::MessageId::Echo:
                HandleEcho(std::move(receivedMessage.pOwner));
                break;
            default:
                break;
            }
        }

        virtual void OnTickRateMeasured(const TickRate tickRate) override
        {
            std::cout << "[SERVER] Tick rate: " << tickRate << "hz\n";
        }

    private:
        void HandleEcho(SessionPointer pSession)
        {
            Message message;
            message.header.id = static_cast<NetCommon::Message::Id>(MessageId::Echo);

            SendMessageAsync(std::move(pSession), std::move(message));
        }

    };
}
