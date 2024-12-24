#pragma once

#include <NetCommon/Include.hpp>
#include <NetCommon/Message.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    class TcpConnection 
        : public std::enable_shared_from_this<TcpConnection<TMessageId>>
    {
    private:
        using Tcp           = typename boost::asio::ip::tcp;
        using Strand        = boost::asio::strand<boost::asio::io_context::executor_type>;
        using Message       = Message<TMessageId>;
        using OwnedMessage  = OwnedMessage<TMessageId>;

    public:
        using Pointer       = std::shared_ptr<TcpConnection>;
        using Id            = uint32_t;

        enum class OwnerType
        {
            Server,
            Client,
        };

    public:
        static Pointer Create(OwnerType ownerType,
                              boost::asio::io_context& ioContext,
                              std::queue<OwnedMessage>& messagesReceived)
        {
            return Pointer(new TcpConnection(ownerType, ioContext, messagesReceived));
        }

        bool IsConnected() const
        {
            boost::system::error_code error;
            _socket.remote_endpoint(error);

            return !error;
        }

        // Called when the client successfully connects to the server
        void OnServerConnected()
        {
            assert(_ownerType == OwnerType::Client);
        }

        // Called when a new client connection is accepted on the server
        void OnClientConnected(uint32_t clientId)
        {
            assert(_ownerType == OwnerType::Server);
            assert(IsConnected());

            SetId(clientId);
        }

        void Disconnect()
        {
        }

        Tcp::socket& Socket()
        {
            return _socket;
        }

        Id GetId() const
        {
            return _id;
        }

        void SetId(Id id)
        {
            _id = id;
        }

    private:
        TcpConnection(OwnerType ownerType, 
                      boost::asio::io_context& ioContext,
                      std::queue<OwnedMessage>& _messagesReceived)
            : _ownerType(ownerType)
            , _strand(boost::asio::make_strand(ioContext))
            , _socket(ioContext)
            , _messagesReceived(_messagesReceived)
        {}

    protected:
        Id                              _id = -1;
        OwnerType                       _ownerType;
        Strand                          _strand;
        Tcp::socket                     _socket;
        std::queue<Message>             _messagesSend;
        std::queue<OwnedMessage>&       _messagesReceived;

    };
}
