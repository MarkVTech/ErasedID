#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

namespace erased_id {

struct SerializedId
{
    std::string representation;
    std::vector<std::byte> payload;

    bool isNull() const noexcept { return representation.empty(); }
};

/**
 * @brief The default IdTraits class
 *
 * The primary IdTraits template marks types unsupported by default.
 * Applications or adapters opt types in by specializing
 * IdTraits and setting supported true.
 */
template<class T, class Enable = void>
struct IdTraits
{
    static constexpr bool supported = false;
};

/**
 * @brief Describes the requirements for an ID representation type.
 *
 * A type T satisfies this concept if \c IdTraits<T> opts it in by
 * setting \c supported to true and provides the required operations
 * with the specified result types.
 */
template<class T>
concept IdRepresentation = IdTraits<T>::supported &&
    requires(const T& lhs, const T& rhs, std::span<const std::byte> bytes) {
        { IdTraits<T>::typeKey() } -> std::same_as<std::string_view>;
        { IdTraits<T>::hash(lhs) } -> std::convertible_to<std::size_t>;
        { IdTraits<T>::compare(lhs, rhs) } -> std::same_as<std::strong_ordering>;
        { IdTraits<T>::encode(lhs) } -> std::same_as<std::vector<std::byte>>;
        { IdTraits<T>::decode(bytes) } -> std::same_as<T>;
    };

/**
 * @brief Type-erased identifier for a value with supported representation.
 *
 * The concrete representation type must satisfy the \c IdRepresentation
 * concept. The \c Tag parameter distinguishes ID domains.
 *
 * @tparam Tag Type tag identifying the ID domain.
 */
template<class Tag>
class Id
{
    /**
    * @brief Type-erased interface for holding an ID representation.
    *
    * Implementations expose the stored representation's type information
    * and operations without requiring callers to know its concrete type.
    */
    struct HolderInterface
    {
        virtual ~HolderInterface() = default;


        /**
        * @brief Returns the concrete C++ type of the stored representation.
        */
        virtual std::type_index cppType() const noexcept = 0;

        /**
        * @brief Returns the registered type key for the representation.
        */
        virtual std::string_view typeKey() const noexcept = 0;

        /**
        * @brief Returns the hash of the stored value.
        */
        virtual std::size_t valueHash() const = 0;


        /**
        * @brief Compares the stored value with another holder's value.
        * @pre The other holder contains the same concrete representation type.
        */
        virtual std::strong_ordering compareSameType(const HolderInterface&) const = 0;

        /**
        * @brief Encodes the stored value as bytes.
        */
        virtual std::vector<std::byte> encode() const = 0;

        /**
        * @brief Returns the address of the stored value if its type matches.
        * @param type The requested concrete C++ type.
        * @return A pointer to the stored value, or nullptr if the types differ.
        */
        virtual const void* addressOf(std::type_index) const noexcept = 0;
    };

    /**
    * @brief Stores a concrete ID representation behind HolderInterface.
    *
    * Implements the type-erased operations using IdTraits<T>.
    *
    * @tparam T Concrete ID representation satisfying IdRepresentation.
    */
    template<IdRepresentation T>
    struct Holder final : HolderInterface
    {
        explicit Holder(T v) : value(std::move(v)) {}

        std::type_index cppType() const noexcept override { return typeid(T); }
        std::string_view typeKey() const noexcept override { return IdTraits<T>::typeKey(); }
        std::size_t valueHash() const override { return IdTraits<T>::hash(value); }

        std::strong_ordering compareSameType(const HolderInterface& other) const override
        {
            const auto* rhs = dynamic_cast<const Holder*>(&other);
            // Id::operator<=> compares C++ representation types before calling this.
            return IdTraits<T>::compare(value, rhs->value);
        }

        std::vector<std::byte> encode() const override { return IdTraits<T>::encode(value); }

        const void* addressOf(std::type_index requested) const noexcept override
        {
            return requested == typeid(T) ? &value : nullptr;
        }

        T value;
    };

public:
    Id() = default;

    /**
    * @brief Creates an Id from a supported representation value.
    *
    * @tparam T Concrete ID representation type.
    * @param value Representation value to store.
    * @return An Id containing the value.
    * @throws std::invalid_argument if the representation's type key is empty.
    */
    template<IdRepresentation T>
    static Id from(T value)
    {
        if (IdTraits<T>::typeKey().empty())
            throw std::invalid_argument("ID representation key must not be empty");
        return Id(std::make_shared<Holder<T>>(std::move(value)));
    }

    bool isNull() const noexcept { return !value_; }
    explicit operator bool() const noexcept { return !isNull(); }

