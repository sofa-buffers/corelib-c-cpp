# SofaBuffers — pure C++20 implementation

A second, independent C++ implementation of the SofaBuffers core library, written
from scratch in **C++20 with no C backend**. It exposes the same public API as the
C-wrapping [`src/include/sofab/sofab.hpp`](../src/include/sofab/sofab.hpp)
(`sofab::OStream` / `sofab::OStreamInline` / `sofab::IStreamObject` / …) so it is a
drop-in alternative — put `cpp20/include` on the include path instead of
`src/include` and the `#include "sofab/sofab.hpp"` is unchanged.

> Status: validated against the **full shared conformance suite** — all 47
> vectors in `assets/test_vectors.json` pass encode / decode / roundtrip /
> chunked (329 checks), the same coverage the C library runs.

## Why a second implementation

The C++ wrapper is thin and inherits the C library's byte-at-a-time decoder. This
implementation explores what a native, modern-C++ core looks like — and, in
particular, a decoder tuned for the **common case where the whole message is
already in contiguous memory**.

## Performance focus

Where the C library is optimised for **minimal code/RAM**, this implementation is
optimised for **throughput** — code size and memory use are not a concern. The
hot paths are built around bulk, allocation-free operations:

- **Encoder:** payloads are written with a single `memcpy` when they fit the
  buffer; a field's header and value varints are emitted as one combined write;
  whole float arrays are copied in one shot on little-endian hosts (wire bytes ==
  native bytes).
- **Decoder:** the common case (a whole message already in contiguous memory)
  parses **with zero copies and zero allocations** — the cursor walks the
  caller's buffer in place; float arrays are bulk-`memcpy`'d on little-endian; and
  reading a field as `std::string_view` yields a **zero-copy view into the
  buffer** instead of allocating a `std::string`.

Indicative numbers for a ~112-byte mixed message (`cpp20/bench`, `-O3`, x86-64):
encode ≈ 1.3–1.5 GB/s, decode ≈ 0.8 GB/s. Run them with
`cmake --build cpp20/build --target run_bench`.

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
- **Streaming is still supported.** `feed()` dispatches each **complete top-level
  field** directly from the caller's memory; only an incomplete trailing field is
  buffered for the next `feed`. The common path — one `feed(whole_message)` —
  parses everything in a single in-place pass with no allocation.
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
cmake -S cpp20 -B cpp20/build && cmake --build cpp20/build && ctest --test-dir cpp20/build
```

Two test binaries run:

- **`test_roundtrip`** — focused encode/decode/nested/chunked/skip checks with
  inline expectations.
- **`test_vectors`** — drives the C++20 encoder/decoder over the full
  `assets/test_vectors.json` suite (47 vectors, 329 checks: encode, decode,
  roundtrip, chunked encode/decode), reusing the repo's vendored JSON reader.

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
