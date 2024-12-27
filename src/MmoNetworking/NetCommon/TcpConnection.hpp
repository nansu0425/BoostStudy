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
        using Tcp           = boost::asio::ip::tcp;
        using Strand        = boost::asio::strand<boost::asio::io_context::executor_type>;
        using Message       = Message<TMessageId>;
        using MessageHeader = MessageHeader<TMessageId>;
        using OwnedMessage  = OwnedMessage<TMessageId>;
        using ErrorCode     = boost::system::error_code;

    public:
        using Pointer       = std::shared_ptr<TcpConnection>;
        using Id            = uint32_t;

        enum class Owner
        {
            Server,
            Client,
        };

    public:
        static Pointer Create(Owner owner,
                              boost::asio::io_context& ioContext,
                              std::queue<OwnedMessage>& receiveBuffer,
                              Strand& receiveBufferStrand)
        {
            return Pointer(new TcpConnection(owner, ioContext, receiveBuffer, receiveBufferStrand));
        }

        bool IsConnected() const
        {
            ErrorCode error;
            _socket.remote_endpoint(error);

            return !error;
        }

        void OnServerConnected()
        {
            assert(_owner == Owner::Client);
        }

        void OnClientApproved(Id clientId)
        {
            assert(_owner == Owner::Server);
            assert(IsConnected());

            SetId(clientId);
            ReadHeaderAsync();
        }

        void Disconnect()
        {
            if (IsConnected())
            {
                _socket.close();
            }
        }

        void SendAsync(const Message& message)
        {
            boost::asio::post(_sendBufferStrand,
                              [wpSelf = this->weak_from_this(), message]()
                              {
                                  auto spSelf = wpSelf.lock();

                                  if (spSelf != nullptr)
                                  {
                                      spSelf->OnSendStarted(message);
                                  }
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

        void SetId(Id id)
        {
            _id = id;
        }

    private:
        TcpConnection(Owner owner, 
                      boost::asio::io_context& ioContext,
                      std::queue<OwnedMessage>& receiveBuffer,
                      Strand& receiveBufferStrand)
            : _owner(owner)
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
                                    [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                      const size_t nBytesTransferred)
                                    {
                                        auto spSelf = wpSelf.lock();

                                        if (spSelf != nullptr)
                                        {
                                            spSelf->OnReadHeaderCompleted(error, nBytesTransferred);
                                        }
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
                                    [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                      const size_t nBytesTransferred)
                                    {
                                        auto spSelf = wpSelf.lock();

                                        if (spSelf != nullptr)
                                        {
                                            spSelf->OnReadPayloadCompleted(error, nBytesTransferred);
                                        }
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
                              [wpSelf = this->weak_from_this()]()
                              {
                                  auto spSelf = wpSelf.lock();

                                  if (spSelf != nullptr)
                                  {
                                      spSelf->OnPushToReceiveBufferStarted();
                                  }
                              });
        }

        void OnPushToReceiveBufferStarted()
        {
            switch (_owner)
            {
            case Owner::Server:
                _receiveBuffer.push(OwnedMessage{this->shared_from_this(), _readMessage});
                break;

            case Owner::Client:
                _receiveBuffer.push(OwnedMessage{nullptr, _readMessage});
                break;

            default:
                assert(true);
            }

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
                                                                [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                                                  const size_t nBytesTransferred)
                                                                {
                                                                    auto spSelf = wpSelf.lock();

                                                                    if (spSelf != nullptr)
                                                                    {
                                                                        spSelf->OnWriteHeaderCompleted(error, nBytesTransferred);
                                                                    }
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
                                                                [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                                                  const size_t nBytesTransferred)
                                                                {
                                                                    auto spSelf = wpSelf.lock();

                                                                    if (spSelf != nullptr)
                                                                    {
                                                                        spSelf->OnWritePayloadCompleted(error, nBytesTransferred);
                                                                    }
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
                              [wpSelf = this->weak_from_this()]()
                              {
                                  auto spSelf = wpSelf.lock();

                                  if (spSelf != nullptr)
                                  {
                                      spSelf->OnPopFromSendBufferStarted();
                                  }
                              });
        }

        void OnPopFromSendBufferStarted()
        {
            _sendBuffer.pop();

            if (!_sendBuffer.empty())
            {
                WriteHeaderAsync();
            }
        }

    protected:
        Id                              _id = -1;
        Owner                           _owner;
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
