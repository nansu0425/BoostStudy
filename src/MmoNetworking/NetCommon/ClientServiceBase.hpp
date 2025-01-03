#pragma once

#include <NetCommon/ServiceBase.hpp>

namespace NetCommon
{
    class ClientServiceBase : public ServiceBase
    {
    protected:
        using Endpoints             = boost::asio::ip::basic_resolver_results<Tcp>;

    public:
        ClientServiceBase()
            : ServiceBase()
            , _socket(_ioContext)
            , _resolver(_ioContext)
        {}

        void Start(std::string_view host, std::string_view service)
        {   
            ConnectAsync(host, service);
            std::cout << "[CLIENT] Started!\n";
        }

    private:
        void ConnectAsync(std::string_view host, std::string_view service)
        {
            _resolver.async_resolve(host,
                                    service,
                                    [this](const ErrorCode& error,
                                           Endpoints endpoints)
                                    {
                                        OnResolveCompleted(error, endpoints);
                                    });
        }

        void OnResolveCompleted(const ErrorCode& error, Endpoints endpoints)
        {
            if (error)
            {
                std::cerr << "[CLIENT] Failed to resolve: " << error << "\n";
                return;
            }

            boost::asio::async_connect(_socket,
                                       endpoints,
                                       [this](const ErrorCode& error,
                                              const Tcp::endpoint& endpoint)
                                       {
                                           OnConnectCompleted(error);
                                       });
        }

        void OnConnectCompleted(const ErrorCode& error)
        {
            if (error)
            {
                std::cerr << "[CLIENT] Failed to connect: " << error << "\n";
                return;
            }

            CreateSession(std::move(_socket));
        }

    private:
        Tcp::socket         _socket;
        Tcp::resolver       _resolver;

    };
}
