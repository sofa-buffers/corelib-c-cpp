<p align="center"><img src="assets/sofabuffers_logo.png" alt="SofaBuffers" height="140"></p>

# SofaBuffers

<b>Structured Objects For Anyone</b><br>
<i>... so optimized, feels amazing.</i>

[Would you like to know more?](https://github.com/sofa-buffers)

## SofaBuffers C/C++ library

[![C coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sofa-buffers/corelib-c-cpp/badges/coverage-c.json)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/coverage.yaml)
[![Docs](https://img.shields.io/badge/docs-online-blue)](https://sofa-buffers.github.io/corelib-c-cpp/)

[GitHub repository](https://github.com/sofa-buffers/corelib-c-cpp)

The footprint-optimized SofaBuffers (*Sofab*) codec: a **heap-free C99 object
API** plus a header-only **C++20 wrapper** (`sofab/sofab.hpp`) over the same C
core. It packs structured fields into a caller-owned buffer and decodes them
through a small, callback-driven streaming decoder — no allocator, no
third-party dependencies, and an API simple enough to use without a code
generator. The same wire format runs from deeply embedded MCUs (bare-metal C)
up to IoT-class devices (the C++ wrapper), and this repository is also the
authoritative source of the shared conformance vectors in
[`assets/test_vectors.json`](assets/test_vectors.json).

For desktop/server C++ where raw throughput matters more than code size, see the
sibling pure-C++20 port — [`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp)
— and [Choosing between the two C++ corelibs](#choosing-between-the-two-cc-corelibs).

### Requirements

- A **C99** (or later) and/or **C++20** (or later) compiler — GCC or Clang.
- [CMake](https://cmake.org/) **3.10** or later to build the tests, benchmarks
  and generated documentation. The corelib itself is just a handful of `.c`
  files and headers and can be dropped into any build system.

### Dependencies

**None.** The C core depends only on the freestanding-friendly C standard
headers (`<stdint.h>`, `<stddef.h>`, `<string.h>`, …) and never on a third-party
library. The C++ wrapper is header-only and pulls in only the C++ standard
library (`<array>`, `<span>`, `<string>`, `<vector>`, `<memory>`,
`<functional>`, `<concepts>`); the heap-free subset of it (`OStreamInline`,
`FixedString`, `FixedBytes`, `InlineVector`) compiles under `-fno-exceptions`
`-fno-rtti`.

### Built with the following compilers

CI builds the corelib across a range of architectures and endiannesses:

| Target | Status |
| - | - |
| x86_64 (GCC, little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml) |
| x86_64 (Clang, little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-clang-x86_64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-clang-x86_64.yaml) |
| AArch64 (GCC) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-aarch64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-aarch64.yaml) |
| Cortex-M (GCC, bare metal) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-cortex-m.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-cortex-m.yaml) |
| AVR / ATmega (GCC) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-avr.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-avr.yaml) |
| RL78 (GCC) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-rl78.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-rl78.yaml) |
| MIPS (GCC, big endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-mips.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-mips.yaml) |
| PowerPC (GCC, big endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-powerpc.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-powerpc.yaml) |
| RISC-V 64 (GCC, little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-riscv64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-riscv64.yaml) |

> The non-native targets above are built and run under [QEMU](https://www.qemu.org/) user-mode emulation in CI, so you can reproduce any of them locally without the real hardware.

### Packaging

Distributed as the port `sofa-buffers-corelib-c-cpp`; every route exposes the
same target `sofa-buffers::corelib` and `#include <sofab/…>`.

#### CMake

Pull it straight from the repo with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  sofa-buffers-corelib-c-cpp
  GIT_REPOSITORY https://github.com/sofa-buffers/corelib-c-cpp.git
  GIT_TAG        <tag or branch>
)
FetchContent_MakeAvailable(sofa-buffers-corelib-c-cpp)
target_link_libraries(my_app PRIVATE sofa-buffers::corelib)
```

#### Conan

Install the package, then in CMake:

```cmake
find_package(sofa-buffers-corelib-c-cpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE sofa-buffers::corelib)
```

#### Arduino / PlatformIO

For embedded targets the corelib ships an [Arduino](https://arduino.cc/) manifest
(`library.properties`) and a [PlatformIO](https://platformio.org/) manifest
(`library.json`).

## Why this design

| Goal | How |
|------|-----|
| No allocator | The C core keeps all state in caller-provided buffers and structs; nothing is ever heap-allocated, avoiding heap fragmentation on constrained targets. |
| No dependencies | The corelib pulls in no third-party libraries, so it drops cleanly into any toolchain. |
| Streaming **out** | `sofab_ostream` writes into a small caller buffer and invokes a flush callback whenever it fills, so a message can exceed available RAM. |
| Streaming **in** | `sofab_istream` is a callback-driven decoder fed arbitrary byte chunks; large string/blob payloads are delivered in pieces. |
| Reserve-offset | `sofab_ostream_init` takes a start offset that leaves room at the front of the buffer for a lower-layer protocol header (saves a copy). |
| Usable without a generator | The field-level API is explicit enough to write by hand, while the descriptor-driven `object` API (the generator target) stays optional. |
| C++ without surprises | The C++ wrapper reports the first error through a sticky `Result` instead of throwing and avoids `std::iostream`, for toolchains that forbid exceptions or lack heavy stdlib facilities. |
| Portable | Plain C99 / C++20 with explicit endianness handling, so the same wire format works across little- and big-endian architectures. |
| Small footprint | Compile-time `SOFAB_DISABLE_*` switches drop whole code paths and `-Os` targets size, down to ~1&nbsp;KB of `.text` (see [Benchmarks → Footprint](#footprint)). |

## Usage

The corelib serializes fields into a caller-owned buffer and deserializes them
through a small callback-driven decoder. No heap is required by the C core, and
the same wire format is shared by the C and C++ APIs.

### C: encoding with `ostream`

Initialize a `sofab_ostream_t` over a buffer, write a few fields, then read the
number of bytes produced:

```c
#include "sofab/ostream.h"

uint8_t buf[64];
sofab_ostream_t os;
sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);

sofab_ostream_write_unsigned(&os, 1, 42);
sofab_ostream_write_signed(&os, 2, -7);
sofab_ostream_write_string(&os, 3, "hi");

size_t used = sofab_ostream_bytes_used(&os); /* buf[0..used] holds the message */
```

To serialize messages larger than the buffer, pass a flush callback. It is
invoked whenever the buffer fills up (and on the final `sofab_ostream_flush()`),
letting you hand the bytes off to a socket, file, or any other sink:

```c
static void on_flush(sofab_ostream_t *os, const uint8_t *data, size_t len, void *usrptr)
{
    int fd = *(int *)usrptr;
    write(fd, data, len); /* stream the chunk out; the buffer is then reused */
}

uint8_t scratch[16];
sofab_ostream_t os;
sofab_ostream_init(&os, scratch, sizeof(scratch), 0, on_flush, &fd);

for (uint32_t i = 0; i < 1000; i++)
    sofab_ostream_write_unsigned(&os, i, i);

sofab_ostream_flush(&os); /* flush the final partial buffer */
```

### C: decoding with `istream`

Decoding is callback-driven: feed raw bytes and dispatch on the field id inside
the callback, binding each field to a destination with a `sofab_istream_read_*()`
function. Fields you do not read are skipped automatically.

```c
#include "sofab/istream.h"

struct my_msg { uint64_t a; int64_t b; char text[16]; };

static void on_field(sofab_istream_t *is, sofab_id_t id,
                     size_t size, size_t count, void *usrptr)
{
    struct my_msg *m = usrptr;
    (void)size; (void)count;

    switch (id)
    {
        case 1: sofab_istream_read_u64(is, &m->a); break;
        case 2: sofab_istream_read_i64(is, &m->b); break;
        case 3: sofab_istream_read_string(is, m->text, sizeof(m->text)); break;
    }
}

struct my_msg msg = {0};
sofab_istream_t is;
sofab_istream_init(&is, on_field, &msg);
sofab_istream_feed(&is, buf, used);
```

### C: generated / descriptor-driven `object` API

The `object` API is the code-generator target for C: instead of emitting a
per-field call sequence, a schema compiled by `sofabgen` emits a plain C struct
plus a **constant descriptor** that drives both encoding and decoding. Reusing
one generic transcoder over many messages keeps flash usage small.

```c
#include "sofab/object.h"

typedef struct { uint32_t id; float value; char name[16]; } sensor_t;

static const sofab_object_descr_field_t sensor_fields[] = {
    SOFAB_OBJECT_FIELD(1, sensor_t, id,    SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(2, sensor_t, value, SOFAB_OBJECT_FIELDTYPE_FP32),
    SOFAB_OBJECT_FIELD(3, sensor_t, name,  SOFAB_OBJECT_FIELDTYPE_STRING),
};

static const sofab_object_descr_t sensor_descr =
    SOFAB_OBJECT_DESCR(sensor_fields, 3, NULL, 0);

/* encode */
sensor_t out = { .id = 7, .value = 3.14f, .name = "temp" };
uint8_t buf[64];
sofab_ostream_t os;
sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);
sofab_object_encode(&os, &sensor_descr, &out);
size_t used = sofab_ostream_bytes_used(&os);

/* decode */
sensor_t in;
sofab_object_init(&sensor_descr, &in); /* apply defaults / zero-init */

sofab_istream_t is;
sofab_object_decoder_t dec = { .info = &sensor_descr, .dst = (uint8_t *)&in, .depth = 0 };
sofab_istream_init(&is, sofab_object_field_cb, &dec);
sofab_istream_feed(&is, buf, used);
```

### C++: encoding with `OStream`

The C++ API is a thin, type-deducing wrapper. `write()` picks the right encoding
from the argument type and the calls chain through a `Result` that carries the
first error:

```cpp
#include "sofab/sofab.hpp"

sofab::OStreamInline<64> os; // 64-byte buffer held inline (no heap)

os.write(1, 42u)                      // unsigned
  .write(2, -7)                       // signed
  .write(3, "hi");                    // string

std::span<const uint8_t> msg{os.data(), os.bytesUsed()};
```

The heap-backed `sofab::OStream(buflen)` variant additionally accepts a flush
callback, so the inline example above scales to messages larger than the buffer
by streaming chunks out through that callback — the C++ mirror of the C
`ostream` flush path.

### C++: decoding with `IStream`

Derive a message from `sofab::IStreamMessage` and dispatch fields in
`deserialize()`; `IStreamObject` wires the decoder to an embedded instance (this
is also the shape generated C++ code takes):

```cpp
#include "sofab/sofab.hpp"

class Sensor : public sofab::IStreamMessage
{
    struct Data { uint32_t id = 0; float value = 0; } data_;

    void deserialize(sofab::IStreamImpl &is, sofab::id id,
                     size_t size, size_t count) noexcept override
    {
        (void)size; (void)count;

        switch (id)
        {
            case 1: is.read(data_.id);    break;
            case 2: is.read(data_.value); break;
        }
    }

public:
    Data* operator->() noexcept { return &data_; }
};

sofab::IStreamObject<Sensor> sensor;
sensor.feed(msg.data(), msg.size()); // msg from the encoding example above
// sensor->id and sensor->value now hold the decoded values
```

For a callback-only decode without subclassing, `sofab::IStreamInline` takes a
`std::function` field callback that binds destinations exactly as the C
`sofab_istream` callback does.

## API summary

The full reference is generated from the headers (see
[Documentation](https://sofa-buffers.github.io/corelib-c-cpp/)); this is the
high-level surface. There are two layers over one wire format: the **C object
API** (`sofab/ostream.h`, `sofab/istream.h`, `sofab/object.h`) and the
header-only **C++ wrapper** (`sofab/sofab.hpp`).

### Encoding

Both languages encode by initializing an output stream over a **caller-owned**
buffer and appending fields:

- **C** — `sofab_ostream_init(ctx, buffer, buflen, offset, flush, usrptr)`, then
  the `sofab_ostream_write_*` family: scalars (`write_unsigned`/`write_signed`/
  `write_boolean`), fixed-length values (`write_fp32`/`write_fp64`/`write_string`/
  `write_blob`), scalar and float arrays (`write_array_of_*`), and nested
  sequences (`write_sequence_begin`/`write_sequence_end`). Every call returns a
  `sofab_ret_t`; a full buffer without a flush callback yields
  `SOFAB_RET_E_BUFFER_FULL`. Call `sofab_ostream_flush()` before teardown.
- **C++** — pick a stream by where its buffer lives, then call
  `write(id, value)`, which deduces the wire encoding from the C++ type. Calls
  chain through a `Result` whose **first error is sticky**, so a whole chain can
  be checked once at the end (`writeIf(id, value, cond)`, `sequenceBegin`/
  `sequenceEnd` chain the same way). A type whose capability was compiled out of
  the C core is a compile-time `static_assert`, not a silent mis-encode.

| C++ output class | Buffer / role |
| - | - |
| `OStreamInline<N, Offset=0>` | `N`-byte buffer inline on the stack — no heap; `Offset` reserves room for a lower-layer header |
| `OStream(buflen, offset=0)` | heap buffer (`shared_ptr<uint8_t[]>`), with an optional flush callback for chunked streaming |
| `OStreamObject<T>` | inline buffer sized from `T::_maxSize`; `serialize()` encodes one `OStreamMessage` |

### Decoding

Decoding is a **pull/visitor** model driven by feeding bytes:

- **C** — `sofab_istream_init(ctx, field_cb, usrptr)`, then `sofab_istream_feed()`
  with arbitrarily small chunks. Inside the field callback a single
  `sofab_istream_read_*` call *binds* the destination for that field
  (`read_u32`, `read_fp64`, `read_string`, `read_blob`, `read_array_of_*`, or
  `read_sequence` to descend into a nested sequence). Bind nothing and the field
  — and any sub-sequence — is skipped.
- **C++** — derive from `IStreamMessage` and dispatch on the field id in
  `deserialize()`, calling `read(member)` (type-deduced) to bind each field;
  `IStreamObject<T>` owns an instance and exposes it via `->` / `*`, while
  `IStreamInline` takes a `std::function` callback instead of a subclass.
  Variable-length sequences are decoded by the `read(std::vector<std::string>&)`
  and `read(std::vector<std::vector<uint8_t>>&)` overloads; blobs use the
  `read(void*, size_t)` overload.

### Memory handling

Ownership and lifetime differ from a copy-on-read decoder, so this is worth
being precise about. The **C core never allocates**: every context, decoder and
buffer is caller-provided. The C++ wrapper is heap-free too *when* you stick to
inline buffers (`OStreamInline`) and the fixed-capacity `FixedString<N>` /
`FixedBytes<N>` / `InlineVector<T,N>` field types; the `OStream(buflen)`
constructor and the `std::string` / `std::vector` read overloads do use the heap.

**Output buffer — owned by the caller.** `sofab_ostream_init()` takes a writable
buffer the stream never allocates, copies, or frees; it only advances a cursor.
`offset` reserves room at the front for a lower-layer header. When the buffer
fills (or on `sofab_ostream_flush()`) an optional flush callback drains it — with
no callback, a full buffer returns `SOFAB_RET_E_BUFFER_FULL` instead of
overflowing — and may call `sofab_ostream_buffer_set()` to swap in a fresh buffer
mid-stream.

**Input buffer and message object — deferred-copy binding.** This port uses a
**deferred-copy decode model**, so a `read_*()` / `read()` call copies nothing:
it only records *where* the value goes (pointer, length, type). The bytes are
written into that destination by later `feed()` calls, as they arrive. Two rules
follow:

1. **Destinations must be address-stable and outlive decoding.** The pointer you
   bind is filled on a *later* `feed()`, so it cannot point at storage that
   disappears or moves first. Hence C++ `read(std::string&)` must be **pre-sized**
   (it reads into existing storage), the `std::vector<std::string>` helper binds
   an emplaced heap-stable slot, and the `FixedString`/`FixedBytes`/`InlineVector`
   types are safe targets because their inline storage never moves.
2. **Data is copied into your memory, not aliased.** This port is **not**
   zero-copy: SofaBuffers minimizes protocol overhead, so payload words are not
   guaranteed to be aligned on the wire. To stay portable across every
   architecture, decoded values are copied into your typed storage rather than
   viewed in place — which is also what keeps decoding alignment/endianness-safe
   and bounds sizes to your buffers. Oversized or malformed fields are rejected
   with `SOFAB_RET_E_INVALID_MSG`, and unbound fields (and their sub-sequences)
   are skipped untouched.

## Feature flags

The corelib ships the full wire format by default, but the C core can be trimmed
at compile time by defining these macros (e.g. `-DSOFAB_DISABLE_ARRAY_SUPPORT`).
Disabling unused features removes their code paths and shrinks the footprint.

| Macro | Default | Effect |
| - | - | - |
| `SOFAB_DISABLE_FIXLEN_SUPPORT` | off | Drop fixed-length fields: floats, strings, and blobs |
| `SOFAB_DISABLE_ARRAY_SUPPORT` | off | Drop array fields (scalar arrays and fixed-length arrays) |
| `SOFAB_DISABLE_SEQUENCE_SUPPORT` | off | Drop nested sequence framing |
| `SOFAB_DISABLE_FP64_SUPPORT` | off | Drop 64-bit float (`fp64`) support; auto-defined where `double` is not 8 bytes |
| `SOFAB_DISABLE_INT64_SUPPORT` | off | Narrow unsigned/signed scalar varint values from 64-bit to 32-bit (drops the `u64`/`i64` read/write and array helpers) |
| `SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` | off | Skip overflow checks when decoding integers (smaller/faster, less safe) |

In addition, the descriptor-driven object API selects an integer-width profile
via `SOFAB_OBJECT_DESCR_PROFILE` (`SOFAB_OBJECT_DESCR_SMALL` / `_MEDIUM` (default)
/ `_BIG`, i.e. `uint8_t` / `uint16_t` / `uint32_t` descriptor `id`/`offset`/`size`
members), and the whole object API can be dropped with the
`SOFAB_DISABLE_OBJECT_API` CMake option.

The C++ wrapper honors these switches: the type-dispatch capabilities
(`FP64`/`INT64`/`ARRAY`) turn into a `static_assert` only when a disabled type is
actually used, while the structural ones (`FIXLEN`/`SEQUENCE`) underpin most of
the C++ surface and so are rejected outright with a `#error` — use the C API
directly for those configs.

> **`SOFAB_DISABLE_INT64_SUPPORT` — change only if you know what you are doing.**
> It shrinks 64-bit varint math (smaller/faster on 32-bit MCUs) but has wire- and
> API-level side effects:
> - **Wire compatibility:** the format is width-agnostic, so messages whose values
>   all fit in 32 bits stay byte-identical and interoperable. A value beyond 32-bit
>   range from a 64-bit peer is **rejected** as a malformed message
>   (`SOFAB_RET_E_INVALID_MSG`) — never silently truncated — so a 32-bit build
>   safely refuses data it cannot represent.
> - **ABI:** the value types appear in public signatures and context structs, so
>   32-bit and 64-bit builds are **not** ABI-compatible — don't mix them.
> - **Field ids:** `SOFAB_ID_MAX` shrinks to `UINT32_MAX >> 3`, since the field
>   header is itself a varint of `(id << 3) | type`.
> - **Conformance:** the shipped test vectors include 64-bit values and won't
>   decode in this mode.

## Build & test

The library is built with CMake and a C99 / C++20 capable toolchain such as GCC
or Clang.

### Build

```sh
cmake -S . -B build
cmake --build build --parallel
```

### Test

Tests are registered with CTest:

```sh
ctest --test-dir build --output-on-failure
```

There are two layers:

- **Unit tests** (`test_c` / `test_cpp`) — hand-written C (Unity) and C++ (Catch2)
  tests covering the encoder, decoder and object API, including error paths
  (truncated varints, unbalanced sequences, overflow). These run in the
  full-feature ("max") build, which exercises every wire type at 64-bit width.
- **Conformance-vector suite** (`test_vectors_c`) — a language-agnostic set of
  vectors in [`assets/test_vectors.json`](assets/test_vectors.json), each pairing
  a message with its exact serialized bytes (format documented in
  [`assets/test_vectors_README.md`](assets/test_vectors_README.md); generator in
  [`test/vectorgen`](test/vectorgen)). The shared engine encodes, decodes,
  round-trips and chunk-feeds every vector. This repository is the authoritative
  source of that shared vector file.

The vector suite is also built as a standalone, **feature-flag-tolerant** runner
(`sofab_vectortest`): each vector declares the capabilities it needs (`requires`),
so a build compiled with a `SOFAB_DISABLE_*` flag skips the vectors it can't
handle and reports a `run` / `skipped` count. This lets one vector file validate
every configuration. CI runs it across a [feature-flag](#feature-flags) matrix
(the `Matrix: C features` and `Matrix: C++ features` workflows) — `max`, each
flag off in turn, and a minimal build — so the union exercises all
configurations.

To run a reduced configuration directly:

```sh
cmake -S . -B build -DSOFAB_ENABLE_CPP=OFF -DCMAKE_C_FLAGS="-DSOFAB_DISABLE_INT64_SUPPORT"
cmake --build build --target sofab_vectortest   # the unit tests are max-only
./build/test/c/sofab_vectortest
```

### Useful CMake options

| Option | Default | Description |
| - | - | - |
| `SOFAB_ENABLE_CPP` | `ON` | Build the C++ tests |
| `SOFAB_ENABLE_CPP_SMOKE` | `OFF` | Build the Catch2-free C++ wrapper smoke test (for reduced configs) |
| `SOFAB_ENABLE_BENCH` | `ON` | Build the benchmarks (`bench_c`/`bench_cpp`, `perf_c`/`perf_cpp`) |
| `SOFAB_ENABLE_COVERAGE` | `OFF` | Enable code coverage instrumentation (`-O0 -g --coverage`) |
| `SOFAB_ENABLE_FUZZ` | `OFF` | Enable fuzzing instrumentation (sanitizers) |
| `SOFAB_ENABLE_DOXYGEN` | `OFF` | Build the `doc` target (API documentation) |
| `SOFAB_ENABLE_VECTORGEN` | `OFF` | Build the JSON test-vector generator (see `test/vectorgen`) |
| `SOFAB_DISABLE_OBJECT_API` | `OFF` | Exclude the descriptor-driven object API (`object.c`) |

For example, to configure a debug build with coverage enabled:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSOFAB_ENABLE_COVERAGE=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Benchmarks

Two complementary benchmark tools are built for both C and C++ (enabled by
`SOFAB_ENABLE_BENCH`, on by default). The corelib sources are compiled straight
into each benchmark at `-O3`, independent of the size-optimized (`-Os`) library
the rest of the project links.

| Tool | Measures | Use it for |
| - | - | - |
| `perf_c` / `perf_cpp` | intrinsic cost in **CPU cycles/operation** plus throughput | comparing algorithmic cost across changes or languages |
| `bench_c` / `bench_cpp` | practical **throughput in MB/s** on the current hardware | seeing real-world speed on a concrete target |

Convenience CMake targets build and run them:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --build build --target run_bench            # throughput (MB/s), C and C++
cmake --build build --target run_perf             # per-op cost (cycles/op + MB/s)
cmake --build build --target run_bench_callgrind  # instructions/op under Callgrind (needs valgrind)
```

The Callgrind instruction-count report (`bench/run_callgrind.sh`) is
machine-independent, so its C-vs-C++ numbers are directly comparable across
machines and language implementations.

### Footprint

Because the C core never allocates, its `.data`/`.bss` are `0.0KB` and the whole
cost is `.text` (flash). The tables below are the size of the built static
library (`size libsofabuffers.a`) for several bare-metal architectures, always
compiled with `-Os`; CI reproduces them in the `Build: Cortex-M`, `Build: AVR`
and related workflows.

**Full configuration**

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~2.7KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~2.8KB | 0.0KB | 0.0KB |
| RV32IMC | ~3.6KB | 0.0KB | 0.0KB |
| atmega8 | ~6.3KB | 0.0KB | 0.0KB |

**Minimal configuration** — built with `SOFAB_DISABLE_FIXLEN_SUPPORT`,
`SOFAB_DISABLE_ARRAY_SUPPORT`, `SOFAB_DISABLE_SEQUENCE_SUPPORT`,
`SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` and the `SOFAB_OBJECT_DESCR_SMALL`
descriptor profile:

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~1.2KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~1.2KB | 0.0KB | 0.0KB |
| RV32IMC | ~1.6KB | 0.0KB | 0.0KB |
| atmega8 | ~2.8KB | 0.0KB | 0.0KB |

Same minimal configuration, additionally without `object.c`
(`SOFAB_DISABLE_OBJECT_API`):

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~0.8KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~0.9KB | 0.0KB | 0.0KB |
| RV32IMC | ~1.0KB | 0.0KB | 0.0KB |
| atmega8 | ~1.9KB | 0.0KB | 0.0KB |

## Choosing between the two C/C++ corelibs

SofaBuffers ships two same-format C++ codecs, tuned for opposite ends of the
spectrum. This repository (`corelib-c-cpp`) is the **footprint / embedded**
choice; [`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp) is the
**throughput** choice.

- **`corelib-c-cpp` (this repo)** — a heap-free C99 object API plus a header-only
  C++20 wrapper over it. Aimed at small embedded devices where C is essential and
  every byte of flash/RAM counts (the C API), and at IoT-class devices that run
  C++ but often forbid exceptions and `std::iostream` (the C++ wrapper). Decoding
  is deferred-copy into caller-owned, address-stable storage.
- **`corelib-cpp`** — a from-scratch pure-C++20 implementation exposing the *same*
  `sofab::` surface, tuned for raw throughput on desktop/server targets. It reads
  immediately from a cursor over contiguous memory and can hand back zero-copy
  `std::string_view`s.

Pick this repo for bare-metal C, MCU-class C++, or a shared C/C++ wire format;
pick `corelib-cpp` when you are on a hosted C++20 target and want maximum speed.

Approximate head-to-head figures from the multi-language benchmark arena
(best-of-5, comparable only within a language):

| Use case | Library | vs. | Throughput | Bare-metal Cortex-M flash |
| - | - | - | - | - |
| Embedded **C** | `corelib-c-cpp` (C API) | nanopb | ~2× | ~3.5&nbsp;KB vs ~6.6&nbsp;KB |
| Embedded **C++** | `corelib-c-cpp` (C++ wrapper) | EmbeddedProto | ~2.1× | ~6.8&nbsp;KB vs ~9.4&nbsp;KB |
| Throughput **C++** | `corelib-cpp` (pure C++20) | protobuf | ~1.5× (~434-byte wire) | — (desktop/server) |

Because both C++ ports share the `sofab::` API, the practical differences show up
in the decode-buffer contract:

| Topic | This library (C-backed) | `corelib-cpp` (pure C++20) |
| - | - | - |
| Decode-buffer lifetime | Deferred: `read()` *binds* the destination; bytes are filled by a later `feed()`, so destinations must be address-stable and outlive decoding | Immediate copy from a cursor over contiguous memory |
| `read(std::string &)` | Must **pre-size** the string; reads into existing storage | Auto-sizes via `assign` |
| `read(std::string_view &)` | Not available | Zero-copy read; the view aliases the source buffer |
| Sequence-of-varlen helpers | `read(std::vector<std::string>&)` and `read(std::vector<std::vector<uint8_t>>&)` | Not available |
| Heap-free field types | `FixedString<N>`, `FixedBytes<N>`, `InlineVector<T,N>` | — |
| Build-time capability flags | Honors the C core's `SOFAB_DISABLE_*` switches (`FP64`/`INT64`/`ARRAY` → `static_assert` on use; `FIXLEN`/`SEQUENCE` → hard `#error`) | None — every type always compiled in |
