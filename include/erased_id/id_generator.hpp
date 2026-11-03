#pragma once

#include "erased_id/integer_traits.hpp"

#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace erased_id {

template<class Tag>
class IdGenerator
{
public:
    virtual ~IdGenerator() = default;
    virtual Id<Tag> generate() = 0;
};

template<class Tag, std::unsigned_integral Integer = std::uint64_t>
    requires (!std::same_as<Integer, bool>)
class MonotonicIntegerIdGenerator final : public IdGenerator<Tag>
{
public:
    explicit MonotonicIntegerIdGenerator(Integer first = 1) : next_(first) {}

    Id<Tag> generate() override
    {
        if (exhausted_)
            throw std::overflow_error("monotonic ID generator exhausted");

        const Integer current = next_;
        if (next_ == std::numeric_limits<Integer>::max())
            exhausted_ = true;
        else
            ++next_;

        return Id<Tag>::from(current);
    }

private:
    Integer next_;
    bool exhausted_ = false;
};

// Adapts a callable supplied by the application or a UUID library.
template<class Tag, class Generator>
class CallableIdGenerator final : public IdGenerator<Tag>
{
public:
    explicit CallableIdGenerator(Generator generator) : generator_(std::move(generator)) {}

    Id<Tag> generate() override
    {
        return Id<Tag>::from(std::invoke(generator_));
    }

private:
    Generator generator_;
};

template<class Tag, class Generator>
CallableIdGenerator(Generator) -> CallableIdGenerator<Tag, Generator>;

} // namespace erased_id
