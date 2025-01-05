#pragma once

#include <NetCommon/Message.hpp>

namespace Server
{
    enum class MessageId : NetCommon::Message::Id
    {
        Accept = 500,
        Deny,
        Ping,
        Send,
        Broadcast,
    };
}
