#pragma once

#include <NetCommon/Include.hpp>

namespace NetCommon
{
    struct Message
    {
        using Id            = uint32_t;
        using Size          = uint32_t;
        using Payload       = std::vector<std::byte>;
        using Buffer        = std::queue<Message>;

        struct Header
        {
            Id      id = 0;
            Size    size = sizeof(Header);
        };

        Header      header;
        Payload     payload;

        size_t CalculateSize() const
        {
            return sizeof(Header) + payload.size();
        }
 
        // Push data to playload of message
        template<typename TData>
        friend Message& operator<<(Message& message, const TData& data)
        {
            static_assert(std::is_standard_layout<TData>::value, "Tdata must be standard-layout type");

            const size_t offsetData = message.payload.size();

            message.payload.resize(offsetData + sizeof(TData));
            std::memcpy(message.payload.data() + offsetData, &data, sizeof(TData));

            message.header.size = static_cast<Message::Size>(message.CalculateSize());

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

            message.header.size = static_cast<Message::Size>(message.CalculateSize());

            return message;
        }

        friend std::ostream& operator<<(std::ostream& os, const Message& message)
        {
            os << "[" << message.header.id << "] Size: " << message.header.size << "B\n";

            return os;
        }
    };

    template<typename TOwner>
    struct OwnedMessage
    {
        using OwnerPointer      = std::shared_ptr<TOwner>;
        using Buffer            = std::queue<OwnedMessage>;

        OwnerPointer    pOwner = nullptr;
        Message         message;

        friend std::ostream& operator<<(std::ostream& os, const OwnedMessage& ownedMessage)
        {
            os << ownedMessage.message;

            return os;
        }
    };
}
