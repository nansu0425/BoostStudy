#pragma once

#include <NetCommon/Message.hpp>

namespace Server
{
    enum class MessageId : NetCommon::MessageId
    {
        Accept = 500,
        Deny,
        Ping,
        Send,
        Broadcast,
    };
}
