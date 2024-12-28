#pragma once

#include <NetCommon/ServiceBase.hpp>
#include <Server/MessageId.hpp>

namespace Server
{
    class ServerServiceBase : public NetCommon::ServiceBase<MessageId>
    {
    protected:
        using Tcp           = boost::asio::ip::tcp;
        using Session       = NetCommon::Session<MessageId>;

    public:
        ServerServiceBase(uint16_t port)
            : NetCommon::ServiceBase<MessageId>::ServiceBase()
            , _acceptor(_ioContext, Tcp::endpoint(Tcp::v4(), port))
        {}

        virtual ~ServerServiceBase()
        {}

        void Start()
        {
            try
            {
                AcceptAsync();
                RunWorker();
            }
            catch (const std::exception&)
            {
                std::cerr << "[SERVER] Failed to start: ";
                throw;
            }

            std::cout << "[SERVER] Started!\n";
        }

    protected:
        virtual bool OnSessionConnected(SessionPointer pSession) = 0;

    private:
        void AcceptAsync()
        {
            SessionPointer pSession = Session::Create(Owner::Server,
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
                std::cout << "[SERVER] New client: " << pSession->Socket().remote_endpoint() << "\n";

                if (OnSessionConnected(pSession))
                {
                    pSession->OnClientApproved(_nextSessionId);
                    _sessions[_nextSessionId] = std::move(pSession);

                    std::cout << "[" << _nextSessionId << "] Client approved\n";
                    ++_nextSessionId;
                }
                else
                {
                    std::cout << "[-----] Client denied\n";
                }
            }
            else
            {
                std::cerr << "[SERVER] Failed to accept: " << error << "\n";
            }

            AcceptAsync();
        }

    protected:
        Tcp::acceptor                   _acceptor;

    };
}
