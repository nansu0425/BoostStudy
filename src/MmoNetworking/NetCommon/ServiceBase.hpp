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
        using TickRate              = uint32_t;
        using ErrorCode             = boost::system::error_code;
        using Tcp                   = boost::asio::ip::tcp;
        using SessionPointer        = Session::Pointer;
        using SessionId             = Session::Id;
        using SessionMap            = std::unordered_map<SessionId, SessionPointer>;

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
        virtual bool OnSessionCreated(SessionPointer pSession) { return true; }
        virtual void OnSessionRegistered(SessionPointer pSession) {}
        virtual void OnSessionUnregistered(SessionPointer pSession) {}
        virtual void HandleReceivedMessage(SessionPointer pSession, Message& message) {}
        virtual bool OnReceivedMessagesDispatched() { return true; }
        virtual void OnTickRateMeasured(const TickRate measured) {}

        void CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](SessionPointer pSession)
                                   {
                                       boost::asio::post(_sessionsStrand,
                                                         [this, pSession]()
                                                         {
                                                             UnregisterSession(pSession);
                                                         });
                                   };

            SessionPointer pSession = Session::Create(_workers,
                                                      std::move(socket),
                                                      AssignId(),
                                                      std::move(onSessionClosed),
                                                      _receiveBuffer,
                                                      _receiveStrand);
            std::cout << pSession << " Session created: " << pSession->GetEndpoint() << "\n";

            if (OnSessionCreated(pSession))
            {
                RegisterSessionAsync(pSession);
            }
            else
            {
                std::cout << pSession << " Session denied: " << pSession->GetEndpoint() << "\n";
            }
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
                                  for (auto& pair : _sessions)
                                  {
                                      SessionPointer pSession = pair.second;
                                      pSession->CloseAsync();
                                  }
                              });
        }

        void SendMessageAsync(SessionPointer pSession, const Message& message)
        {
            assert(pSession != nullptr);

            pSession->SendMessageAsync(message);
        }

        void BroadcastMessageAsync(const Message& message, SessionPointer pIgnoredSession = nullptr)
        {
            boost::asio::post(_sessionsStrand,
                              [this, &message, pIgnoredSession]()
                              {
                                  for (auto& pair : _sessions)
                                  {
                                      SessionPointer pSession = pair.second;

                                      if (pSession != pIgnoredSession)
                                      {
                                          pSession->SendMessageAsync(message);
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
                              [this, pSession]()
                              {
                                  RegisterSession(pSession);
                              });
        }

        void RegisterSession(SessionPointer pSession)
        {
            _sessions[pSession->GetId()] = pSession;
            std::cout << _sessions[pSession->GetId()] << " Session registered\n";

            OnSessionRegistered(pSession);

            pSession->ReceiveMessageAsync();
        }

        void UnregisterSession(SessionPointer pSession)
        {
            assert(_sessions.count(pSession->GetId()) == 1);

            _sessions.erase(pSession->GetId());
            std::cout << pSession << " Session unregistered\n";

            OnSessionUnregistered(pSession);
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

                    _receivedMessages.push(std::move(_receiveBuffer.front()));
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
                OwnedMessage receiveMessage = std::move(_receivedMessages.front());
                _receivedMessages.pop();

                HandleReceivedMessage(receiveMessage.pOwner, receiveMessage.message);
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

            const TickRate measured = _tickRate.exchange(0);
            OnTickRateMeasured(measured);
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
        std::queue<OwnedMessage>        _receiveBuffer;
        Strand                          _receiveStrand;
        std::queue<OwnedMessage>        _receivedMessages;
        const size_t                    _nMaxReceivedMessages;

    };
}
