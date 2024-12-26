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
            ErrorCode error;
            _socket.remote_endpoint(error);

            return !error;
        }

        // Called when the client successfully connects to the server
        void OnServerConnected()
        {
            assert(_ownerType == OwnerType::Client);
        }

        // Called when a new client connection is accepted on the server
        void OnClientConnected(Id clientId)
        {
            assert(_ownerType == OwnerType::Server);
            assert(IsConnected());

            SetId(clientId);
            StartReadHeader();
        }

        void Disconnect()
        {
        }

        void Send(const Message& writeMessage)
        {
            boost::asio::post(_strand,
                              [wpSelf = this->weak_from_this(), writeMessage]()
                              {
                                  auto spSelf = wpSelf.lock();

                                  if (spSelf != nullptr)
                                  {
                                      spSelf->HandleSend(writeMessage);
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
        TcpConnection(OwnerType ownerType, 
                      boost::asio::io_context& ioContext,
                      std::queue<OwnedMessage>& receiveBuffer)
            : _ownerType(ownerType)
            , _strand(boost::asio::make_strand(ioContext))
            , _socket(ioContext)
            , _receiveBuffer(receiveBuffer)
        {}

        void StartReadHeader()
        {
            boost::asio::async_read(_socket,
                                    boost::asio::buffer(&_readMessage.header,
                                                        sizeof(MessageHeader)),
                                    boost::asio::bind_executor(_strand,
                                                               [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                                                 const size_t nBytesTransferred)
                                                               {
                                                                   auto spSelf = wpSelf.lock();

                                                                   if (spSelf != nullptr)
                                                                   {
                                                                       spSelf->HandleReadHeader(error, nBytesTransferred);
                                                                   }
                                                               }));
        }

        void HandleReadHeader(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(nBytesTransferred == sizeof(MessageHeader));
                assert(_readMessage.header.size >= sizeof(MessageHeader));

                // The size of payload is bigger than 0
                if (_readMessage.header.size > sizeof(MessageHeader))
                {
                    _readMessage.payload.resize(_readMessage.header.size - sizeof(MessageHeader));
                    StartReadPayload();
                }
                else
                {
                    AddReadMessageToReceiveBuffer();
                }
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to read header: " << error << "\n";
                _socket.close();
            }
        }

        void StartReadPayload()
        {
            boost::asio::async_read(_socket,
                                    boost::asio::buffer(_readMessage.payload.data(),
                                                        _readMessage.payload.size()),
                                    boost::asio::bind_executor(_strand,
                                                               [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                                                 const size_t nBytesTransferred)
                                                               {
                                                                   auto spSelf = wpSelf.lock();

                                                                   if (spSelf != nullptr)
                                                                   {
                                                                       spSelf->HandleReadPayload(error, nBytesTransferred);
                                                                   }
                                                               }));
        }

        void HandleReadPayload(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(_readMessage.payload.size() == nBytesTransferred);

                AddReadMessageToReceiveBuffer();
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to read payload: " << error << "\n";
                _socket.close();
            }
        }

        void HandleSend(const Message& writeMessage)
        {
            bool isWritingMessage = !_sendBuffer.empty();

            _sendBuffer.push(writeMessage);

            if (!isWritingMessage)
            {
                StartWriteHeader();
            }
        }

        void StartWriteHeader()
        {
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(&_sendBuffer.front().header, 
                                                         sizeof(MessageHeader)),
                                     boost::asio::bind_executor(_strand,
                                                                [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                                                  const size_t nBytesTransferred)
                                                                {
                                                                    auto spSelf = wpSelf.lock();

                                                                    if (spSelf != nullptr)
                                                                    {
                                                                        spSelf->HandleWriteHeader(error, nBytesTransferred);
                                                                    }
                                                                }));
        }

        void HandleWriteHeader(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(sizeof(MessageHeader) == nBytesTransferred);

                // The size of payload is bigger than 0
                if (_sendBuffer.front().header.size > sizeof(MessageHeader))
                {
                    StartWritePayload();
                }
                else
                {
                    RemoveWriteMessageFromSendBuffer();
                }
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write header: " << error << "\n";
                _socket.close();
            }
        }

        void StartWritePayload()
        {
            boost::asio::async_write(_socket,
                                     boost::asio::buffer(_sendBuffer.front().payload.data(),
                                                         _sendBuffer.front().payload.size()),
                                     boost::asio::bind_executor(_strand,
                                                                [wpSelf = this->weak_from_this()](const ErrorCode& error,
                                                                                                  const size_t nBytesTransferred)
                                                                {
                                                                    auto spSelf = wpSelf.lock();

                                                                    if (spSelf != nullptr)
                                                                    {
                                                                        HandleWritePayload(error, nBytesTransferred);
                                                                    }
                                                                }));
        }

        void HandleWritePayload(const ErrorCode& error, const size_t nBytesTransferred)
        {
            if (!error)
            {
                assert(nBytesTransferred == _sendBuffer.front().payload.size());

                RemoveWriteMessageFromSendBuffer();
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write payload: " << error << "\n";
                _socket.close();
            }
        }

        void AddReadMessageToReceiveBuffer()
        {
            switch (_ownerType)
            {
            case OwnerType::Server:
                _receiveBuffer.push(OwnedMessage{this->shared_from_this(), _readMessage});
                break;

            case OwnerType::Client:
                _receiveBuffer.push(OwnedMessage{nullptr, _readMessage});
                break;

            default:
                assert(true);
            }

            StartReadHeader();
        }

        void RemoveWriteMessageFromSendBuffer()
        {
            _sendBuffer.pop();

            if (!_sendBuffer.empty())
            {
                StartWriteHeader();
            }
        }

    protected:
        Id                              _id = -1;
        OwnerType                       _ownerType;
        Strand                          _strand;
        Tcp::socket                     _socket;
        std::queue<Message>             _sendBuffer;
        std::queue<OwnedMessage>&       _receiveBuffer;

    private:
        Message                         _readMessage;

    };
}
