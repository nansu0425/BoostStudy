#pragma once

#include <NetCommon/ServiceBase.hpp>

namespace NetCommon
{
    class ServerServiceBase : public ServiceBase
    {
    public:
        ServerServiceBase(size_t nWorkers, 
                          size_t nMaxReceivedMessages,
                          uint16_t port)
            : ServiceBase(nWorkers, nMaxReceivedMessages)
            , _acceptor(_workers, Tcp::endpoint(Tcp::v4(), port))
        {}

        void Start()
        {
            AcceptAsync();
            std::cout << "[SERVER] Started!\n";
        }

    private:
        void AcceptAsync()
        {
            _acceptor.async_accept([this](const ErrorCode& error,
                                          Tcp::socket socket)
                                   {
                                       OnAcceptCompleted(error, std::move(socket));
                                   });
        }

        void OnAcceptCompleted(const ErrorCode& error, Tcp::socket&& socket)
        {
            if (error)
            {
                std::cerr << "[SERVER] Failed to accept: " << error << "\n";
                return;
            }

            CreateSession(std::move(socket));
            AcceptAsync();
        }

    protected:
        Tcp::acceptor       _acceptor;

    };
}
