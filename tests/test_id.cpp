#include "erased_id/id_generator.hpp"
#include "erased_id/id_registry.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct NodeTag {};
struct ConnectionTag {};

static_assert(erased_id::IdRepresentation<std::uint64_t>);
static_assert(!erased_id::IdRepresentation<std::string>);

// Small UUID-shaped test representation. The library itself has no UUID dependency.
struct TestUuid
{
    std::array<std::uint8_t, 16> bytes{};
    friend bool operator==(const TestUuid&, const TestUuid&) = default;
};

namespace erased_id {
template<>
struct IdTraits<TestUuid>
{
    static constexpr bool supported = true;
    static constexpr std::string_view typeKey() noexcept { return "test.uuid.v1"; }

    static std::size_t hash(const TestUuid& value) noexcept
    {
        std::size_t result = 1469598103934665603ULL;
        for (auto byte : value.bytes) {
            result ^= byte;
            result *= 1099511628211ULL;
        }
        return result;
    }

    static std::strong_ordering compare(const TestUuid& lhs, const TestUuid& rhs) noexcept
    {
        return lhs.bytes <=> rhs.bytes;
    }

    static std::vector<std::byte> encode(const TestUuid& value)
    {
        std::vector<std::byte> result;
        result.reserve(value.bytes.size());
        for (auto byte : value.bytes)
            result.push_back(static_cast<std::byte>(byte));
        return result;
    }

    static TestUuid decode(std::span<const std::byte> bytes)
    {
        if (bytes.size() != 16)
            throw std::invalid_argument("UUID payload must be exactly 16 bytes");

        TestUuid result;
        for (std::size_t i = 0; i < result.bytes.size(); ++i)
            result.bytes[i] = std::to_integer<std::uint8_t>(bytes[i]);
        return result;
    }
};
} // namespace erased_id

int main()
{
    using erased_id::Id;
    using NodeId = Id<NodeTag>;
    using ConnectionId = Id<ConnectionTag>;
    static_assert(std::totally_ordered<NodeId>);

    const NodeId nullId;
    assert(nullId.isNull());
    assert(nullId == NodeId{});

    const NodeId integerA = NodeId::from(std::uint64_t{42});
    const NodeId integerB = NodeId::from(std::uint64_t{42});
    const NodeId integerC = NodeId::from(std::uint64_t{43});
    assert(!integerA.isNull());
    assert(integerA == integerB);
    assert(integerA != integerC);
    assert(integerA < integerC);
    assert(integerC > integerA);
    assert(integerA.hash() == integerB.hash());
    assert(integerA.getIf<std::uint64_t>() != nullptr);
    assert(*integerA.getIf<std::uint64_t>() == 42);
    assert(integerA.getIf<std::uint32_t>() == nullptr);

    const TestUuid uuid{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
    const NodeId uuidId = NodeId::from(uuid);
    assert(uuidId.getIf<TestUuid>() != nullptr);
    assert(*uuidId.getIf<TestUuid>() == uuid);
    assert(uuidId != integerA); // Cross-representation values are unequal.

    auto uuidLibraryGenerator = [uuid] { return uuid; };
    erased_id::CallableIdGenerator<NodeTag, decltype(uuidLibraryGenerator)> uuidGenerator{
        uuidLibraryGenerator};
    assert(uuidGenerator.generate() == uuidId);

    const ConnectionId connectionId = ConnectionId::from(std::uint64_t{42});
    assert(connectionId); // A different tag gives a different compile-time ID domain.

    const NodeId copied = uuidId;
    assert(copied == uuidId); // Copies name the same logical ID value.

    erased_id::IdRegistry<NodeTag> registry;
    registry.registerType<std::uint64_t>();
    registry.registerType<TestUuid>();

    const NodeId integerRoundTrip = registry.decode(integerA.serialize());
    const NodeId uuidRoundTrip = registry.decode(uuidId.serialize());
    assert(integerRoundTrip == integerA);
    assert(uuidRoundTrip == uuidId);
    assert(registry.decode(nullId.serialize()).isNull());

    erased_id::MonotonicIntegerIdGenerator<NodeTag, std::uint8_t> generator{254};
    assert(*generator.generate().getIf<std::uint8_t>() == 254);
    assert(*generator.generate().getIf<std::uint8_t>() == 255);
    bool exhausted = false;
    try {
        (void)generator.generate();
    } catch (const std::overflow_error&) {
        exhausted = true;
    }
    assert(exhausted);

    auto values = std::unordered_set<NodeId>{integerA, integerB, integerC, uuidId};
    assert(values.size() == 3);

    std::map<NodeId, int> orderedValues;
    orderedValues.emplace(integerA, 42);
    orderedValues.emplace(uuidId, 16);
    assert(orderedValues.size() == 2);

    std::cout << "erased_id tests passed\n";
}
