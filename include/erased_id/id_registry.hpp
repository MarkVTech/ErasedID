#pragma once

#include "erased_id/id.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace erased_id {

/**
 * @brief Registry of decoders for type-erased IDs.
 *
 * Maps stable representation keys to decoders that reconstruct IDs with
 * this registry's tag. Register only the representations allowed by the
 * serialized protocol or schema.
 *
 * @tparam Tag The ID domain tag.
 */
template<class Tag>
class IdRegistry
{
    using Decoder = std::function<Id<Tag>(std::span<const std::byte>)>;

public:
    /**
     * @brief Registers a decoder for representation type T.
     *
     * The representation's type key must be non-empty and unique in this
     * registry.
     *
     * @tparam T A supported ID representation type.
     * @throws std::invalid_argument If the type key is empty or already
     *         registered.
     */
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

    /**
     * @brief Decodes a serialized ID using its registered representation.
     *
     * A serialized null ID produces an empty Id.
     *
     * @param serialized The serialized ID data.
     * @return The decoded ID, or an empty Id if the serialized ID is null.
     * @throws std::invalid_argument If a null ID has a payload or the
     *         representation key is not registered.
     */
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
