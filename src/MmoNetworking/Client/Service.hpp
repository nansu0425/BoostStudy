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
        using TimePoint     = std::chrono::steady_clock::time_point;
        using Timer         = boost::asio::steady_timer;

    public:
        Service()
            : ClientServiceBase()
            , _timer(_ioContext)
        {}

    protected:
        virtual bool OnSessionCreated(SessionPointer pSession) override
        {
            return true;
        }

        virtual void OnSessionRegistered(SessionPointer pSession) override
        {
            Ping(pSession);
        }

        virtual void OnSessionUnregistered(SessionPointer pSession) override
        {}

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

        virtual bool OnUpdateCompleted() override
        {
            return true;
        }

    private:
        void Ping(SessionPointer pSession)
        {
            _start = std::chrono::steady_clock::now();

            Message message;
            message.header.id = static_cast<NetCommon::MessageId>(MessageId::Ping);

            SendMessageAsync(pSession, message);
        }

        void HandlePing(SessionPointer pSession, Message& message)
        {
            TimePoint end = std::chrono::steady_clock::now();

            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - _start);;
            std::cout << pSession << " Ping: " << elapsed.count() << "us \n";

            _timer.expires_after(std::chrono::seconds(1));
            _timer.async_wait([this, pSession](const ErrorCode& error)
                              {
                                  Ping(pSession);
                              });
        }

    private:
        TimePoint       _start;
        Timer           _timer;
    
    };
}
