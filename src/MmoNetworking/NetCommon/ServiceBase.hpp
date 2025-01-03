#pragma once

#include <NetCommon/Session.hpp>

namespace NetCommon
{
    class ServiceBase
    {
    protected:
        using IoContext             = boost::asio::io_context;
        using ThreadPool            = std::thread;
        using SessionPointer        = Session::Pointer;
        using SessionId             = Session::Id;
        using SessionMap            = std::unordered_map<SessionId, SessionPointer>;
        using Strand                = boost::asio::strand<boost::asio::io_context::executor_type>;
        using WorkGuard             = boost::asio::executor_work_guard< boost::asio::io_context::executor_type>;
        using ErrorCode             = boost::system::error_code;
        using Tcp                   = boost::asio::ip::tcp;

    public:
        ServiceBase()
            : _workGuard(boost::asio::make_work_guard(_ioContext))
            , _sessionsStrand(boost::asio::make_strand(_ioContext))
            , _receiveStrand(boost::asio::make_strand(_ioContext))
        {
            UpdateAsync();
            RunWorker();
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

        void StopWorker()
        {
            _ioContext.stop();
        }

        void JoinWorkers()
        {
            if (_workers.joinable())
            {
                _workers.join();
            }
        }

    protected:
        virtual bool OnSessionCreated(SessionPointer pSession) = 0;
        virtual void OnSessionRegistered(SessionPointer pSession) = 0;
        virtual void OnSessionUnregistered(SessionPointer pSession) = 0;
        virtual void OnMessageReceived(SessionPointer pSession, Message& message) = 0;
        virtual bool OnUpdateCompleted() = 0;

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

            SessionPointer pSession = Session::Create(_ioContext,
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
        void RunWorker()
        {
            _workers = std::thread([this]()
                                  {
                                      _ioContext.run();
                                  });
        }

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
                                   ProcessReceivedMessages(nMaxReceivedMessages);
                              });
        }

        void ProcessReceivedMessages(size_t nMaxReceivedMessages)
        {
            while (!_receivedMessages.empty())
            {
                OwnedMessage receiveMessage = std::move(_receivedMessages.front());
                _receivedMessages.pop();

                OnMessageReceived(receiveMessage.pOwner, receiveMessage.message);
            }

            if (OnUpdateCompleted())
            {
                UpdateAsync(nMaxReceivedMessages);
            }
        }

    protected:
        IoContext                       _ioContext;
        WorkGuard                       _workGuard;
        ThreadPool                      _workers;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;

        // Receive
        std::queue<OwnedMessage>        _receiveBuffer;
        Strand                          _receiveStrand;
        std::queue<OwnedMessage>        _receivedMessages;

    };
}
