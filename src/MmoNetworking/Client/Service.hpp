#pragma once

#include <NetCommon/ClientBase.hpp>
#include <Client/MessageId.hpp>

namespace Client
{
    class Service : public NetCommon::ClientBase<MessageId>
    {};
}
