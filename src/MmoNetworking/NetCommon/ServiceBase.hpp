#pragma once

#include <NetCommon/Session.hpp>

namespace NetCommon
{
    class ServiceBase
    {
    protected:
        using ThreadPool            = boost::asio::thread_pool;
        using WorkGuard             = boost::asio::executor_work_guard<ThreadPool::executor_type>;
        using Strand                = boost::asio::strand<ThreadPool::executor_type>;
        using Timer                 = boost::asio::steady_timer;
        using Seconds               = std::chrono::seconds;
        using MicroSeconds          = std::chrono::microseconds;
        using TickRate              = uint32_t;
        using ErrorCode             = boost::system::error_code;
        using Tcp                   = boost::asio::ip::tcp;
        using SessionPointer        = Session::Pointer;
        using SessionId             = Session::Id;
        using SessionMap            = std::unordered_map<SessionId, SessionPointer>;
        using OwnedMessage          = Session::OwnedMessage;
        using OwnedMessageBuffer    = Session::OwnedMessageBuffer;

    public:
        ServiceBase(size_t nWorkers, size_t nMaxReceivedMessages)
            : _workers(nWorkers)
            , _workGuard(boost::asio::make_work_guard(_workers))
            , _sessionsStrand(boost::asio::make_strand(_workers))
            , _tickRateTimer(_workers)
            , _tickRate(0)
            , _receiveStrand(boost::asio::make_strand(_workers))
            , _nMaxReceivedMessages(nMaxReceivedMessages)
        {
            UpdateAsync();
            WaitTickRateTimerAsync();
        }

        virtual ~ServiceBase()
        {}

        void StopWorkers()
        {
            _workers.stop();
        }

        void JoinWorkers()
        {
            _workers.join();
        }

    protected:
        virtual SessionPointer OnSessionCreated(SessionPointer pSession, bool& isDenied) { return pSession; }
        virtual void OnSessionRegistered(SessionPointer pSession) {}
        virtual void OnSessionUnregistered(SessionPointer pSession) {}
        virtual void HandleReceivedMessage(OwnedMessage receivedMessage) {}
        virtual bool OnReceivedMessagesDispatched() { return true; }
        virtual void OnTickRateMeasured(const TickRate tickRate) {}

        void CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](SessionPointer pSession)
                                   {
                                       boost::asio::post(_sessionsStrand,
                                                         [this, pSession = std::move(pSession)]() mutable
                                                         {
                                                             UnregisterSession(std::move(pSession));
                                                         });
                                   };

            SessionPointer pSession = Session::Create(_workers,
                                                      std::move(socket),
                                                      AssignId(),
                                                      std::move(onSessionClosed),
                                                      _receiveBuffer,
                                                      _receiveStrand);
            std::cout << pSession << " Session created: " << pSession->GetEndpoint() << "\n";

            bool isDenied = false;
            pSession = OnSessionCreated(std::move(pSession), isDenied);

            if (isDenied)
            {
                std::cout << pSession << " Session denied: " << pSession->GetEndpoint() << "\n";
                return;   
            }

            RegisterSessionAsync(std::move(pSession));
        }

        void DestroySessionAsync(SessionPointer pSession)
        {
            pSession->CloseAsync();
        }

        void DestroyAllSessionsAsync()
        {
            boost::asio::post(_sessionsStrand,
                              [this]()
                              {
                                  for (auto& sessionPair : _sessions)
                                  {
                                      sessionPair.second->CloseAsync();
                                  }
                              });
        }

        template<typename TMessage>
        void SendMessageAsync(SessionPointer pSession, TMessage&& message)
        {
            assert(pSession != nullptr);

            pSession->SendMessageAsync(std::forward<TMessage>(message));
        }

        template<typename TMessage>
        void BroadcastMessageAsync(TMessage&& message, SessionPointer pIgnoredSession = nullptr)
        {
            boost::asio::post(_sessionsStrand,
                              [this, 
                              message = std::forward<TMessage>(message), 
                              pIgnoredSession = std::move(pIgnoredSession)]()
                              {
                                  for (auto& sessionPair : _sessions)
                                  {
                                      if (sessionPair.second != pIgnoredSession)
                                      {
                                          sessionPair.second->SendMessageAsync(message);
                                      }
                                  }
                              });
        }

    private:
        SessionId AssignId()
        {
            static SessionId id = 10000;
            SessionId assignedId = id;
            ++id;

            return assignedId;
        }

        void RegisterSessionAsync(SessionPointer pSession)
        {
            boost::asio::post(_sessionsStrand,
                              [this, pSession = std::move(pSession)]() mutable
                              {
                                  RegisterSession(std::move(pSession));
                              });
        }

        void RegisterSession(SessionPointer pSession)
        {
            const SessionId id = pSession->GetId();

            _sessions[id] = std::move(pSession);
            std::cout << _sessions[id] << " Session registered\n";

            OnSessionRegistered(_sessions[id]);

            _sessions[id]->ReceiveMessageAsync();
        }

        void UnregisterSession(SessionPointer pSession)
        {
            assert(_sessions.count(pSession->GetId()) == 1);

            _sessions.erase(pSession->GetId());
            std::cout << pSession << " Session unregistered\n";

            OnSessionUnregistered(std::move(pSession));
        }

        void UpdateAsync()
        {
            boost::asio::post(_receiveStrand,
                              [this]()
                              {
                                  FetchReceivedMessages();
                              });
        }

        void FetchReceivedMessages()
        {
            if (_nMaxReceivedMessages == 0)
            {
                _receivedMessages = std::move(_receiveBuffer);
            }
            else
            {
                for (size_t messageCount = 0; messageCount < _nMaxReceivedMessages; ++messageCount)
                {
                    if (_receiveBuffer.empty())
                    {
                        break;
                    }

                    _receivedMessages.emplace(std::move(_receiveBuffer.front()));
                    _receiveBuffer.pop();
                }
            }

            boost::asio::post([this]()
                              {
                                   DispatchReceivedMessages();
                              });
        }

        void DispatchReceivedMessages()
        {
            while (!_receivedMessages.empty())
            {
                HandleReceivedMessage(std::move(_receivedMessages.front()));
                _receivedMessages.pop();
            }

            const bool shouldUpdate = OnReceivedMessagesDispatched();
            OnUpdateCompleted(shouldUpdate);
        }

        void OnUpdateCompleted(const bool shouldUpdate)
        {
            _tickRate.fetch_add(1);

            if (shouldUpdate)
            {
                UpdateAsync();
            }
        }

        void WaitTickRateTimerAsync()
        {
            _tickRateTimer.expires_after(Seconds(1));
            _tickRateTimer.async_wait([this](const ErrorCode& error)
                                             {
                                                 OnTickRateTimerExpired(error);
                                             });
        }

        void OnTickRateTimerExpired(const ErrorCode& error)
        {
            if (error)
            {
                std::cerr << "[TICK_RATE_TIMER] Failed to wait: " << error << "\n";
                return;
            }

            WaitTickRateTimerAsync();

            const TickRate tickRate = _tickRate.exchange(0);
            OnTickRateMeasured(tickRate);
        }

    protected:
        ThreadPool                      _workers;
        WorkGuard                       _workGuard;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;

        // Update
        Timer                           _tickRateTimer;
        std::atomic<TickRate>           _tickRate;

        // Receive
        OwnedMessageBuffer              _receiveBuffer;
        Strand                          _receiveStrand;
        OwnedMessageBuffer              _receivedMessages;
        const size_t                    _nMaxReceivedMessages;

    };
}
