#pragma once

#include <NetCommon/TcpConnection.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    class ServerBase
    {
    private:
        using ConnectionPointer     = typename TcpConnection<TMessageId>::Pointer;
        using Tcp                   = boost::asio::ip::tcp;
        using OwnedMessage          = OwnedMessage<TMessageId>;
        using Message               = Message<TMessageId>;
        using ConnectionId          = typename TcpConnection<TMessageId>::Id;
        using ConnectionMap         = std::unordered_map<ConnectionId, ConnectionPointer>;

    public:
        ServerBase(boost::asio::ip::port_type port)
            : _acceptor(_ioContext, Tcp::endpoint(Tcp::v4, port))
        {}

        virtual ~ServerBase()
        {
            Stop();
        }

        void Start()
        {
            try
            {
                Listen();

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

        void Listen()
        {
            ConnectionPointer pClient = TcpConnection<TMessageId>::Create(_ioContext, _messagesReceived);
            assert(pClient != nullptr);

            _acceptor.async_accept(pClient->Socket(),
                                   [this, pClient](const boost::system::error_code& error)
                                   {
                                       HandleConnect(pClient, error);
                                   });
        }

        void Send(ConnectionPointer pClient, const Message& message)
        {
            assert(pClient != nullptr);

            if (pClient->IsConnected())
            {
                pClient->Send(message);
            }
            else
            {
                OnDisconnect(pClient);
                _clients.erase(pClient->GetId());
            }
        }

        void Broadcast(const Message& message, ConnectionPointer pIgnoredClient = nullptr)
        {
            for (auto iter = _clients.begin(); iter != _clients.end();)
            {
                ConnectionPointer pClient = *iter.second;
                assert(pClient != nullptr);

                if (pClient->IsConnected())
                {
                    if (pClient != pIgnoredClient)
                    {
                        pClient->Send(message);
                    }

                    ++iter;
                }
                else
                {
                    OnDisconnect(pClient);
                    iter = _clients.erase(iter);
                }
            }
        }

        void Update(size_t nMaxMessages = -1)
        {
            for (size_t messageCount = 0; messageCount < nMaxMessages; ++messageCount)
            {
                if (_messagesReceived.empty())
                {
                    break;
                }

                OwnedMessage ownedMessage = _messagesReceived.front();
                _messagesReceived.pop();

                OnReceive(ownedMessage->pOwner, ownedMessage->message);
            }
        }

    protected:
        virtual bool OnConnect(ConnectionPointer pClient) = 0;
        virtual void OnDisconnect(ConnectionPointer pClient) = 0;
        virtual void OnReceive(ConnectionPointer pClient, Message& message) = 0;

    private:
        void HandleConnect(ConnectionPointer pClient, const boost::system::error_code& error)
        {
            assert(pClient != nullptr);

            if (!error)
            {
                std::cout << "[SERVER] New connection: " << pClient->Socket()->remote_endpoint() << "\n";

                if (OnConnect(pClient))
                {
                    pClient->SetId(_nextClientId);
                    _clients[_nextClientId] = std::move(pClient);

                    std::cout << "[" << _nextClientId << "] Connection approved\n";
                    ++_nextClientId;
                }
                else
                {
                    std::cout << "[-----] Connection denied\n";
                }
            }
            else
            {
                std::cerr << "[SERVER] Failed to connect: " << error << "\n";
            }

            Listen();
        }

        void HandleReceive(ConnectionPointer pClient, Message& message)
        {}

    protected:
        boost::asio::io_context         _ioContext;
        std::thread                     _worker;
        Tcp::acceptor                   _acceptor;
        ConnectionId                    _nextClientId = 10000;
        ConnectionMap                   _clients;

    private:
        std::queue<OwnedMessage>        _messagesReceived;

    };
}
