#pragma once

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
            std::lock_guard lockGuard(_socketLock);
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

        void SendAsync(const Message& message)
        {
            boost::asio::post(_sendBufferStrand,
                              [self = shared_from_this(), message]()
                              {
                                  self->OnSendStarted(message);
                              });
        }

        Id GetId() const
        {
            return _id;
        }

        Tcp::endpoint GetEndpoint()
        {
            Tcp::endpoint endpoint;

            {
                std::lock_guard lockGuard(_socketLock);
                endpoint = _socket.remote_endpoint();
            }

            return endpoint;
        }

        Tcp::endpoint GetEndpoint(ErrorCode& error)
        {
            Tcp::endpoint endpoint;

            {
                std::lock_guard lockGuard(_socketLock);
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
                Disconnect();
            }
        }

        void WritePayloadAsync()
        {
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(_sendBuffer.front().payload.data(),
                                                         _sendBuffer.front().payload.size()),
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
                assert(nBytesTransferred == _sendBuffer.front().payload.size());

                PopFromSendBufferAsync();
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write payload: " << error << "\n";
                Disconnect();
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

    private:
        const Id                        _id;
        IoContext&                      _ioContext;
        Tcp::socket                     _socket;
        std::mutex                      _socketLock;
        std::queue<Message>             _sendBuffer;
        Strand                          _sendBufferStrand;
        std::queue<OwnedMessage>&       _receiveBuffer;
        Strand&                         _receiveBufferStrand;
        Message                         _readMessage;

    };
}
