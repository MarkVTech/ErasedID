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

// The primary template rejects types until an application or adapter opts them in.
template<class T, class Enable = void>
struct IdTraits
{
    static constexpr bool supported = false;
};

template<class T>
concept IdRepresentation = IdTraits<T>::supported &&
    requires(const T& lhs, const T& rhs, std::span<const std::byte> bytes) {
        { IdTraits<T>::typeKey() } -> std::same_as<std::string_view>;
        { IdTraits<T>::hash(lhs) } -> std::convertible_to<std::size_t>;
        { IdTraits<T>::compare(lhs, rhs) } -> std::same_as<std::strong_ordering>;
        { IdTraits<T>::encode(lhs) } -> std::same_as<std::vector<std::byte>>;
        { IdTraits<T>::decode(bytes) } -> std::same_as<T>;
    };

template<class Tag>
class Id
{
    struct Concept
    {
        virtual ~Concept() = default;
        virtual std::type_index cppType() const noexcept = 0;
        virtual std::string_view typeKey() const noexcept = 0;
        virtual std::size_t valueHash() const = 0;
        virtual std::strong_ordering compareSameType(const Concept&) const = 0;
        virtual std::vector<std::byte> encode() const = 0;
        virtual const void* addressOf(std::type_index) const noexcept = 0;
    };

    template<IdRepresentation T>
    struct Holder final : Concept
    {
        explicit Holder(T v) : value(std::move(v)) {}

        std::type_index cppType() const noexcept override { return typeid(T); }
        std::string_view typeKey() const noexcept override { return IdTraits<T>::typeKey(); }
        std::size_t valueHash() const override { return IdTraits<T>::hash(value); }

        std::strong_ordering compareSameType(const Concept& other) const override
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

    template<IdRepresentation T>
    static Id from(T value)
    {
        if (IdTraits<T>::typeKey().empty())
            throw std::invalid_argument("ID representation key must not be empty");
        return Id(std::make_shared<Holder<T>>(std::move(value)));
    }

    bool isNull() const noexcept { return !value_; }
    explicit operator bool() const noexcept { return !isNull(); }

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
    explicit Id(std::shared_ptr<const Concept> value) : value_(std::move(value)) {}

    static constexpr std::size_t nullHash() noexcept { return 0x4e554c4cU; }

    static std::size_t combine(std::size_t first, std::size_t second) noexcept
    {
        // A common hash-combine pattern; this hash is for in-process containers,
        // not a stable serialized identifier.
        return first ^ (second + static_cast<std::size_t>(0x9e3779b9U) +
                        (first << 6U) + (first >> 2U));
    }

    std::shared_ptr<const Concept> value_;
};

template<class Tag>
struct IdHash
{
    std::size_t operator()(const Id<Tag>& id) const { return id.hash(); }
};

} // namespace erased_id

namespace std {
template<class Tag>
struct hash<erased_id::Id<Tag>>
{
    std::size_t operator()(const erased_id::Id<Tag>& id) const { return id.hash(); }
};
} // namespace std
