#pragma once

#include <NetCommon/TcpConnection.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    class ServerBase
    {
    protected:
        using ClientPointer         = typename TcpConnection<TMessageId>::Pointer;
        using Tcp                   = boost::asio::ip::tcp;
        using OwnedMessage          = OwnedMessage<TMessageId>;
        using Message               = Message<TMessageId>;
        using ClientId              = typename TcpConnection<TMessageId>::Id;
        using ClientMap             = std::unordered_map<ClientId, ClientPointer>;
        using Owner                 = typename TcpConnection<TMessageId>::Owner;
        using Strand                = boost::asio::strand<boost::asio::io_context::executor_type>;

    public:
        ServerBase(uint16_t port)
            : _acceptor(_ioContext, Tcp::endpoint(Tcp::v4(), port))
            , _receiveBufferStrand(boost::asio::make_strand(_ioContext))
        {}

        virtual ~ServerBase()
        {
            Stop();
        }

        void Start()
        {
            try
            {
                AcceptAsync();

                _worker = std::thread([this]()
                                      {
                                          _ioContext.run();
                                      });
            }
            catch (const std::exception&)
            {
                std::cerr << "[SERVER] Failed to start: ";
                throw;
            }

            std::cout << "[SERVER] Started!\n";
        }

        void Stop()
        {
            _ioContext.stop();

            if (_worker.joinable())
            {
                _worker.join();
            }
        }

        void Send(ClientPointer pClient, const Message& message)
        {
            assert(pClient != nullptr);

            if (pClient->IsConnected())
            {
                pClient->SendAsync(message);
            }
            else
            {
                OnClientDisconnected(pClient);
                _clients.erase(pClient->GetId());
            }
        }

        void Broadcast(const Message& message, ClientPointer pIgnoredClient = nullptr)
        {
            for (auto iter = _clients.begin(); iter != _clients.end();)
            {
                ClientPointer pClient = *iter.second;
                assert(pClient != nullptr);

                if (pClient->IsConnected())
                {
                    if (pClient != pIgnoredClient)
                    {
                        pClient->SendAsync(message);
                    }

                    ++iter;
                }
                else
                {
                    OnClientDisconnected(pClient);
                    iter = _clients.erase(iter);
                }
            }
        }

        bool Update(size_t nMaxMessages = -1)
        {
            std::promise<bool> resultPromise;
            std::future<bool> resultFuture = resultPromise.get_future();

            boost::asio::post(_receiveBufferStrand,
                              [this, nMaxMessages, &resultPromise]()
                              {
                                  OnUpdateStarted(resultPromise, nMaxMessages);
                              });

            return resultFuture.get();
        }

    protected:
        virtual bool OnClientConnected(ClientPointer pClient) = 0;
        virtual void OnClientDisconnected(ClientPointer pClient) = 0;
        virtual void OnMessageReceived(ClientPointer pClient, Message& message) = 0;

    private:
        void AcceptAsync()
        {
            ClientPointer pClient = TcpConnection<TMessageId>::Create(Owner::Server,
                                                                      _ioContext,
                                                                      _receiveBuffer,
                                                                      _receiveBufferStrand);
            assert(pClient != nullptr);

            _acceptor.async_accept(pClient->Socket(),
                                   [this, pClient](const boost::system::error_code& error)
                                   {
                                       OnAcceptCompleted(pClient, error);
                                   });
        }

        void OnAcceptCompleted(ClientPointer pClient, const boost::system::error_code& error)
        {
            assert(pClient != nullptr);

            if (!error)
            {
                std::cout << "[SERVER] New client: " << pClient->Socket().remote_endpoint() << "\n";

                if (OnClientConnected(pClient))
                {
                    pClient->OnClientApproved(_nextClientId);
                    _clients[_nextClientId] = std::move(pClient);

                    std::cout << "[" << _nextClientId << "] Client approved\n";
                    ++_nextClientId;
                }
                else
                {
                    std::cout << "[-----] Client denied\n";
                }
            }
            else
            {
                std::cerr << "[SERVER] Failed to accept: " << error << "\n";
            }

            AcceptAsync();
        }

        void OnUpdateStarted(std::promise<bool>& updateResult, size_t nMaxMessages = -1)
        {
            if (nMaxMessages == -1)
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

            ProcessReceivedMessagesAsync(updateResult);
        }

        void ProcessReceivedMessagesAsync(std::promise<bool>& updateResult)
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

            updateResult.set_value(true);
        }

    protected:
        boost::asio::io_context         _ioContext;
        std::thread                     _worker;
        Tcp::acceptor                   _acceptor;
        ClientId                        _nextClientId = 10000;
        ClientMap                       _clients;

    private:
        std::queue<OwnedMessage>        _receiveBuffer;
        Strand                          _receiveBufferStrand;
        std::queue<OwnedMessage>        _receivedMessages;
        
    };
}
