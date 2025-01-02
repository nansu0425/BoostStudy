#pragma once

#include <NetCommon/Session.hpp>

namespace NetCommon
{
    class ServiceBase
    {
    protected:
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
            RunWorker();
        }

        virtual ~ServiceBase()
        {
            StopWorker();
        }

        bool Update(size_t nMaxMessages = 0)
        {
            std::promise<bool> resultPromise;
            std::future<bool> resultFuture = resultPromise.get_future();

            boost::asio::post(_receiveStrand,
                              [this, nMaxMessages, &resultPromise]()
                              {
                                  FetchReceivedMessages(resultPromise, nMaxMessages);
                              });

            return resultFuture.get();
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
                                  BroadcastMessage(message, pIgnoredSession);
                              });
        }

    protected:
        virtual bool OnSessionConnected(SessionPointer pSession) = 0;
        virtual void OnSessionRegistered(SessionPointer pSession) = 0;
        virtual void OnSessionDisconnected(SessionPointer pSession) = 0;
        virtual void OnMessageReceived(SessionPointer pSession, Message& message) = 0;
        virtual bool OnUpdateStarted() = 0;

        SessionPointer CreateSession(Tcp::socket&& socket)
        {
            auto onSessionClosed = [this](SessionPointer pSession)
                                   {
                                       boost::asio::post(_sessionsStrand,
                                                         [this, pSession]()
                                                         {
                                                             UnregisterSession(pSession);
                                                         });
                                   };

            SessionPointer pSession = Session::Create(AssignId(),
                                                      _ioContext,
                                                      std::move(socket),
                                                      std::move(onSessionClosed),
                                                      _receiveBuffer,
                                                      _receiveStrand);

            return pSession;
        }

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

        void DisconnectSessionAsync(SessionPointer pSession)
        {
            pSession->CloseAsync();
        }

        void DisconnectAllSessionsAsync()
        {
            boost::asio::post(_sessionsStrand,
                              [this]()
                              {
                                  DisconnectAllSessions();
                              });
        }

    private:
        void RunWorker()
        {
            _worker = std::thread([this]()
                                  {
                                      _ioContext.run();
                                  });
        }

        void StopWorker()
        {
            _ioContext.stop();

            if (_worker.joinable())
            {
                _worker.join();
            }
        }

        void DisconnectAllSessions()
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
            OnSessionDisconnected(pSession);
        }

        void RegisterSession(SessionPointer pSession)
        {
            _sessions[pSession->GetId()] = pSession;
            pSession->ReceiveMessageAsync();

            OnSessionRegistered(pSession);
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

        void FetchReceivedMessages(std::promise<bool>& updateResult, size_t nMaxMessages)
        {
            if (nMaxMessages == 0)
            {
                _receivedMessages = std::move(_receiveBuffer);
            }
            else
            {
                for (size_t messageCount = 0; messageCount < nMaxMessages; ++messageCount)
                {
                    if (_receiveBuffer.empty())
                    {
                        break;
                    }

                    _receivedMessages.push(std::move(_receiveBuffer.front()));
                    _receiveBuffer.pop();
                }
            }

            boost::asio::post([this, &updateResult]()
                               {
                                    ProcessReceivedMessages(updateResult);
                               });
        }

        void ProcessReceivedMessages(std::promise<bool>& updateResult)
        {
            while (!_receivedMessages.empty())
            {
                OwnedMessage receiveMessage = std::move(_receivedMessages.front());
                _receivedMessages.pop();

                OnMessageReceived(receiveMessage.pOwner, receiveMessage.message);
            }

            updateResult.set_value(OnUpdateStarted());
        }

    protected:
        boost::asio::io_context         _ioContext;
        WorkGuard                       _workGuard;
        std::thread                     _worker;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;

        // Receive
        std::queue<OwnedMessage>        _receiveBuffer;
        Strand                          _receiveStrand;
        std::queue<OwnedMessage>        _receivedMessages;

    };
}
