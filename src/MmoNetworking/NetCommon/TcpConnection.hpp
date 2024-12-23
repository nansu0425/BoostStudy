﻿#pragma once

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
                              std::queue<OwnedMessage>& _messagesReceived)
        {
            return Pointer(new TcpConnection(ioContext, _messagesReceived));
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
                      std::queue<OwnedMessage>& _messagesReceived)
            : _strand(boost::asio::make_strand(ioContext))
            , _socket(ioContext)
            , _messagesReceived(_messagesReceived)
        {}

    protected:
        Strand                          _strand;
        Tcp::socket                     _socket;
        Tcp::resolver::results_type     _endpoints;
        std::queue<Message>             _messagesSend;
        std::queue<OwnedMessage>&       _messagesReceived;
        
    };
}
