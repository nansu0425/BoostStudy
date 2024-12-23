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

        static Pointer Create(boost::asio::io_context& ioContext,
                              std::queue<OwnedMessage>& receiveMessages)
        {
            return Pointer(new TcpConnection(ioContext, receiveMessages));
        }

        bool IsConnected()
        {
            return false;
        }

        void ConnectToServer(Tcp::resolver::results_type&& endpoints)
        {
            _endpoints = std::move(endpoints);
        }

        void Disconnect()
        {

        }

    private:
        TcpConnection(boost::asio::io_context& ioContext,
                      std::queue<OwnedMessage>& receiveMessages)
            : _strand(boost::asio::make_strand(ioContext))
            , _socket(ioContext)
            , _receiveMessages(receiveMessages)
        {}

    protected:
        Strand                          _strand;
        Tcp::socket                     _socket;
        Tcp::resolver::results_type     _endpoints;
        std::queue<Message>             _sendMessages;
        std::queue<OwnedMessage>&       _receiveMessages;
        
    };
}
