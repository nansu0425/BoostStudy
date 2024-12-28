#pragma once

#include <Server/ServerServiceBase.hpp>

namespace Server
{
    class Service : public ServerServiceBase
    {
    public:
        Service(uint16_t port)
            : ServerServiceBase(port)
        {}

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
    };
}
