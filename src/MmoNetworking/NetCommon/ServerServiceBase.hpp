#pragma once

#include <NetCommon/ServiceBase.hpp>

namespace NetCommon
{
    class ServerServiceBase : public ServiceBase
    {
    protected:
        using Tcp       = boost::asio::ip::tcp;
        using Acceptor  = Tcp::acceptor;

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
                std::cerr << "[SERVER] Failed to start: ";
                throw;
            }

            std::cout << "[SERVER] Started!\n";
        }

    private:
        void AcceptAsync()
        {
            SessionPointer pSession = Session::Create(AssignId(),
                                                      _ioContext,
                                                      _receiveBuffer,
                                                      _receiveBufferStrand);
            assert(pSession != nullptr);

            _acceptor.async_accept(pSession->Socket(),
                                   boost::asio::bind_executor(_sessionsStrand,
                                                              [this, pSession](const boost::system::error_code& error)
                                                              {
                                                                  OnAcceptCompleted(pSession, error);
                                                              }));
        }

        void OnAcceptCompleted(SessionPointer pSession, const boost::system::error_code& error)
        {
            assert(pSession != nullptr);

            if (!error)
            {
                std::cout << "[SERVER] New session: " << pSession->Socket().remote_endpoint() << "\n";

                if (OnSessionConnected(pSession))
                {
                    RegisterSession(pSession);
                    std::cout << "[" << pSession->GetId() << "] Session registered\n";
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
        Acceptor       _acceptor;

    };
}
