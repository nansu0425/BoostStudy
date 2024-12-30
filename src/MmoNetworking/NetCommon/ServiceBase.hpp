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
            , _receiveBufferStrand(boost::asio::make_strand(_ioContext))
        {
            RunWorker();
        }

        virtual ~ServiceBase()
        {
            DisconnectAll();
            StopWorker();
        }

        bool Update(size_t nMaxMessages = 0)
        {
            std::promise<bool> resultPromise;
            std::future<bool> resultFuture = resultPromise.get_future();

            boost::asio::post(_receiveBufferStrand,
                              [this, nMaxMessages, &resultPromise]()
                              {
                                  OnFetchReceivedMessagesStarted(resultPromise, nMaxMessages);
                              });

            return resultFuture.get();
        }

    protected:
        virtual bool OnSessionConnected(SessionPointer pSession) = 0;
        virtual void OnSessionDisconnected(SessionPointer pSession) = 0;
        virtual void OnMessageReceived(SessionPointer pSession, Message& message) = 0;
        virtual bool OnUpdateStarted() = 0;

        void SendAsync(SessionPointer pSession, const Message& message)
        {
            boost::asio::post([this, pSession, &message]()
                              {
                                  OnSendStarted(pSession, message);
                              });
        }

        void BroadcastAsync(const Message& message, SessionPointer pIgnoredSession = nullptr)
        {
            boost::asio::post(_sessionsStrand,
                              [this, &message, pIgnoredSession]()
                              {
                                  OnBroadcastStarted(message, pIgnoredSession);
                              });
        }

        void Disconnect(SessionPointer pSession)
        {
            pSession->Disconnect();
            OnSessionDisconnected(pSession);

            EraseSessionAsync(pSession);
        }

        void DisconnectAll()
        {
            for (auto& pair : _sessions)
            {
                SessionPointer pSession = pair.second;

                pSession->Disconnect();
                OnSessionDisconnected(pSession);
            }

            ClearSessionsAsync();
        }

        bool IsConnected(SessionPointer pSession) const
        {
            return (pSession)
                   ? pSession->IsConnected()
                   : false;
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
                                  OnRegisterSessionStarted(pSession);
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

        void EraseSessionAsync(SessionPointer pSession)
        {
            boost::asio::post(_sessionsStrand,
                              [this, pSession]()
                              {
                                  OnEraseSessionStarted(pSession);
                              });
        }

        void OnEraseSessionStarted(SessionPointer pSession)
        {
            _sessions.erase(pSession->GetId());
        }

        void ClearSessionsAsync()
        {
            boost::asio::post(_sessionsStrand,
                              [this]()
                              {
                                  OnClearSessionsStarted();
                              });
        }

        void OnRegisterSessionStarted(SessionPointer pSession)
        {
            _sessions[pSession->GetId()] = pSession;

            OnRegisterSessionCompleted(pSession);
        }

        void OnRegisterSessionCompleted(SessionPointer pSession)
        {
            assert(_sessions.count(pSession->GetId()) == 1);

            pSession->ReadAsync();
            std::cout << "[" << pSession->GetId() << "] Session registered\n";
        }

        void OnClearSessionsStarted()
        {
            _sessions.clear();
        }

        void OnSendStarted(SessionPointer pSession, const Message& message)
        {
            assert(pSession != nullptr);

            if (pSession->IsConnected())
            {
                pSession->SendAsync(message);
            }
            else
            {
                OnSessionDisconnected(pSession);
                EraseSessionAsync(pSession);
            }
        }

        void OnBroadcastStarted(const Message& message, SessionPointer pIgnoredSession)
        {
            for (auto iter = _sessions.begin(); iter != _sessions.end();)
            {
                SessionPointer pSession = iter->second;
                assert(pSession != nullptr);

                if (pSession->IsConnected())
                {
                    if (pSession != pIgnoredSession)
                    {
                        pSession->SendAsync(message);
                    }

                    ++iter;
                }
                else
                {
                    OnSessionDisconnected(pSession);
                    EraseSessionAsync(pSession);
                }
            }
        }

        void OnFetchReceivedMessagesStarted(std::promise<bool>& updateResult, size_t nMaxMessages)
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

            OnFetchReceivedMessagesCompleted(updateResult);
        }

        void OnFetchReceivedMessagesCompleted(std::promise<bool>& updateResult)
        {
            boost::asio::post([this, &updateResult]()
                              {
                                  OnProcessReceivedMessagesStarted(updateResult);
                              });
        }

        void OnProcessReceivedMessagesStarted(std::promise<bool>& updateResult)
        {
            while (!_receivedMessages.empty())
            {
                OwnedMessage receiveMessage = std::move(_receivedMessages.front());
                _receivedMessages.pop();

                OnMessageReceived(receiveMessage.pOwner, receiveMessage.message);
            }

            OnProcessReceivedMessagesCompleted(updateResult);
        }

        void OnProcessReceivedMessagesCompleted(std::promise<bool>& updateResult)
        {
            updateResult.set_value(OnUpdateStarted());
        }

    protected:
        boost::asio::io_context         _ioContext;
        WorkGuard                       _workGuard;
        std::thread                     _worker;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;
        std::queue<OwnedMessage>        _receiveBuffer;
        Strand                          _receiveBufferStrand;
        std::queue<OwnedMessage>        _receivedMessages;

    };
}
