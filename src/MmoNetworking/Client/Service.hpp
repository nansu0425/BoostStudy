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

        struct EchoTimer
        {
            using Pointer   = std::unique_ptr<EchoTimer>;
            using Map       = std::unordered_map<SessionId, Pointer>;

            SessionId       id;
            Timer           timer;
            TimePoint       start;

            EchoTimer(SessionId id,
                      ThreadPool& workers)
                : id(id)
                , timer(workers)
            {}

            ~EchoTimer()
            {
                std::cout << "[" << id << "] EchoTimer destroyed\n";
            }
        };

    public:
        Service(size_t nWorkers,
                size_t nMaxReceivedMessages,
                uint16_t nConnects)
            : ClientServiceBase(nWorkers,
                                nMaxReceivedMessages,
                                nConnects)
            , _echoTimersStrand(boost::asio::make_strand(_workers))
        {}

    protected:
        virtual void OnSessionRegistered(SessionPointer pSession) override
        {
            boost::asio::post(_echoTimersStrand,
                              [this, pSession = std::move(pSession)]() mutable
                              {
                                  const SessionId id = pSession->GetId();

                                  _echoTimers.emplace(id,
                                                      std::make_unique<EchoTimer>(id,
                                                                                  _workers));
                                  
                                  Echo(std::move(pSession));
                              });
        }

        virtual void OnSessionUnregistered(SessionPointer pSession) override 
        {
            boost::asio::post(_echoTimersStrand,
                              [this, id = pSession->GetId()]()
                              {
                                  _echoTimers.erase(id);
                              });
        }

        virtual void HandleReceivedMessage(OwnedMessage receivedMessage) override
        {
            Server::MessageId messageId = 
                static_cast<Server::MessageId>(receivedMessage.message.header.id);

            switch (messageId)
            {
            case Server::MessageId::Echo:
                HandleEchoAsync(std::move(receivedMessage.pOwner));
                break;

            default:
                break;
            }
        }

    private:
        void Echo(SessionPointer pSession)
        {
            const SessionId id = pSession->GetId();

            if (_echoTimers.count(id) == 0)
            {
                std::cerr << pSession << " Echo error: non-existent EchoTimer\n";
                return;
            }

            _echoTimers[id]->start = std::chrono::steady_clock::now();

            Message message;
            message.header.id = static_cast<NetCommon::Message::Id>(MessageId::Echo);

            SendMessageAsync(std::move(pSession), std::move(message));
        }

        void HandleEchoAsync(SessionPointer pSession)
        {
            TimePoint end = std::chrono::steady_clock::now();
            const SessionId id = pSession->GetId();

            auto elapsed = std::chrono::duration_cast<MicroSeconds>(end - _echoTimers[id]->start);

            boost::asio::post(_echoTimersStrand,
                              [this, pSession = std::move(pSession)]() mutable
                              {
                                  OnEchoCompleted(std::move(pSession));
                              });

            std::cout << "[" << id << "] Echo: " << elapsed.count() << "us \n";
        }

        void OnEchoCompleted(SessionPointer pSession)
        {
            const SessionId id = pSession->GetId();

            if (_echoTimers.count(id) == 0)
            {
                std::cerr << pSession << " OnEchoCompleted error: non-existent EchoTimer\n";
                return;
            }

            _echoTimers[id]->timer.expires_after(Seconds(1));
            _echoTimers[id]->timer.async_wait([this, 
                                              pSession = std::move(pSession)](const ErrorCode& error) mutable
                                              {
                                                  OnEchoTimerExpired(error, std::move(pSession));
                                              });
        }

        void OnEchoTimerExpired(const ErrorCode& error, SessionPointer pSession)
        {
            if (error)
            {
                std::cerr << pSession << " EchoTimer error: " << error << "\n";
                return;
            }

            EchoAsync(std::move(pSession));
        }

        void EchoAsync(SessionPointer pSession)
        {
            boost::asio::post(_echoTimersStrand,
                              [this,
                              pSession = std::move(pSession)]() mutable
                              {
                                  Echo(std::move(pSession));
                              });
        }

    private:
        EchoTimer::Map      _echoTimers;
        Strand              _echoTimersStrand;
    
    };
}
