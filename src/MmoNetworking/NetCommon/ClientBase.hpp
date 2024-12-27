#pragma once

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
        using Owner                 = typename TcpConnection<TMessageId>::Owner;
        using Strand                = boost::asio::strand<boost::asio::io_context::executor_type>;

    public:
        ClientBase()
            : _receiveBufferStrand(boost::asio::make_strand(_ioContext))
        {}
        
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
                _pServer = TcpConnection<TMessageId>::Create(Owner::Client,
                                                             _ioContext,
                                                             _receiveBuffer,
                                                             _receiveBufferStrand);

                Tcp::resolver resolver(_ioContext);
                _pServer->OnServerConnected();
            }
            catch (const std::exception&)
            {
                std::cerr << "[CLIENT] Failed to connect\n";
                throw;
            }
        }

        void Disconnect()
        {
            _pServer->Disconnect();
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

        std::queue<OwnedMessage>& ReceiveBuffer()
        {
            return _receiveBuffer;
        }

    protected:
        boost::asio::io_context     _ioContext;
        std::thread                 _worker;
        ConnectionPointer           _pServer;

    private:
        std::queue<OwnedMessage>    _receiveBuffer;
        Strand                      _receiveBufferStrand;

    };
}
