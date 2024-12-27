#pragma once

#include <NetCommon/Message.hpp>

namespace Client
{
    enum class MessageId : NetCommon::MessageId
    {
        Accept = 1000,
        Deny,
        Ping,
        Send,
        Broadcast,
    };
}
