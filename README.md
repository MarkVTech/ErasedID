# erased_id

A small, header-only C++20 library for tagged, type-erased identifier values.

The goal is to let a model expose one ID type while the application selects a
concrete representation, such as a monotonic integer or a UUID type from a
library it already uses. The core has no Qt or UUID-library dependency.

## What is included

- `Id<Tag>` owns one immutable concrete ID value behind a type-erased interface.
- A default-constructed `Id<Tag>` is null; integer zero remains an ordinary value.
- Copies share the immutable representation. Moves are supported.
- Equality requires the same concrete C++ representation and equal values.
- Ordering puts null before non-null IDs; different representations are ordered
  by their runtime type token, and same-type values use their `IdTraits` order.
- Hashing includes concrete representation identity and the value hash.
- `getIf<T>()` provides checked extraction without exposing Qt or Boost in the
  model API.
- `SerializedId` carries a stable representation key and codec payload.
- `IdRegistry<Tag>` reconstructs IDs from registered representation codecs.
- `MonotonicIntegerIdGenerator` and `CallableIdGenerator` return erased IDs.

## Quick example

```cpp
#include <cstdint>
#include <erased_id/integer_traits.hpp>

struct NodeTag {};
using NodeId = erased_id::Id<NodeTag>;

NodeId id = NodeId::from(std::uint64_t{17});
if (const auto* value = id.getIf<std::uint64_t>()) {
    // use *value
}
```

The integral `IdTraits` specialization is provided by
`<erased_id/integer_traits.hpp>`.

## Adapting a UUID library

Specialize `erased_id::IdTraits<YourUuidType>` in the application or in a small
optional adapter header. The specialization provides a stable `typeKey()`,
`hash()`, `compare()`, `encode()`, and `decode()`. `CallableIdGenerator` can
wrap the generator supplied by Boost.UUID, stduuid, Qt, or another UUID library;
this project does not implement UUID generation.

The representation key is part of the persistence or wire format. Keep it
stable and version it when the encoding changes. Register each key once in the
receiver's `IdRegistry`. In-memory ordering uses `std::type_index` and is not a
portable serialization order. The in-memory hash is likewise unsuitable as a
persistent key. The built-in integer keys describe signedness
and width; if an application uses two distinct integer types with the same
signedness and width in one registry, give them distinct custom keys or settle
on one fixed-width type. `Tag` is a compile-time domain, so an enclosing
protocol/schema should distinguish node IDs from connection IDs when needed.

## Build and run the unit tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests use a small UUID-shaped value type to verify the adapter contract
without adding a UUID dependency to the library.