    /**
    * @brief Returns a pointer to the stored representation if its type is T.
    *
    * @tparam T The requested representation type.
    * @return A pointer to the stored value, or nullptr if the Id is empty
    *         or its representation type does not match T.
    */
    template<class T>
    const T* getIf() const noexcept
    {
        if (!value_)
            return nullptr;
        return static_cast<const T*>(value_->addressOf(typeid(T)));
    }

    std::size_t hash() const
    {
        if (!value_)
            return nullHash();

        // Include concrete representation identity so different backends do not
        // accidentally share a semantic hash domain. Collisions remain allowed.
        const std::size_t typeHash = value_->cppType().hash_code();
        const std::size_t valueHash = value_->valueHash();
        return combine(typeHash, valueHash);
    }

    SerializedId serialize() const
    {
        if (!value_)
            return {};
        return {std::string(value_->typeKey()), value_->encode()};
    }

    /**
    * @brief Deserializes an ID using the specified representation type.
    *
    * A serialized null ID produces an empty Id. A non-null ID is decoded
    * only if its representation key matches the key for T.
    *
    * @tparam T The ID representation type.
    * @param serialized The serialized ID data.
    * @return The deserialized Id.
    * @throws std::invalid_argument If a null ID has a payload or the
    *         representation key does not match T.
    */
    template<class T>
        requires IdRepresentation<T>
    static Id deserializeAs(const SerializedId& serialized)
    {
        if (serialized.isNull()) {
            if (!serialized.payload.empty())
                throw std::invalid_argument("null ID has a payload");
            return {};
        }
        if (serialized.representation != IdTraits<T>::typeKey())
            throw std::invalid_argument("serialized ID has a different representation");
        return from(IdTraits<T>::decode(serialized.payload));
    }

    friend bool operator==(const Id& lhs, const Id& rhs)
    {
        return (lhs <=> rhs) == std::strong_ordering::equal;
    }

    /**
    * @brief Compares two IDs using a strong ordering.
    *
    * Two empty IDs compare equal. An empty ID sorts before a non-empty ID.
    * IDs with different representation types are ordered by C++ type; IDs
    * with the same representation type are ordered by their stored values.
    *
    * @param lhs The left-hand ID.
    * @param rhs The right-hand ID.
    * @return The ordering of the IDs.
    */
    friend std::strong_ordering operator<=>(const Id& lhs, const Id& rhs)
    {
        if (!lhs.value_ || !rhs.value_) {
            if (!lhs.value_ && !rhs.value_)
                return std::strong_ordering::equal;
            return lhs.value_ ? std::strong_ordering::greater : std::strong_ordering::less;
        }

        const auto lhsType = lhs.value_->cppType();
        const auto rhsType = rhs.value_->cppType();
        if (lhsType != rhsType)
            return lhsType < rhsType ? std::strong_ordering::less : std::strong_ordering::greater;

        return lhs.value_->compareSameType(*rhs.value_);
    }

private:
    explicit Id(std::shared_ptr<const HolderInterface> value) : value_(std::move(value)) {}

    /**
    * @brief Returns the hash value used for an empty Id.
    *
    * @return The hash value assigned to an empty Id.
    */
    static constexpr std::size_t nullHash() noexcept { return 0x4e554c4cU; }

    /**
    * @brief Combines two hash values into one.
    *
    * Intended for in-process hash containers. Do not persist the result or use
    * it as an identifier.
    *
    * @param first The initial hash value.
    * @param second The hash value to combine with it.
    * @return The combined hash value.
    */
    static std::size_t combine(std::size_t first, std::size_t second) noexcept
    {
        // A common hash-combine pattern; this hash is for in-process containers,
        // not a stable serialized identifier.
        return first ^ (second + static_cast<std::size_t>(0x9e3779b9U) +
                        (first << 6U) + (first >> 2U));
    }

    // Copies of Id share the immutable holder, making copying inexpensive.
    std::shared_ptr<const HolderInterface> value_;
};

/**
 * @brief Hash functor for Id<Tag>.
 *
 * Allows Id<Tag> values to be used as keys in unordered associative
 * containers. The hash is intended for in-process lookup, not as a
 * persistent or serialized identifier.
 *
 * @tparam Tag The ID domain tag.
 */
template<class Tag>
struct IdHash
{
    std::size_t operator()(const Id<Tag>& id) const { return id.hash(); }
};

} // namespace erased_id

namespace std {

/**
 * @brief Provides hashing support for erased_id::Id<Tag>.
 *
 * Allows Id<Tag> values to be used as keys in unordered associative
 * containers. The hash is intended for in-process container lookup.
 *
 * @tparam Tag The ID domain tag.
 */
template<class Tag>
struct hash<erased_id::Id<Tag>>
{
    std::size_t operator()(const erased_id::Id<Tag>& id) const { return id.hash(); }
};

} // namespace std
