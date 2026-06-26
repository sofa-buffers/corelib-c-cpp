# SofaBuffers — pure C++20 implementation

A second, independent C++ implementation of the SofaBuffers core library, written
from scratch in **C++20 with no C backend**. It exposes the same public API as the
C-wrapping [`src/include/sofab/sofab.hpp`](../src/include/sofab/sofab.hpp)
(`sofab::OStream` / `sofab::OStreamInline` / `sofab::IStreamObject` / …) so it is a
drop-in alternative — put `cpp20/include` on the include path instead of
`src/include` and the `#include "sofab/sofab.hpp"` is unchanged.

> Status: working first cut. All eight wire types encode/decode and round-trip;
> validated against known byte sequences from the shared conformance vectors
> (`assets/test_vectors.json`). Full vector-suite validation is a planned
> follow-up.

## Why a second implementation

The C++ wrapper is thin and inherits the C library's byte-at-a-time decoder. This
implementation explores what a native, modern-C++ core looks like — and, in
particular, a decoder tuned for the **common case where the whole message is
already in contiguous memory**.

## Design

- **Encoding stays streamable.** `OStream` writes into a caller-owned buffer and
  invokes a flush callback when it fills, so a message can exceed RAM — identical
  semantics to the C encoder (start offset, mid-stream `setBuffer`, explicit
  `flush`).
- **Decoding is a protobuf-style cursor.** Instead of a per-byte state machine,
  the decoder advances a pointer over the buffer: read a varint header, branch on
  the 3-bit type, and either hand the field to the user (`read(...)`) or skip its
  payload. Nested sequences recurse; unread fields and whole sub-sequences are
  skipped automatically.
- **Streaming is still supported.** `feed()` accumulates bytes and dispatches each
  **complete top-level field** as it becomes available (a field that is only
  partially present is left for the next `feed`). The common path — one
  `feed(whole_message)` — parses everything in a single pass.
- **Modern techniques:** `std::span`, `std::bit_cast` for float punning, concepts
  for the message/array dispatch, `if constexpr` type deduction in `write()` /
  `read()`, `[[nodiscard]]`. The wire is little-endian and handled explicitly, so
  there is no host-endian branching.

## Architecture compliance

Mirrors the requirements in
[ARCHITECTURE.md](https://github.com/sofa-buffers/documentation/blob/main/ARCHITECTURE.md):
all 8 wire types with `(id << 3) | type` header packing, LEB128 varint and
zig-zag signed encoding, little-endian byte order, fixed `0x07` sequence-end,
streaming encode via flush callback and streaming decode via `feed`, and the
`sofab::` namespace.

## Build & test

Header-only — just add `cpp20/include` to your include path. To run the checks:

```sh
g++ -std=c++20 -Wall -Wextra -I cpp20/include cpp20/test/test_roundtrip.cpp -o /tmp/t && /tmp/t
# or:
cmake -S cpp20 -B cpp20/build && cmake --build cpp20/build && ctest --test-dir cpp20/build
```

## Example

```cpp
#include "sofab/sofab.hpp"

// encode
sofab::OStreamInline<64> os;
os.write(1, 42u).write(2, -7).write(3, "hi");
std::span<const uint8_t> msg{os.data(), os.bytesUsed()};

// decode
struct Sensor : sofab::IStreamMessage {
    uint32_t id = 0; float value = 0;
    void deserialize(sofab::IStreamImpl& is, sofab::id i, size_t, size_t) noexcept override {
        switch (i) { case 1: is.read(id); break; case 2: is.read(value); break; }
    }
};
sofab::IStreamObject<Sensor> in;
in.feed(msg.data(), msg.size());
// (*in).id and (*in).value now hold the decoded values
```
