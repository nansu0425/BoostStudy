#pragma once

#include <NetCommon/Include.hpp>
#include <NetCommon/Message.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    class TcpConnection 
        : public std::enable_shared_from_this<TcpConnection<TMessageId>>
    {
    public:
        using Pointer = std::shared_ptr<TcpConnection>;

        static Pointer Create(boost::asio::io_context& ioContext,
                              std::queue<OwnedMessage<TMessageId>>& receiveMessages)
        {
            return Pointer(new TcpConnection(ioContext, receiveMessages));
        }

    private:
        TcpConnection(boost::asio::io_context& ioContext,
                      std::queue<OwnedMessage<TMessageId>>& receiveMessages)
            : _strand(boost::asio::make_strand(ioContext))
            , _socket(ioContext)
            , _receiveMessages(receiveMessages)
        {}

    protected:
        boost::asio::strand<boost::asio::io_context::executor_type>     _strand;
        boost::asio::ip::tcp::socket                                    _socket;
        std::queue<Message<TMessageId>>                                 _sendMessages;
        std::queue<OwnedMessage<TMessageId>>&                           _receiveMessages;
    };
}
