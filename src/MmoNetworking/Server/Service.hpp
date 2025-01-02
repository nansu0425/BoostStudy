#pragma once

#include <NetCommon/ServerServiceBase.hpp>

namespace Server
{
    class Service : public NetCommon::ServerServiceBase
    {
    protected:
        using Message       = NetCommon::Message;

    public:
        Service(uint16_t port)
            : ServerServiceBase(port)
        {}

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
    };
}
