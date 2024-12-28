#pragma once

#include <NetCommon/Message.hpp>

namespace Client
{
    enum class MessageId : NetCommon::MessageId
    {
        Ping = 1000,
    };
}
