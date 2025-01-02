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
                              std::function<void(Pointer)> onSessionClosed,
                              std::queue<OwnedMessage>& receiveBuffer,
                              Strand& receiveBufferStrand)
        {
            return Pointer(new Session(id, 
                                       ioContext, 
                                       std::move(socket),
                                       onSessionClosed,
                                       receiveBuffer, 
                                       receiveBufferStrand));
        }

        void ReceiveMessageAsync()
        {
            ReadMessageAsync();
        }

        void SendMessageAsync(const Message& message)
        {
            boost::asio::post(_sendStrand,
                              [pSelf = shared_from_this(), message]()
                              {
                                  pSelf->PushMessageToSendBuffer(message);
                              });
        }

        void CloseAsync()
        {
            boost::asio::post(_socketStrand,
                              [pSelf = shared_from_this()]()
                              {
                                  pSelf->Close();
                              });
        }

        Id GetId() const
        {
            return _id;
        }

        const Tcp::endpoint& GetEndpoint() const
        {
            return _endpoint;
        }

    private:
        Session(Id id,
                boost::asio::io_context& ioContext,
                Tcp::socket&& socket,
                std::function<void(Pointer)> onSessionClosed,
                std::queue<OwnedMessage>& receiveBuffer,
                Strand& receiveBufferStrand)
            : _id(id)
            , _ioContext(ioContext)
            , _socket(std::move(socket))
            , _socketStrand(boost::asio::make_strand(ioContext))
            , _endpoint(_socket.remote_endpoint())
            , _onSessionClose(onSessionClosed)
            , _receiveBuffer(receiveBuffer)
            , _receiveStrand(receiveBufferStrand)
            , _sendStrand(boost::asio::make_strand(ioContext))
            , _isWritingMessage(false)
        {}

        void Close()
        {
            if (_socket.is_open())
            {
                _socket.close();
                _onSessionClose(shared_from_this());
            }
        }

        void ReadMessageAsync()
        {
            boost::asio::post(_socketStrand,
                              [pSelf = shared_from_this()]()
                              {
                                  pSelf->ReadHeaderAsync();
                              });
        }

        void OnReadMessageCompleted(const ErrorCode& error)
        {
            if (!error)
            {
                boost::asio::post(_receiveStrand,
                                  [pSelf = shared_from_this()]
                                  {
                                      pSelf->PushMessageToReceiveBuffer();
                                  });
            }
            else
            {
                CloseAsync();
            }
        }

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

                    boost::asio::post(_socketStrand,
                                      [pSelf = shared_from_this()]()
                                      {
                                          pSelf->ReadPayloadAsync();
                                      });

                    return;
                }
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to read header: " << error << "\n";
            }

            OnReadMessageCompleted(error);
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
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to read payload: " << error << "\n";
            }

            OnReadMessageCompleted(error);
        }

        void PushMessageToReceiveBuffer()
        {
            _receiveBuffer.push(OwnedMessage{shared_from_this(), _readMessage});

            ReadMessageAsync();
        }

        void PushToReceiveBufferAsync()
        {
            boost::asio::post(_receiveStrand,
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

        void PushMessageToSendBuffer(const Message& message)
        {
            _sendBuffer.push(message);

            WriteMessageAsync();
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

            boost::asio::post(_socketStrand,
                              [pSelf = shared_from_this()]()
                              {
                                  pSelf->WriteHeaderAsync();
                              });

            _isWritingMessage = true;
        }

        void OnWriteMessageCompleted(const ErrorCode& error)
        {
            _isWritingMessage = false;

            if (!error)
            {
                WriteMessageAsync();
            }
            else
            {
                CloseAsync();
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
                    boost::asio::post(_socketStrand,
                                      [pSelf = shared_from_this()]()
                                      {
                                          pSelf->WritePayloadAsync();
                                      });
                    
                    return;
                }
            }
            else
            {
                std::cerr << "[" << _id << "] Failed to write header: " << error << "\n";
            }

            boost::asio::post(_sendStrand,
                              [pSelf = shared_from_this(), error]
                              {
                                  pSelf->OnWriteMessageCompleted(error);
                              });            
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

            boost::asio::post(_sendStrand,
                              [pSelf = shared_from_this(), error]
                              {
                                  pSelf->OnWriteMessageCompleted(error);
                              });
        }

    private:
        const Id                        _id;
        IoContext&                      _ioContext;
        Tcp::socket                     _socket;
        Strand                          _socketStrand;
        const Tcp::endpoint             _endpoint;

        // Close
        std::function<void(Pointer)>    _onSessionClose;

        // Receive
        std::queue<OwnedMessage>&       _receiveBuffer;
        Strand&                         _receiveStrand;
        Message                         _readMessage;

        // Send
        std::queue<Message>             _sendBuffer;
        Strand                          _sendStrand;
        Message                         _writeMessage;
        bool                            _isWritingMessage;

    };
}
