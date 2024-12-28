#pragma once

#include <NetCommon/ClientServiceBase.hpp>
#include <Client/MessageId.hpp>

namespace Client
{
    class Service : public NetCommon::ClientServiceBase
    {
    protected:
        using Message       = NetCommon::Message;

    public:
        void PingServer()
        {
            Message message;
            message.header.id = static_cast<NetCommon::MessageId>(MessageId::Ping);

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            message << now;

            Send(message);
        }
    };
}
