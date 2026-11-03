#pragma once

#include "erased_id/id.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace erased_id {

// A registry lets a receiver reconstruct erased IDs by their stable codec key.
// Register only the representations used by the serialized protocol/schema.
template<class Tag>
class IdRegistry
{
    using Decoder = std::function<Id<Tag>(std::span<const std::byte>)>;

public:
    template<IdRepresentation T>
    void registerType()
    {
        const std::string key(IdTraits<T>::typeKey());
        if (key.empty())
            throw std::invalid_argument("ID representation key must not be empty");
        auto [_, inserted] = decoders_.emplace(key, [](std::span<const std::byte> bytes) {
            return Id<Tag>::from(IdTraits<T>::decode(bytes));
        });
        if (!inserted)
            throw std::invalid_argument("duplicate ID representation key");
    }

    Id<Tag> decode(const SerializedId& serialized) const
    {
        if (serialized.isNull()) {
            if (!serialized.payload.empty())
                throw std::invalid_argument("null ID has a payload");
            return {};
        }

        const auto it = decoders_.find(std::string(serialized.representation));
        if (it == decoders_.end())
            throw std::invalid_argument("unregistered ID representation");
        return it->second(serialized.payload);
    }

private:
    std::unordered_map<std::string, Decoder> decoders_;
};

} // namespace erased_id
