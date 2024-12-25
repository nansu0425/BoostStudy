#pragma once

#include <NetCommon/Include.hpp>

namespace NetCommon
{
    using MessageId = uint32_t;

    template<typename TMessageId>
    struct MessageHeader
    {
        TMessageId id = TMessageId();
        uint32_t size = sizeof(MessageHeader);

        friend std::ostream& operator<<(std::ostream& os, const MessageHeader<TMessageId>& header)
        {
            os << "[id = " << static_cast<MessageId>(header.id) << " | size = " << header.size << "]";

            return os;
        }
    };

    template<typename TMessageId>
    struct Message
    {
        MessageHeader<TMessageId> header;
        std::vector<std::byte> payload;

        size_t CalculateSize() const
        {
            return sizeof(MessageHeader<TMessageId>) + payload.size();
        }

        friend std::ostream& operator<<(std::ostream& os, const Message<TMessageId>& message)
        {
            os << "Header:\n" << message.header << std::endl;
            
            os << "Payload:\n";
            os << "[size = " << message.payload.size() << "]";

            return os;
        }
 
        // Push data to playload of message
        template<typename TData>
        friend Message<TMessageId>& operator<<(Message<TMessageId>& message, const TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            size_t offsetData = message.payload.size();

            message.payload.resize(message.payload.size() + sizeof(TData));
            std::memcpy(message.payload.data() + offsetData, &data, sizeof(TData));

            message.header.size = static_cast<uint32_t>(message.CalculateSize());

            return message;
        }

        // Pop data from playload of message
        template<typename TData>
        friend Message<TMessageId>& operator>>(Message<TMessageId>& message, TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            size_t offsetData = message.payload.size() - sizeof(TData);
            
            std::memcpy(&data, message.payload.data() + offsetData, sizeof(TData));
            message.payload.resize(offsetData);

            message.header.size = static_cast<uint32_t>(message.CalculateSize());

            return message;
        }
    };

    template<typename TMessageId>
    class TcpConnection;

    template<typename TMessageId>
    struct OwnedMessage
    {
        std::shared_ptr<TcpConnection<TMessageId>> pOwner = nullptr;
        Message<TMessageId> message;

        friend std::ostream& operator<<(std::ostream& os, const OwnedMessage<TMessageId>& ownedMessage)
        {
            os << ownedMessage.message;

            return os;
        }
    };
}
