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
        using ErrorCode             = boost::system::error_code;
        using Tcp                   = boost::asio::ip::tcp;
        using SessionPointer        = Session::Pointer;
        using SessionId             = Session::Id;
        using SessionMap            = std::unordered_map<SessionId, SessionPointer>;

    public:
        ServiceBase(size_t nWorkers)
            : _workers(nWorkers)
            , _workGuard(boost::asio::make_work_guard(_workers))
            , _sessionsStrand(boost::asio::make_strand(_workers))
            , _receiveStrand(boost::asio::make_strand(_workers))
        {
            UpdateAsync();
        }

        virtual ~ServiceBase()
        {}

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
                                  BroadcastMessage(message, pIgnoredSession);
                              });
        }

        void JoinWorkers()
        {
            _workers.join();
        }

    protected:
        virtual bool OnSessionCreated(SessionPointer pSession) = 0;
        virtual void OnSessionRegistered(SessionPointer pSession) = 0;
        virtual void OnSessionUnregistered(SessionPointer pSession) = 0;
        virtual void HandleReceivedMessage(SessionPointer pSession, Message& message) = 0;
        virtual bool OnUpdateCompleted() = 0;

        void StopWorkers()
        {
            _workers.stop();
        }

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
                std::cout << "[" << pSession->GetEndpoint() << "] Session denied\n";
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
                                  DestroyAllSessions();
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

        void DestroyAllSessions()
        {
            for (auto& pair : _sessions)
            {
                SessionPointer pSession = pair.second;
                pSession->CloseAsync();
            }
        }

        void UnregisterSession(SessionPointer pSession)
        {
            assert(_sessions.count(pSession->GetId()) == 1);

            _sessions.erase(pSession->GetId());
            std::cout << pSession << " Session unregistered\n";

            OnSessionUnregistered(pSession);
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

        void BroadcastMessage(const Message& message, SessionPointer pIgnoredSession)
        {
            for (auto& pair : _sessions)
            {
                SessionPointer pSession = pair.second;

                if (pSession != pIgnoredSession)
                {
                    pSession->SendMessageAsync(message);
                }
            }
        }

        void UpdateAsync(size_t nMaxReceivedMessages = 0)
        {
            boost::asio::post(_receiveStrand,
                              [this, nMaxReceivedMessages]()
                              {
                                  FetchReceivedMessages(nMaxReceivedMessages);
                              });
        }

        void FetchReceivedMessages(size_t nMaxReceivedMessages)
        {
            if (nMaxReceivedMessages == 0)
            {
                _receivedMessages = std::move(_receiveBuffer);
            }
            else
            {
                for (size_t messageCount = 0; messageCount < nMaxReceivedMessages; ++messageCount)
                {
                    if (_receiveBuffer.empty())
                    {
                        break;
                    }

                    _receivedMessages.push(std::move(_receiveBuffer.front()));
                    _receiveBuffer.pop();
                }
            }

            boost::asio::post([this, nMaxReceivedMessages]()
                              {
                                   DispatchReceivedMessages(nMaxReceivedMessages);
                              });
        }

        void DispatchReceivedMessages(size_t nMaxReceivedMessages)
        {
            while (!_receivedMessages.empty())
            {
                OwnedMessage receiveMessage = std::move(_receivedMessages.front());
                _receivedMessages.pop();

                HandleReceivedMessage(receiveMessage.pOwner, receiveMessage.message);
            }

            if (OnUpdateCompleted())
            {
                UpdateAsync(nMaxReceivedMessages);
            }
        }

    protected:
        ThreadPool                      _workers;
        WorkGuard                       _workGuard;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;

        // Receive
        std::queue<OwnedMessage>        _receiveBuffer;
        Strand                          _receiveStrand;
        std::queue<OwnedMessage>        _receivedMessages;

    };
}
