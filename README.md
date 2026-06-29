<p align="center"><img src="assets/sofabuffers_logo.png" alt="SofaBuffers" height="140"></p>

# SofaBuffers

<b>Structured Objects For Anyone</b><br>
<i>... so optimized, feels amazing.</i>

[Would you like to know more?](https://github.com/sofa-buffers)

## SofaBuffers C/C++ library

[![C coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sofa-buffers/corelib-c-cpp/badges/coverage-c.json)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml)
[![Docs](https://img.shields.io/badge/docs-online-blue)](https://sofa-buffers.github.io/corelib-c-cpp/)

[GitHub repository](https://github.com/sofa-buffers/corelib-c-cpp)

A dependency-free, **heap-free**, **streaming** C99 / C++20 implementation of the
SofaBuffers (*Sofab*) serialization format. It packs structured fields into a
caller-owned buffer and decodes them through a small callback-driven decoder — no
allocator and no third-party dependencies — with an API simple enough to use
without a code generator, so the same wire format runs from deeply embedded MCUs
up to IoT-class devices.

### Supported languages

- **C99** or later
- **C++20** or later

### Built with following compilers

| Target | Status |
| - | - |
| GCC x86_64 (little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml) |
| Clang x86_64 (little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-clang-x86_64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-clang-x86_64.yaml) |
| GCC MIPS (big endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-mips.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-mips.yaml) |
| GCC PowerPC (big endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-powerpc.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-powerpc.yaml) |
| GCC RISCV-V 64 (little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-riscv64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-riscv64.yaml) |

## Why this design

| Goal | How |
|------|-----|
| No allocator | All state lives in caller-provided buffers and structs; nothing is ever heap-allocated, avoiding heap fragmentation. |
| No dependencies | The corelib pulls in no third-party libraries, so it drops cleanly into any toolchain. |
| Streaming **out** | `sofab_ostream` writes into a small caller buffer and invokes a flush callback whenever it fills, so a message can exceed available RAM. |
| Streaming **in** | `sofab_istream` is a callback-driven decoder fed arbitrary byte chunks; large string/blob payloads are delivered in pieces. |
| Reserve-offset | `sofab_ostream_init` takes a start offset that leaves room at the front of the buffer for a lower-layer protocol header (saves a copy). |
| Usable without a generator | The field-level API is explicit enough to write by hand, while the descriptor-driven `object` API stays optional. |
| C++ without surprises | The C++ wrapper reports the first error through a `Result` instead of throwing and avoids `std::iostream`, for toolchains that forbid exceptions or lack heavy stdlib facilities. |
| Portable | Plain C99 / C++20 with explicit endianness handling, so the same wire format works across little- and big-endian architectures. |
| Small footprint | CMake options drop whole code paths (e.g. `SOFAB_DISABLE_OBJECT_API`); release builds target size with `-Os`, down to ~1&nbsp;KB of `.text`. |

## Usage

The corelib serializes fields into a caller-owned buffer and deserializes them
through a small callback-driven decoder. No heap is required, and the same wire
format is shared by the C and C++ APIs.

### C: encoding with `ostream`

Initialize an `sofab_ostream_t` over a buffer, write a few fields, then read the
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

### C: descriptor-driven `object` API

When you have many messages, the generic transcoder keeps the footprint small:
instead of per-field API calls, a constant descriptor drives both encoding and
decoding of a plain C struct.

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

sofab::OStreamInline<64> os; // 64-byte buffer held inline

os.write(1, 42u)                      // unsigned
  .write(2, -7)                       // signed
  .write(3, "hi");                    // string

std::span<const uint8_t> msg{os.data(), os.bytesUsed()};
```

### C++: decoding with `IStream`

Derive a message from `sofab::IStreamMessage` and dispatch fields in
`deserialize()`; `IStreamObject` wires the decoder to an embedded instance:

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

## API summary

The full reference is generated from the headers (see
[Documentation](https://sofa-buffers.github.io/corelib-c-cpp/)); this is the
high-level surface.

### Setting up a stream (C)

Both contexts are initialised over a **caller-owned** buffer/struct, then driven;
see [Memory handling](#memory-handling) for who owns what.

- **Encode** — `sofab_ostream_init(ctx, buffer, buflen, offset, flush, usrptr)`,
  then the `sofab_ostream_write_*` helpers (scalars, `fp32`/`fp64`, `string`,
  `blob`, `array_of_*`, `sequence_begin`/`end`), then `sofab_ostream_flush()`.
  `sofab_ostream_buffer_set()` swaps in a fresh buffer mid-stream.
- **Decode** — `sofab_istream_init(ctx, field_cb, usrptr)`, then
  `sofab_istream_feed()` with arbitrarily small chunks. Inside the field callback
  a single `sofab_istream_read_*` call *binds* the destination for that field
  (`read_sequence` descends into a nested sequence); bind nothing and the field —
  and any sub-sequence — is skipped.

### Setting up a stream (C++)

The C++ layer is a header-only RAII wrapper; pick a class by where its buffer
lives. `write(id, value)` / `read(value)` deduce the wire encoding from the C++
type — an unsupported type is a compile-time `static_assert`, not a silent
mis-encode (and disabled capabilities, e.g. `double` under
`SOFAB_DISABLE_FP64_SUPPORT`, fail the same way).

| Class | Buffer / role |
| - | - |
| `OStreamInline<N, Offset=0>` | output, `N`-byte buffer inline on the stack (no heap; `Offset` reserves room for a lower-layer header) |
| `OStream(buflen, offset=0)` | output, heap buffer (`shared_ptr<uint8_t[]>`) with an optional flush callback for streaming |
| `OStreamObject<T>` | output, inline buffer sized from `T::_maxSize`; `serialize()` encodes one `OStreamMessage` type |
| `IStreamInline(cb)` | input, decode with a `std::function` callback (no subclassing) |
| `IStreamObject<T>` | input, decode into one `IStreamMessage` type; access fields via `->` / `*` |

Define a message by deriving from `OStreamMessage` / `IStreamMessage` and
overriding `serialize` / `deserialize`. As in C, each `read()` only *binds* a
destination that a later `feed()` fills — `read(std::string&)` must be pre-sized,
and the `std::vector<std::string>` / `std::vector<std::vector<uint8_t>>` overloads
decode sequences of variable-length elements.

### Memory handling

This is the core of the design: **no heap is ever used**, and all state lives in
caller-provided structs and buffers. The `sofab_ostream_t` / `sofab_istream_t`
contexts hold only pointers and counters (which is why the [Footprint](#footprint)
tables report `.data` and `.bss` of `0.0KB`).

**Encoder — caller owns the output buffer.**

- `sofab_ostream_init(ctx, buffer, buflen, offset, flush, usrptr)` hands the stream
  a writable buffer the *caller* owns; the stream never copies, allocates, or frees
  it and only advances a cursor inside it. The buffer must stay valid until it is
  replaced or the stream is torn down.
- `offset` reserves space at the front of the buffer for a lower-layer protocol
  header, so the framing layer can prepend its header without an extra copy.
- When the buffer fills (or on an explicit `sofab_ostream_flush()`), the optional
  **flush callback** drains the bytes to a socket/file/sink. With no callback set,
  a full buffer makes the write return an error instead of overflowing.
- The callback can call `sofab_ostream_buffer_set()` to hand the stream a fresh
  buffer **mid-stream**, so a single message can exceed available RAM. C++ mirrors
  this: `OStreamInline<N>` keeps the buffer inline on the stack, while heap-backed
  `OStream` accepts a `shared_ptr<uint8_t[]>` or a flush callback.

**Decoder — lazy/deferred binding into caller memory.**

The C `istream` is **deferred**: a `read_*()` call does *not* copy anything. It
only records the destination pointer, length, and field options in the context
(`read_field` / `read_array` just store `target_ptr` / `target_len` / `target_opt`;
`read_sequence` registers a child decoder). The actual bytes are **copied into the
bound destination by later `feed()` calls** as they arrive — a scalar is written
once its varint completes, and fixed-length/array/string payloads are streamed
byte-by-byte straight into `target_ptr`.

Two consequences follow directly, and they are the most important rules for using
this library:

1. **Destinations must be address-stable and must outlive decoding.** Because the
   pointer you bind is filled on a *later* `feed()`, it cannot point into a stack
   frame (or any storage) that goes away before the message is fully fed. This is
   exactly why C++ `read(std::string&)` must **pre-size** the string (it binds
   `data()` now) and why the `std::vector<std::string>` helper emplaces an element
   and binds *that* heap-stable slot — it never reads into a temporary and moves.
2. **Data is copied into your memory, not aliased** (this port is **not**
   zero-copy; see [Is this implementation zero-copy?](#is-this-cc-implementation-zero-copy)).
   Copying into caller-provided, correctly-typed storage is what makes the format
   safe on every architecture regardless of alignment or endianness, and it keeps
   sizes bounded by the buffers you supply: `read_string` caps the payload at
   `maxlen-1` and a `read_blob` field must carry the BLOB tag; an oversized or
   malformed field is rejected with `SOFAB_RET_E_INVALID_MSG` rather than
   overrunning the destination. Fields you do not bind — and their nested
   sub-sequences — are skipped without touching any memory.

The decoder allocates nothing of its own: the `sofab_istream_t`, any nested
`sofab_istream_decoder_t` structs, and every destination buffer are all provided
by the caller and must outlive the `feed()` sequence. This ties into the
[Footprint](#footprint) memory-requirement tables, whose zero `.data`/`.bss`
figures reflect that no state is held outside those caller structures.

### Differences from the pure-C++20 port (`corelib-cpp`)

The [`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp) project exposes
the *same* `sofab::` C++ surface but is a from-scratch C++20 implementation rather
than a wrapper over the C core. Comparing the two `sofab.hpp` headers, the
important C++-API differences are:

| Topic | This library (C-backed) | `corelib-cpp` (pure C++20) |
| - | - | - |
| Decode-buffer lifetime | Lazy: `read()` *binds* the destination and the bytes are filled by a later `feed()` — destinations must be address-stable and outlive decoding | Immediate copy from a cursor over contiguous memory |
| `read(std::string &)` | Must **pre-size** the string; reads into existing storage | Auto-sizes via `assign` |
| `read(std::string_view &)` | Not available | Zero-copy read, view aliases the source buffer |
| Sequence-of-varlen helpers | `read(std::vector<std::string> &)` and `read(std::vector<std::vector<uint8_t>> &)` | Not available |
| Build-time capability flags | Honors the C core's `SOFAB_DISABLE_*` switches (`FP64`/`INT64`/`ARRAY` → `static_assert` on use; `FIXLEN`/`SEQUENCE` → hard `#error`) | None — every type always compiled in |

## Feature flags

The corelib can be trimmed at compile time by defining these macros (e.g. via
`-DSOFAB_DISABLE_ARRAY_SUPPORT`). Disabling unused features removes their code
paths and shrinks the footprint (see [Footprint](#footprint)).

| Macro | Default | Effect |
| - | - | - |
| `SOFAB_DISABLE_FIXLEN_SUPPORT` | off | Drop fixed-length fields: floats, strings, and blobs |
| `SOFAB_DISABLE_ARRAY_SUPPORT` | off | Drop array fields (scalar arrays and fixed-length arrays) |
| `SOFAB_DISABLE_SEQUENCE_SUPPORT` | off | Drop nested sequence framing |
| `SOFAB_DISABLE_FP64_SUPPORT` | off | Drop 64-bit float (`fp64`) support; auto-defined where `double` is not 8 bytes |
| `SOFAB_DISABLE_INT64_SUPPORT` | off | Narrow unsigned/signed scalar varint values from 64-bit to 32-bit (drops the `u64`/`i64` read and array helpers) |
| `SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` | off | Skip overflow checks when decoding integers (smaller/faster, less safe) |

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

The descriptor-driven object API is excluded with the CMake option
`-DSOFAB_DISABLE_OBJECT_API=ON` (drops `object.c` entirely). Example minimal
build (integers only):

```sh
cmake -S . -B build -DSOFAB_DISABLE_FIXLEN_SUPPORT=1 -DSOFAB_DISABLE_ARRAY_SUPPORT=1 -DSOFAB_DISABLE_SEQUENCE_SUPPORT=1
```

## Build & test

The library is built with [CMake](https://cmake.org/) (version 3.10 or later) and a C99 / C++20 capable toolchain such as GCC or Clang.

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
  round-trips and chunk-feeds every vector.

The vector suite is also built as a standalone, **feature-flag-tolerant** runner
(`sofab_vectortest`): each vector declares the capabilities it needs (`requires`),
so a build compiled with a `SOFAB_DISABLE_*` flag skips the vectors it can't
handle and reports a `run` / `skipped` count. This lets one vector file validate
every configuration. CI runs it across a [feature-flag](#feature-flags) matrix —
`max`, each flag off in turn, and a minimal build — so the union exercises all
configurations. Boundary vectors that fit in 32 bits (e.g. `UINT32_MAX`,
`INT32_MIN/MAX`) run in every build, giving the reduced builds their own min/max
coverage.

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
| `SOFAB_ENABLE_BENCH` | `ON` | Build the throughput benchmarks (`bench_c` / `bench_cpp`) |
| `SOFAB_ENABLE_COVERAGE` | `OFF` | Enable code coverage instrumentation (`-O0 -g --coverage`) |
| `SOFAB_ENABLE_FUZZ` | `OFF` | Enable fuzzing instrumentation (sanitizers) |
| `SOFAB_ENABLE_DOXYGEN` | `OFF` | Build the `doc` target (API documentation) |
| `SOFAB_ENABLE_VECTORGEN` | `OFF` | Build the test-vector generator (see `test/vectorgen`) |
| `SOFAB_DISABLE_OBJECT_API` | `OFF` | Exclude the descriptor-driven object API (`object.c`) |

For example, to configure a debug build with coverage enabled:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSOFAB_ENABLE_COVERAGE=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Benchmarks

Two complementary benchmark tools are built for both C and C++ (enabled by
`SOFAB_ENABLE_BENCH`, on by default):

| Tool | Measures | Use it for |
| - | - | - |
| `perf` | intrinsic cost in **instructions/operation** (machine-independent, deterministic) | comparing algorithmic cost across changes or languages without hardware noise |
| `bench` | practical **throughput in MB/s** on the current hardware | seeing real-world speed on a concrete target |

Convenience CMake targets run them:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --build build --target run_bench            # throughput (MB/s), C and C++
cmake --build build --target run_perf             # per-op cost, C and C++
cmake --build build --target run_bench_callgrind  # instructions/op under Callgrind (needs valgrind)
```

The instruction-count report (`bench/run_callgrind.sh`) is machine-independent,
so its numbers are directly comparable across machines and language
implementations.

## Who is this suitable for?

The C core library is very much aimed at small embedded devices, where C is simply essential. The focus here was therefore on minimal resource consumption.

The C++ core library is aimed more at IoT devices. Such devices do not necessarily run Linux directly, but they are powerful enough to deal with C++/Heap etc. However, the focus here was also on supporting IoT devices that do not support everything specified by C++ - for example, no exceptions are used, as these are often prohibited or not supported. Similarly, std::iostream is not used, as this is often not supported or simply too heavy.

## Is this C/C++ implementation zero-copy?

No. SofaBuffers focuses on generating as little protocol overhead as possible, which is why the words in the payload of a message are not always aligned.
To be compatible with all architectures, the data from the message is copied to user-provided memory.

## Footprint

This table shows the memory requirements of the C corelib for different bare metal architectures.

The lib was always built with `-Os` (optimized for minimal size).

### Full configuration

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~2.7KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~2.8KB | 0.0KB | 0.0KB
| RV32IMC |            ~3.6KB | 0.0KB | 0.0KB
| atmega8 |            ~6.3KB | 0.0KB | 0.0KB

### Minimal configuration

Built with `SOFAB_DISABLE_FIXLEN_SUPPORT`, `SOFAB_DISABLE_ARRAY_SUPPORT`,
`SOFAB_DISABLE_SEQUENCE_SUPPORT`, `SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` and the
`SOFAB_OBJECT_DESCR_SMALL` descriptor profile.

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~1.2KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~1.2KB | 0.0KB | 0.0KB
| RV32IMC |            ~1.6KB | 0.0KB | 0.0KB
| atmega8 |            ~2.8KB | 0.0KB | 0.0KB

Same minimal configuration, additionally without `object.c`
(`SOFAB_DISABLE_OBJECT_API`):

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~0.8KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~0.9KB | 0.0KB | 0.0KB
| RV32IMC |            ~1.0KB | 0.0KB | 0.0KB
| atmega8 |            ~1.9KB | 0.0KB | 0.0KB
