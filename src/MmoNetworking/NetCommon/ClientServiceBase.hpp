#pragma once

#include <NetCommon/Session.hpp>

namespace NetCommon
{
    class ClientServiceBase
    {
    protected:
        using SessionPointer        = Session::Pointer;
        using Tcp                   = boost::asio::ip::tcp;
        using Owner                 = Session::Owner;
        using Strand                = boost::asio::strand<boost::asio::io_context::executor_type>;
        using Endpoints             = boost::asio::ip::basic_resolver_results<Tcp>;

    public:
        ClientServiceBase()
            : _receiveBufferStrand(boost::asio::make_strand(_ioContext))
        {}
        
        virtual ~ClientServiceBase()
        {
            Disconnect();
            _ioContext.stop();

            if (_worker.joinable())
            {
                _worker.join();
            }
        }

    public:
        void Connect(std::string_view host, std::string_view service)
        {
            try
            {
                Tcp::resolver resolver(_ioContext);
                Endpoints endpoints = resolver.resolve(host, service);

                _pServer = Session::Create(Owner::Client,
                                           _ioContext,
                                           _receiveBuffer,
                                           _receiveBufferStrand);
                _pServer->ConnectToServer(endpoints);

                _worker = std::thread([this]()
                                      {
                                          _ioContext.run();
                                      });
            }
            catch (const std::exception&)
            {
                std::cerr << "[CLIENT] Failed to connect\n";
                throw;
            }
        }

        void Disconnect()
        {
            _pServer->Close();
        }

        bool IsConnected()
        {
            if (_pServer != nullptr)
            {
                return _pServer->IsConnected();
            }
            else
            {
                return false;
            }
        }

        void Send(const Message& message)
        {
            if (!IsConnected())
            {
                _pServer->Close();
                return;
            }

            _pServer->SendAsync(message);
        }

    protected:
        boost::asio::io_context     _ioContext;
        std::thread                 _worker;
        SessionPointer              _pServer;

    private:
        std::queue<OwnedMessage>    _receiveBuffer;
        Strand                      _receiveBufferStrand;

    };
}
