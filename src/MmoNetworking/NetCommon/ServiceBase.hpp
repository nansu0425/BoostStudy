#pragma once

#include <NetCommon/Session.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    class ServiceBase
    {
    protected:
        using SessionPointer        = typename Session<TMessageId>::Pointer;
        using OwnedMessage          = OwnedMessage<TMessageId>;
        using Message               = Message<TMessageId>;
        using SessionId             = typename Session<TMessageId>::Id;
        using SessionMap            = std::unordered_map<SessionId, SessionPointer>;
        using Owner                 = typename Session<TMessageId>::Owner;
        using Strand                = boost::asio::strand<boost::asio::io_context::executor_type>;
        using WorkGuard             = boost::asio::executor_work_guard< boost::asio::io_context::executor_type>;

    public:
        ServiceBase()
            : _workGuard(boost::asio::make_work_guard(_ioContext))
            , _sessionsStrand(boost::asio::make_strand(_ioContext))
            , _receiveBufferStrand(boost::asio::make_strand(_ioContext))
        {}

        virtual ~ServiceBase()
        {
            Stop();
        }

        void Stop()
        {
            _ioContext.stop();

            if (_worker.joinable())
            {
                _worker.join();
            }
        }

        void SendAsync(SessionPointer pSession, const Message& message)
        {
            boost::asio::post(_sessionsStrand,
                              [this, pSession, &message]()
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
        virtual void OnSessionDisconnected(SessionPointer pSession) = 0;
        virtual void OnMessageReceived(SessionPointer pSession, Message& message) = 0;
        virtual bool OnUpdateStarted() = 0;

        void RunWorker()
        {
            _worker = std::thread([this]()
                                  {
                                      _ioContext.run();
                                  });
        }

    private:
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
                _sessions.erase(pSession->GetId());
            }
        }

        void OnBroadcastStarted(const Message& message, SessionPointer pIgnoredClient)
        {
            for (auto iter = _sessions.begin(); iter != _sessions.end();)
            {
                SessionPointer pSession = *iter.second;
                assert(pSession != nullptr);

                if (pSession->IsConnected())
                {
                    if (pSession != pIgnoredClient)
                    {
                        pSession->SendAsync(message);
                    }

                    ++iter;
                }
                else
                {
                    OnSessionDisconnected(pSession);
                    iter = _sessions.erase(iter);
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
        SessionId                       _nextSessionId = 10000;
        SessionMap                      _sessions;
        Strand                          _sessionsStrand;
        std::queue<OwnedMessage>        _receiveBuffer;
        Strand                          _receiveBufferStrand;
        std::queue<OwnedMessage>        _receivedMessages;

    };
}
