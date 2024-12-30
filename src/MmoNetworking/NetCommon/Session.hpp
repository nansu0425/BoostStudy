﻿#pragma once

#include <NetCommon/Include.hpp>
#include <NetCommon/Message.hpp>

namespace NetCommon
{
    class Session 
        : public std::enable_shared_from_this<Session>
    {
    private:
        using Tcp               = boost::asio::ip::tcp;
        using Strand            = boost::asio::strand<boost::asio::io_context::executor_type>;
        using ErrorCode         = boost::system::error_code;
        using Endpoints         = boost::asio::ip::basic_resolver_results<Tcp>;

    public:
        using Pointer           = std::shared_ptr<Session>;
        using Id                = uint32_t;

    public:
        static Pointer Create(Id id,
                              boost::asio::io_context& ioContext,
                              std::queue<OwnedMessage>& receiveBuffer,
                              Strand& receiveBufferStrand)
        {
            return Pointer(new Session(id, 
                                       ioContext, 
                                       receiveBuffer, 
                                       receiveBufferStrand));
        }

        void Connect(const Endpoints& endpoints)
        {
            boost::asio::connect(_socket, endpoints);
        }

        void Disconnect()
        {
            _socket.close();
        }

        bool IsConnected() const
        {
            ErrorCode error;
            _socket.remote_endpoint(error);

            return !error;
        }

        void ReadAsync()
        {
            assert(IsConnected());

            ReadHeaderAsync();
        }

        void SendAsync(const Message& message)
        {
            boost::asio::post(_sendBufferStrand,
                              [self = shared_from_this(), message]()
                              {
                                  self->OnSendStarted(message);
                              });
        }

        Tcp::socket& Socket()
        {
            return _socket;
        }

        Id GetId() const
        {
            return _id;
        }

    private:
        Session(Id id,
                boost::asio::io_context& ioContext,
                std::queue<OwnedMessage>& receiveBuffer,
                Strand& receiveBufferStrand)
            : _id(id)
            , _ioContext(ioContext)
            , _socket(ioContext)
            , _sendBufferStrand(boost::asio::make_strand(ioContext))
            , _receiveBuffer(receiveBuffer)
            , _receiveBufferStrand(receiveBufferStrand)
        {}

        void ReadHeaderAsync()
        {
            boost::asio::async_read(_socket,
                                    boost::asio::buffer(&_readMessage.header,
                                                        sizeof(MessageHeader)),
                                    [pSelf = shared_from_this()](const ErrorCode& error,
                                                                 const size_t nBytesTransferred)
                                    {
                                        pSelf->OnReadHeaderCompleted(error, nBytesTransferred);
                                    });
        }

        void OnReadHeaderCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(nBytesTransferred == sizeof(MessageHeader));
                assert(_readMessage.header.size >= sizeof(MessageHeader));

                // The size of payload is bigger than 0
                if (_readMessage.header.size > sizeof(MessageHeader))
                {
                    _readMessage.payload.resize(_readMessage.header.size - sizeof(MessageHeader));
                    ReadPayloadAsync();
                }
                else
                {
                    PushToReceiveBufferAsync();
                }
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to read header: " << error << "\n";
                _socket.close();
            }
        }

        void ReadPayloadAsync()
        {
            boost::asio::async_read(_socket,
                                    boost::asio::buffer(_readMessage.payload.data(),
                                                        _readMessage.payload.size()),
                                    [pSelf = shared_from_this()](const ErrorCode& error,
                                                                 const size_t nBytesTransferred)
                                    {
                                        pSelf->OnReadPayloadCompleted(error, nBytesTransferred);
                                    });
        }

        void OnReadPayloadCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(_readMessage.payload.size() == nBytesTransferred);

                PushToReceiveBufferAsync();
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to read payload: " << error << "\n";
                _socket.close();
            }
        }

        void PushToReceiveBufferAsync()
        {
            boost::asio::post(_receiveBufferStrand,
                              [pSelf = shared_from_this()]()
                              {
                                  pSelf->OnPushToReceiveBufferStarted();
                              });
        }

        void OnPushToReceiveBufferStarted()
        {
            _receiveBuffer.push(OwnedMessage{shared_from_this(), _readMessage});

            ReadHeaderAsync();
        }

        void OnSendStarted(const Message& message)
        {
            bool isWritingMessage = !_sendBuffer.empty();

            _sendBuffer.push(message);

            if (!isWritingMessage)
            {
                WriteHeaderAsync();
            }
        }

        void WriteHeaderAsync()
        {
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(&_sendBuffer.front().header, 
                                                         sizeof(MessageHeader)),
                                     boost::asio::bind_executor(_sendBufferStrand,
                                                                [pSelf = shared_from_this()](const ErrorCode& error,
                                                                                             const size_t nBytesTransferred)
                                                                {
                                                                    pSelf->OnWriteHeaderCompleted(error, nBytesTransferred);
                                                                }));
        }

        void OnWriteHeaderCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(sizeof(MessageHeader) == nBytesTransferred);

                // The size of payload is bigger than 0
                if (_sendBuffer.front().header.size > sizeof(MessageHeader))
                {
                    WritePayloadAsync();
                }
                else
                {
                    PopFromSendBufferAsync();
                }
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write header: " << error << "\n";
                _socket.close();
            }
        }

        void WritePayloadAsync()
        {
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(_sendBuffer.front().payload.data(),
                                                         _sendBuffer.front().payload.size()),
                                     boost::asio::bind_executor(_sendBufferStrand,
                                                                [pSelf = shared_from_this()](const ErrorCode& error,
                                                                                             const size_t nBytesTransferred)
                                                                {
                                                                    pSelf->OnWritePayloadCompleted(error, nBytesTransferred);
                                                                }));
        }

        void OnWritePayloadCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(nBytesTransferred == _sendBuffer.front().payload.size());

                PopFromSendBufferAsync();
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write payload: " << error << "\n";
                _socket.close();
            }
        }

        void PopFromSendBufferAsync()
        {
            boost::asio::post(_sendBufferStrand,
                              [pSelf = shared_from_this()]()
                              {
                                  pSelf->OnPopFromSendBufferStarted();
                              });
        }

        void OnPopFromSendBufferStarted()
        {
            _sendBuffer.pop();

            OnPopFromSendBufferCompleted();
        }

        void OnPopFromSendBufferCompleted()
        {
            if (!_sendBuffer.empty())
            {
                WriteHeaderAsync();
            }
        }

    protected:
        const Id                        _id;
        boost::asio::io_context&        _ioContext;
        Tcp::socket                     _socket;
        std::queue<Message>             _sendBuffer;
        Strand                          _sendBufferStrand;
        std::queue<OwnedMessage>&       _receiveBuffer;
        Strand&                         _receiveBufferStrand;

    private:
        Message                         _readMessage;

    };
}
