#pragma once

#include <NetCommon/Message.hpp>

namespace Client
{
    enum class MessageId : NetCommon::Message::Id
    {
        Ping = 1000,
    };
}
