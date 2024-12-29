#pragma once

#include <NetCommon/ServiceBase.hpp>

namespace NetCommon
{
    class ClientServiceBase : public ServiceBase
    {
    protected:
        using Tcp                   = boost::asio::ip::tcp;
        using Owner                 = Session::Owner;
        using Endpoints             = boost::asio::ip::basic_resolver_results<Tcp>;

    public:
        ClientServiceBase()
            : ServiceBase()
        {
            RunWorker();
        }
        
        virtual ~ClientServiceBase()
        {
            Disconnect();
        }

    public:
        void Connect(std::string_view host, std::string_view service)
        {
            try
            {
                Tcp::resolver resolver(_ioContext);
                Endpoints endpoints = resolver.resolve(host, service);

                _pSession = Session::Create(Owner::Client,
                                            _ioContext,
                                            _receiveBuffer,
                                            _receiveBufferStrand);
                _pSession->Connect(endpoints);

                OnSessionConnected();
            }
            catch (const std::exception&)
            {
                std::cerr << "[CLIENT] Failed to connect\n";
                throw;
            }
        }

        void Disconnect()
        {
            _pSession->Disconnect();
            OnSessionDisconnected(_pSession);
        }

        bool IsConnected() const
        {
            return (_pSession) 
                   ? _pSession->IsConnected() 
                   : false;
        }

        void SendAsync(const Message& message)
        {
            if (!IsConnected())
            {
                return;
            }

            _pSession->SendAsync(message);
        }

    protected:
        virtual void OnSessionConnected() = 0;

    protected:
        SessionPointer      _pSession;

    };
}
