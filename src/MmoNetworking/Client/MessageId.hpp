#pragma once

#include <NetCommon/Message.hpp>

namespace Client
{
    enum class MessageId : NetCommon::Message::Id
    {
        Echo = 1000,
    };
}
