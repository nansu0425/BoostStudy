﻿#pragma once

#include <NetCommon/Include.hpp>
#include <NetCommon/Message.hpp>

namespace NetCommon
{
    class Session 
        : public std::enable_shared_from_this<Session>
    {
    private:
        using IoContext         = boost::asio::io_context;
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
                              Tcp::socket&& socket,
                              std::queue<OwnedMessage>& receiveBuffer,
                              Strand& receiveBufferStrand)
        {
            return Pointer(new Session(id, 
                                       ioContext, 
                                       std::move(socket),
                                       receiveBuffer, 
                                       receiveBufferStrand));
        }

        void Connect(const Endpoints& endpoints)
        {
            boost::asio::connect(_socket, endpoints);
        }

        void Disconnect()
        {
            auto socketLock = std::unique_lock(_socketLock);
            _socket.close();
        }

        bool IsConnected()
        {
            ErrorCode error;
            GetEndpoint(error);

            return !error;
        }

        void ReadAsync()
        {
            assert(IsConnected());

            ReadHeaderAsync();
        }

        void PushMessageToSendBuffer(const Message& message)
        {
            auto messageWriteLock = std::lock_guard(_messageWriteLock);

            _sendBuffer.push(message);
            WriteMessageAsync();
        }

        Id GetId() const
        {
            return _id;
        }

        Tcp::endpoint GetEndpoint()
        {
            Tcp::endpoint endpoint;

            {
                auto socketLock = std::shared_lock(_socketLock);
                endpoint = _socket.remote_endpoint();
            }

            return endpoint;
        }

        Tcp::endpoint GetEndpoint(ErrorCode& error)
        {
            Tcp::endpoint endpoint;

            {
                auto socketLock = std::shared_lock(_socketLock);
                endpoint = _socket.remote_endpoint(error);
            }

            return endpoint;
        }

    private:
        Session(Id id,
                boost::asio::io_context& ioContext,
                Tcp::socket&& socket,
                std::queue<OwnedMessage>& receiveBuffer,
                Strand& receiveBufferStrand)
            : _id(id)
            , _ioContext(ioContext)
            , _socket(std::move(socket))
            , _isWritingMessage(false)
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
                Disconnect();
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
                Disconnect();
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

        void WriteMessageAsync()
        {
            if (_isWritingMessage || 
                _sendBuffer.empty())
            {
                return;
            }

            _writeMessage = std::move(_sendBuffer.front());
            _sendBuffer.pop();

            WriteHeaderAsync();
            _isWritingMessage = true;
        }

        void OnWriteMessageCompleted(const ErrorCode& error)
        {
            auto messageWriteLock = std::lock_guard(_messageWriteLock);

            _isWritingMessage = false;

            if (!error)
            {
                WriteMessageAsync();
            }
            else
            {
                Disconnect();
            }
        }

        void WriteHeaderAsync()
        {
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(&_writeMessage.header, 
                                                         sizeof(MessageHeader)),
                                     [pSelf = shared_from_this()](const ErrorCode& error,
                                                                  const size_t nBytesTransferred)
                                     {
                                         pSelf->OnWriteHeaderCompleted(error, nBytesTransferred);
                                     });
        }

        void OnWriteHeaderCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(sizeof(MessageHeader) == nBytesTransferred);

                // The size of payload is bigger than 0
                if (_writeMessage.header.size > sizeof(MessageHeader))
                {
                    WritePayloadAsync();
                    return;
                }
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write header: " << error << "\n";
            }

            OnWriteMessageCompleted(error);
        }

        void WritePayloadAsync()
        {
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(_writeMessage.payload.data(),
                                                         _writeMessage.payload.size()),
                                     [pSelf = shared_from_this()](const ErrorCode& error,
                                                                  const size_t nBytesTransferred)
                                     {
                                         pSelf->OnWritePayloadCompleted(error, nBytesTransferred);
                                     });
        }

        void OnWritePayloadCompleted(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(nBytesTransferred == _writeMessage.payload.size());
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write payload: " << error << "\n";
            }

            OnWriteMessageCompleted(error);
        }

    private:
        const Id                        _id;
        IoContext&                      _ioContext;
        Tcp::socket                     _socket;
        std::shared_mutex               _socketLock;

        // Write message
        std::queue<Message>             _sendBuffer;
        Message                         _writeMessage;
        std::mutex                      _messageWriteLock;
        bool                            _isWritingMessage;

        // Read message
        std::queue<OwnedMessage>&       _receiveBuffer;
        Strand&                         _receiveBufferStrand;
        Message                         _readMessage;

    };
}
