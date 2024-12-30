#pragma once

#include <NetCommon/ServiceBase.hpp>

namespace NetCommon
{
    class ClientServiceBase : public ServiceBase
    {
    protected:
        using Endpoints             = boost::asio::ip::basic_resolver_results<Tcp>;

    public:
        void Start(std::string_view host, std::string_view service)
        {
            try
            {
                Tcp::resolver resolver(_ioContext);
                Endpoints endpoints = resolver.resolve(host, service);

                Connect(endpoints);
            }
            catch (const std::exception&)
            {
                std::cerr << "[CLIENT] Failed to start\n";
                throw;
            }

            std::cout << "[CLIENT] Started!\n";
        }

    private:
        void Connect(Endpoints endpoints)
        {
            try
            {
                SessionPointer pSession = Session::Create(AssignId(),
                                                          _ioContext,
                                                          Tcp::socket(_ioContext),
                                                          _receiveBuffer,
                                                          _receiveBufferStrand);
                pSession->Connect(endpoints);

                std::cout << "[CLIENT] New session: " << pSession->GetEndpoint() << "\n";

                if (OnSessionConnected(pSession))
                {
                    RegisterSessionAsync(pSession);
                }
                else
                {
                    std::cout << "[-----] Session denied\n";
                }
            }
            catch (const std::exception&)
            {
                std::cerr << "[CLIENT] Failed to connect\n";
                throw;
            }
        }

    };
}
