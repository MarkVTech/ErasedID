#pragma once

#include "erased_id/id.hpp"

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace erased_id {

template<std::integral T>
    requires (!std::same_as<T, bool>)
struct IdTraits<T>
{
    static constexpr bool supported = true;

    static constexpr std::string_view typeKey() noexcept
    {
        if constexpr (std::is_signed_v<T>) {
            if constexpr (sizeof(T) == 1) return "integer:signed:8";
            if constexpr (sizeof(T) == 2) return "integer:signed:16";
            if constexpr (sizeof(T) == 4) return "integer:signed:32";
            if constexpr (sizeof(T) == 8) return "integer:signed:64";
        } else {
            if constexpr (sizeof(T) == 1) return "integer:unsigned:8";
            if constexpr (sizeof(T) == 2) return "integer:unsigned:16";
            if constexpr (sizeof(T) == 4) return "integer:unsigned:32";
            if constexpr (sizeof(T) == 8) return "integer:unsigned:64";
        }
        return "integer:unsupported-width";
    }

    static std::size_t hash(T value) noexcept { return std::hash<T>{}(value); }

    static std::strong_ordering compare(T lhs, T rhs) noexcept { return lhs <=> rhs; }

    static std::vector<std::byte> encode(T value)
    {
        char buffer[std::numeric_limits<T>::digits10 + 4]{};
        auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (error != std::errc{})
            throw std::runtime_error("could not encode integer ID");
        std::vector<std::byte> bytes;
        bytes.reserve(static_cast<std::size_t>(end - buffer));
        for (const char* p = buffer; p != end; ++p)
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(*p)));
        return bytes;
    }

    static T decode(std::span<const std::byte> bytes)
    {
        std::string text;
        text.reserve(bytes.size());
        for (std::byte b : bytes)
            text.push_back(static_cast<char>(std::to_integer<unsigned char>(b)));

        T value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
            throw std::invalid_argument("invalid integer ID payload");
        return value;
    }
};

} // namespace erased_id
