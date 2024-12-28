#pragma once

#include <NetCommon/Include.hpp>

namespace NetCommon
{
    using MessageId = uint32_t;

    struct MessageHeader
    {
        MessageId id = 0;
        uint32_t size = sizeof(MessageHeader);

        friend std::ostream& operator<<(std::ostream& os, const MessageHeader& header)
        {
            os << "[id = " << header.id << " | size = " << header.size << "]";

            return os;
        }
    };

    struct Message
    {
        MessageHeader header;
        std::vector<std::byte> payload;

        size_t CalculateSize() const
        {
            return sizeof(MessageHeader) + payload.size();
        }

        friend std::ostream& operator<<(std::ostream& os, const Message& message)
        {
            os << "Header:\n" << message.header << std::endl;
            
            os << "Payload:\n";
            os << "[size = " << message.payload.size() << "]";

            return os;
        }
 
        // Push data to playload of message
        template<typename TData>
        friend Message& operator<<(Message& message, const TData& data)
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
        friend Message& operator>>(Message& message, TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            size_t offsetData = message.payload.size() - sizeof(TData);
            
            std::memcpy(&data, message.payload.data() + offsetData, sizeof(TData));
            message.payload.resize(offsetData);

            message.header.size = static_cast<uint32_t>(message.CalculateSize());

            return message;
        }
    };

    struct OwnedMessage
    {
        std::shared_ptr<class Session> pOwner = nullptr;
        Message message;

        friend std::ostream& operator<<(std::ostream& os, const OwnedMessage& ownedMessage)
        {
            os << ownedMessage.message;

            return os;
        }
    };
}
