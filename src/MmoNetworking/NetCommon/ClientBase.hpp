#pragma once

#include <NetCommon/Include.hpp>
#include <NetCommon/TcpConnection.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    class ClientBase
    {
    private:
        using ConnectionPointer     = typename TcpConnection<TMessageId>::Pointer;
        using Tcp                   = boost::asio::ip::tcp;
        using OwnedMessage          = OwnedMessage<TMessageId>;
        using Message               = Message<TMessageId>;

    public:
        ClientBase() = default;
        
        virtual ~ClientBase()
        {
            Disconnect();
            _ioContext.stop();

            if (_worker.joinable())
            {
                _worker.join();
            }
        }

    public:
        void Connect(std::string_view host, 
                     std::string_view service)
        {
            try
            {
                _pServer = TcpConnection<TMessageId>::Create(_ioContext, _messagesReceived);

                Tcp::resolver resolver(_ioContext);
                _pServer->ConnectToServer(resolver.resolve(host, service));
            }
            catch (const std::exception&)
            {
                std::cerr << "[CLIENT] Failed to connect\n";
                throw;
            }
        }

        void Disconnect()
        {
            if (IsConnected())
            {
                _pServer->Disconnect();
            }
        }

        bool IsConnected()
        {
            if (_pServer != nullptr)
            {
                return _pServer->IsConnected();
            }
            else
            {
                return false;
            }
        }

        void Send(const Message& message)
        {

        }

        std::queue<OwnedMessage>& ReceiveMessages()
        {
            return _messagesReceived;
        }

    protected:
        boost::asio::io_context     _ioContext;
        std::thread                 _worker;
        ConnectionPointer           _pServer;

    private:
        std::queue<OwnedMessage>    _messagesReceived;
    };
}
