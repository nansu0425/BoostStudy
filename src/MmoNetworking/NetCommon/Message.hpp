#pragma once

#include <NetCommon/Pch.hpp>

namespace NetCommon
{
    template<typename TMessageId>
    struct MessageHeader
    {
        TMessageId id;
        uint32_t size = 0;

        friend std::ostream& operator<<(std::ostream& os, const MessageHeader<TMessageId>& header)
        {
            os << "[id = " << static_cast<int>(header.id) << " | size = " << header.size << "]";

            return os;
        }
    };

    template<typename TMessageId>
    struct Message
    {
        MessageHeader<TMessageId> header;
        std::vector<std::byte> payload;

        size_t GetSize() const
        {
            return sizeof(MessageHeader<TMessageId>) + payload.size();
        }

        friend std::ostream& operator<<(std::ostream& os, const Message<TMessageId>& message)
        {
            os << message.header;

            return os;
        }
 
        // Push data to playload of message
        template<typename TData>
        friend Message<TMessageId>& operator<<(Message<TMessageId>& message, const TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            size_t offsetData = message.GetSize();

            message.payload.resize(message.payload.size() + sizeof(TData));
            std::memcpy(message.payload.data() + offsetData, &data, sizeof(TData));

            return message;
        }

        // Pop data from playload of message
        template<typename TData>
        friend Message<TMessageId>& operator>>(Message<TMessageId>& message, TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            size_t offsetData = message.GetSize() - sizeof(TData);
            
            std::memcpy(&data, message.payload.data() + offsetData, sizeof(Tdata));
            message.payload.resize(offsetData);

            return message;
        }
    };
}
