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

### C (`sofab/ostream.h`, `sofab/istream.h`)

| Encoder (`sofab_ostream_*`) | Purpose |
| - | - |
| `init` / `buffer_set` / `flush` / `bytes_used` | set up the stream (with optional start offset + flush callback), swap the buffer mid-stream, flush, query size |
| `write_unsigned` / `write_signed` / `write_boolean` | scalar integers and booleans |
| `write_fp32` / `write_fp64` / `write_string` / `write_blob` | fixed-length values |
| `write_array_of_*` (`u8`..`u64`, `i8`..`i64`, `fp32`, `fp64`) | arrays |
| `write_sequence_begin` / `write_sequence_end` | nested sequence framing |

| Decoder (`sofab_istream_*`) | Purpose |
| - | - |
| `init` / `feed` | register a field callback, then feed bytes in arbitrarily small chunks |
| `read_u8`..`read_u64` / `read_i8`..`read_i64` / `read_bool` | bind a scalar destination inside the callback |
| `read_fp32` / `read_fp64` / `read_string` / `read_blob` | bind a fixed-length destination |
| `read_array_of_*` | bind an array destination |
| `read_sequence` | descend into a nested sequence with a child callback |
| *(read nothing)* | the field — and any sub-sequence — is skipped automatically |

### C++ (`sofab/sofab.hpp`)

| Output class | Purpose |
| - | - |
| `OStreamInline<N, Offset=0>` | Stack-allocated N-byte output stream — zero heap, suitable for bare-metal targets |
| `OStream(buflen, offset=0)` | Heap-backed output stream; accepts an optional `shared_ptr<uint8_t[]>` or flush callback for streaming output |
| `OStreamMessage` | Abstract base: override `serialize(OStreamImpl &)` to define the encoding of a message type |
| `OStreamObject<T>` | Combines an `OStreamMessage`-derived type with its own inline buffer; call `serialize()` to encode |
| `OStream::Result` | Propagates the first error across a chain of `write()` / `writeIf()` / `sequenceBegin()` / `sequenceEnd()` calls |

| Input class | Purpose |
| - | - |
| `IStreamMessage` | Abstract base: override `deserialize(IStreamImpl &, id, size, count)` to bind fields inside the callback |
| `IStreamObject<T>` | Wraps an `IStreamMessage`-derived type; call `feed(buf, len)` to decode; access data via `->` / `*` |
| `IStreamInline` | Lambda-based decoder — pass a `std::function` callback without subclassing |

`write(id, value)` and `read(value)` deduce the wire encoding from the C++ type.

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
  [`assets/README.md`](assets/README.md); generator in
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
| ARMv6-m |            ~2.6KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~2.7KB | 0.0KB | 0.0KB
| ARMv8-m.main+fp |    ~2.7KB | 0.0KB | 0.0KB
| ARMv8.1-m.main+mve | ~2.7KB | 0.0KB | 0.0KB
| atmega8 |            ~6.3KB | 0.0KB | 0.0KB

### Minimal configuration

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~1.0KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~1.1KB | 0.0KB | 0.0KB
| ARMv8-m.main+fp |    ~1.1KB | 0.0KB | 0.0KB
| ARMv8.1-m.main+mve | ~1.1KB | 0.0KB | 0.0KB
| atmega8 |            ~2.6KB | 0.0KB | 0.0KB

corelib API without `object.c`:

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~0.8KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~0.9KB | 0.0KB | 0.0KB
| ARMv8-m.main+fp |    ~0.9KB | 0.0KB | 0.0KB
| ARMv8.1-m.main+mve | ~0.9KB | 0.0KB | 0.0KB
| atmega8 |            ~1.9KB | 0.0KB | 0.0KB
