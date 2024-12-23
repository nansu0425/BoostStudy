#pragma once

#include <NetCommon/Include.hpp>
#include <NetCommon/TcpConnection.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    class IClient
    {
    private:
        using ConnectionPointer     = typename TcpConnection<TMessageId>::Pointer;
        using Tcp                   = boost::asio::ip::tcp;
        using OwnedMessage          = OwnedMessage<TMessageId>;
        using Message               = Message<TMessageId>;

    public:
        IClient() = default;
        
        virtual ~IClient()
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
                _pConnection = TcpConnection<TMessageId>::Create(_ioContext, _messagesReceived);

                Tcp::resolver resolver(_ioContext);
                _pConnection->ConnectToServer(resolver.resolve(host, service));
            }
            catch (const std::exception&)
            {
                std::cerr << "IClient: Connect: ";
                throw;
            }
        }

        void Disconnect()
        {
            if (IsConnected())
            {
                _pConnection->Disconnect();
            }
        }

        bool IsConnected()
        {
            if (_pConnection != nullptr)
            {
                return _pConnection->IsConnected();
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
        ConnectionPointer           _pConnection;

    private:
        std::queue<OwnedMessage>    _messagesReceived;
    };
}
