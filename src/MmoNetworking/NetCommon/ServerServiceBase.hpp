#pragma once

#include <NetCommon/ServiceBase.hpp>

namespace NetCommon
{
    class ServerServiceBase : public ServiceBase
    {
    public:
        ServerServiceBase(uint16_t port)
            : ServiceBase()
            , _acceptor(_ioContext, Tcp::endpoint(Tcp::v4(), port))
        {}

        void Start()
        {
            try
            {
                AcceptAsync();
            }
            catch (const std::exception&)
            {
                std::cerr << "[SERVER] Failed to start\n";
                throw;
            }

            std::cout << "[SERVER] Started!\n";
        }

    private:
        void AcceptAsync()
        {
            _acceptor.async_accept(boost::asio::bind_executor(_sessionsStrand,
                                                              [this](const ErrorCode& error,
                                                                     Tcp::socket socket)
                                                              {
                                                                  OnAcceptCompleted(error, std::move(socket));
                                                              }));
        }

        void OnAcceptCompleted(const ErrorCode& error, Tcp::socket&& socket)
        {
            if (!error)
            {
                SessionPointer pSession = Session::Create(AssignId(),
                                                          _ioContext,
                                                          std::move(socket),
                                                          _receiveBuffer,
                                                          _receiveStrand);

                std::cout << "[SERVER] New session: " << pSession->GetEndpoint() << "\n";

                if (OnSessionConnected(pSession))
                {
                    RegisterSessionAsync(pSession);
                }
                else
                {
                    std::cout << "[-----] Session denied\n";
                }
            }
            else
            {
                std::cerr << "[SERVER] Failed to accept: " << error << "\n";
            }

            AcceptAsync();
        }

    protected:
        Tcp::acceptor       _acceptor;

    };
}
