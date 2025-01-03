#pragma once

#include <NetCommon/ClientServiceBase.hpp>
#include <Client/MessageId.hpp>
#include <Server/MessageId.hpp>

namespace Client
{
    class Service : public NetCommon::ClientServiceBase
    {
    private:
        using Message       = NetCommon::Message;
        using TimePoint     = std::chrono::system_clock::time_point;

    protected:
        virtual bool OnSessionCreated(SessionPointer pSession) override
        {
            return true;
        }

        virtual void OnSessionRegistered(SessionPointer pSession) override
        {
            Ping(pSession);
        }

        virtual void OnSessionDisconnected(SessionPointer pSession) override
        {
            std::cout << "[" << pSession->GetId() << "] Session disconnected\n";
        }

        virtual void OnMessageReceived(SessionPointer pSession, Message& message) override
        {
            Server::MessageId messageId = static_cast<Server::MessageId>(message.header.id);

            switch (messageId)
            {
            case Server::MessageId::Ping:
                HandlePing(pSession, message);
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
        void Ping(SessionPointer pSession)
        {
            Message message;
            message.header.id = static_cast<NetCommon::MessageId>(MessageId::Ping);

            _start = std::chrono::system_clock::now();
            message << _start;

            SendMessageAsync(pSession, message);
        }

        void HandlePing(SessionPointer pSession, Message& message)
        {
            TimePoint end;
            message >> end;

            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - _start);;
            std::cout << "[" << pSession->GetId() << "] Ping " << elapsed.count() << "us \n";

            std::this_thread::sleep_for(std::chrono::seconds(1));
            Ping(pSession);
        }

    private:
        TimePoint _start;
    
    };
}
