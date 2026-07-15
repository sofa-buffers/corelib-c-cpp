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
dependencies, no code generator required. The same wire format runs from
bare-metal MCUs (C) up to IoT-class devices (the C++ wrapper). This repo is also
the authoritative source of the shared conformance vectors in
[`assets/test_vectors.json`](assets/test_vectors.json).

For desktop/server C++ where throughput matters more than code size, see the
sibling pure-C++20 port [`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp)
and [Choosing between the two C/C++ corelibs](#choosing-between-the-two-cc-corelibs).

### Requirements

- A **C99** and/or **C++20** compiler — GCC or Clang.
- [CMake](https://cmake.org/) **3.10**+ for tests, benchmarks and docs. The
  corelib itself is a handful of `.c` files and headers; drop it into any build.

### Dependencies

**None.** The C core uses only freestanding-friendly standard headers
(`<stdint.h>`, `<stddef.h>`, `<string.h>`, …). The C++ wrapper is header-only and
pulls in only the C++ standard library; its heap-free subset (`OStreamInline`,
`FixedString`, `FixedBytes`, `InlineVector`) compiles under `-fno-exceptions
-fno-rtti`.

### Built with the following compilers

CI builds the corelib across many architectures and endiannesses (non-native
targets run under [QEMU](https://www.qemu.org/) user-mode emulation):

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

### Packaging

Distributed as the port `sofa-buffers-corelib-c-cpp`; every route exposes the
same target `sofa-buffers::corelib` and `#include <sofab/…>`.

**CMake** (`FetchContent`):

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

**Conan** (installed package, then in CMake):

```cmake
find_package(sofa-buffers-corelib-c-cpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE sofa-buffers::corelib)
```

**Arduino / PlatformIO** — the corelib ships an [Arduino](https://arduino.cc/)
manifest (`library.properties`) and a [PlatformIO](https://platformio.org/)
manifest (`library.json`).

## Why this design

| Goal | How |
|------|-----|
| No allocator | All state lives in caller-provided buffers and structs; nothing is heap-allocated. |
| No dependencies | No third-party libraries, so it drops into any toolchain. |
| Streaming **out** | `sofab_ostream` writes into a small caller buffer, invoking a flush callback when it fills — a message can exceed available RAM. |
| Streaming **in** | `sofab_istream` is a callback-driven decoder fed arbitrary byte chunks; large payloads arrive in pieces. |
| Reserve-offset | `sofab_ostream_init` takes a start offset leaving room for a lower-layer header (saves a copy). |
| Usable without a generator | The field-level API is explicit enough to write by hand; the descriptor-driven `object` API stays optional. |
| C++ without surprises | The wrapper reports errors through a sticky `Result` instead of throwing, and avoids `std::iostream`. |
| Portable | Plain C99 / C++20 with explicit endianness handling. |
| Small footprint | `SOFAB_DISABLE_*` switches drop whole code paths; `-Os` gets down to ~1&nbsp;KB of `.text` (see [Footprint](#footprint)). |

## Usage

The codec has four use cases — serialize a message that fits in one buffer,
serialize one too large for the buffer (streamed out in chunks), deserialize a
whole message, and deserialize one arriving in chunks — plus the generated-code
path that wraps them. The examples below use the **C core**; the header-only
**C++ wrapper** mirrors each through `sofab::OStreamInline` / `sofab::IStreamObject`
(the same `sofab::` surface as [`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp)).

### Serialize

Init a `sofab_ostream_t` over a buffer large enough for the whole message, write
fields, then read the byte count:

```c
#include "sofab/ostream.h"

uint8_t buf[64];
sofab_ostream_t os;
sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);

sofab_ostream_write_unsigned(&os, 1, 42);
sofab_ostream_write_signed(&os, 2, -7);
sofab_ostream_write_string(&os, 3, "hi");

size_t used = sofab_ostream_bytes_used(&os);   /* buf[0..used] holds the message */
```

### Serialize stream

For a message larger than the buffer, pass a flush callback. It fires whenever the
buffer fills (and on the final `sofab_ostream_flush()`), so a small scratch buffer
streams out an arbitrarily large message:

```c
static void on_flush(sofab_ostream_t *os, const uint8_t *data, size_t len, void *usrptr)
{
    int fd = *(int *)usrptr;
    write(fd, data, len);   /* stream the chunk out; the buffer is then reused */
}

uint8_t scratch[16];
sofab_ostream_t os;
sofab_ostream_init(&os, scratch, sizeof(scratch), 0, on_flush, &fd);

for (uint32_t i = 0; i < 1000; i++)
    sofab_ostream_write_unsigned(&os, i, i);

sofab_ostream_flush(&os);   /* flush the final partial buffer */
```

### Deserialize

Decoding is callback-driven: feed the bytes and, inside the callback, bind each
field to a destination with a `sofab_istream_read_*()` call. Unread fields are
skipped automatically.

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

### Deserialize stream

`sofab_istream_feed()` accepts arbitrarily small chunks and resumes at any byte
boundary, driving the same callback — so it decodes a piecewise stream identically
to a one-shot buffer, no matter where the chunks come from:

```c
struct my_msg msg = {0};
sofab_istream_t is;
sofab_istream_init(&is, on_field, &msg);

for (size_t i = 0; i < used; i++)   /* feed whatever arrives — here one byte at a time */
    sofab_istream_feed(&is, &buf[i], 1);
/* a field split across feeds is buffered internally and resumed */
```

The feed outcome is **three-valued**, with no separate finalize step:

| return | meaning |
|---|---|
| `SOFAB_RET_OK` | the bytes consumed so far end exactly on a field boundary — a complete message |
| `SOFAB_RET_INCOMPLETE` | the bytes end inside a field (a partial varint, or a fixlen/array payload shorter than declared) or with an open sequence — a **valid but partial** decode, not an error. Feed more bytes to continue; the caller owns end-of-input |
| `SOFAB_RET_E_INVALID_MSG` | the bytes are malformed regardless of what follows (varint too wide, length/count/id over the limit, bad type/subtype, …) |

Truncated input is therefore reported as `SOFAB_RET_INCOMPLETE` — it is neither
silently accepted as complete nor rejected as invalid. In the C++ wrapper the same
three outcomes are surfaced by `IStream::feed`'s `Result`: `ok()` (complete),
`incomplete()` (partial), and `code() == Error::InvalidMessage` (malformed).

Sequences may nest at most `SOFAB_MAX_DEPTH` (255) levels deep; a message that
exceeds this is rejected with `SOFAB_RET_E_INVALID_MSG`. The limit is enforced on
the skip path — nested sequences the caller does not bind with
`sofab_istream_read_sequence()` — where the depth counter is a `uint8_t`.
Actively-decoded nesting is instead bounded by the number of caller-provided
decoder handles.

### Code generator

`sofabgen` is the schema compiler. For **C** it targets the descriptor-driven
`object` API, emitting a plain struct plus `_encode` / `_decode` functions so one
generic transcoder serves every message and keeps flash small:

```c
#include "message/point.h"   /* generated by: sofabgen --lang c */

/* encode */
message_Point_t out;
message_point_init(&out);              /* apply schema defaults */
out.x = 3; out.y = 4;
uint8_t buf[MESSAGE_POINT_MAX_SIZE];
size_t used;
message_point_encode(&out, buf, sizeof(buf), &used);

/* decode */
message_Point_t in;
message_point_init(&in);
message_point_decode(&in, buf, used);  /* in.x == 3, in.y == 4 */
```

For **C++** (`sofabgen --lang cpp`) it emits a struct deriving
`sofab::OStreamMessage` / `sofab::IStreamMessage` with `encode()` / `decode()`
helpers over the same wire format:

```cpp
Point pt; pt.x = 3; pt.y = 4;
std::vector<uint8_t> wire = pt.encode();
Point got = Point::decode(wire.data(), wire.size());   // got.x == 3, got.y == 4
```

## Memory handling

Buffer ownership differs from a copy-on-read decoder, so it is worth being
precise. The **C core never allocates** — every context, decoder and buffer is
caller-provided. (The C++ wrapper is heap-free too with inline buffers /
`FixedString` / `FixedBytes` / `InlineVector`; only `OStream(buflen)` and the
`std::string` / `std::vector` read overloads use the heap.)

**Encode (ostream) — output buffer is caller-provided; the core never
allocates.** `sofab_ostream_init()` takes a writable buffer the stream never
allocates, copies, or frees — it just advances a cursor. `offset` reserves room
at the front for a lower-layer header. When the buffer fills (or on
`sofab_ostream_flush()`) an optional flush callback drains it; with no callback,
a full buffer returns `SOFAB_RET_E_BUFFER_FULL` instead of overflowing.

**Decode (istream) — deferred-copy binding.** A `read_*()` / `read()` call
copies nothing: it records only *where* the value goes (pointer, length, type).
The bytes are written into that destination by later `feed()` calls. Two rules
follow:

1. **Destinations must be address-stable and outlive decoding.** The pointer you
   bind is filled on a *later* `feed()`, so it cannot point at storage that moves
   or disappears first. Hence C++ `read(std::string&)` must be **pre-sized**;
   `FixedString` / `FixedBytes` / `InlineVector` are safe because their inline
   storage never moves.
2. **Data is copied into your memory, not aliased.** This port is **not**
   zero-copy: payload words are not guaranteed aligned on the wire, so decoded
   values are copied into your typed storage — keeping decode alignment- and
   endianness-safe and bounded to your buffers. Oversized or malformed fields are
   rejected with `SOFAB_RET_E_INVALID_MSG`; unbound fields are skipped untouched.

## Feature flags

The full wire format ships by default; the C core can be trimmed at compile time
by defining these macros (e.g. `-DSOFAB_DISABLE_ARRAY_SUPPORT`). Disabling unused
features removes their code paths and shrinks the footprint.

| Macro | Default | Effect |
| - | - | - |
| `SOFAB_DISABLE_FIXLEN_SUPPORT` | off | Drop fixed-length fields: floats, strings, and blobs |
| `SOFAB_DISABLE_ARRAY_SUPPORT` | off | Drop array fields (scalar arrays and fixed-length arrays) |
| `SOFAB_DISABLE_SEQUENCE_SUPPORT` | off | Drop nested sequence framing |
| `SOFAB_DISABLE_FP64_SUPPORT` | off | Drop 64-bit float (`fp64`); auto-defined where `double` is not 8 bytes |
| `SOFAB_DISABLE_INT64_SUPPORT` | off | Narrow scalar varints from 64-bit to 32-bit (drops the `u64`/`i64` helpers) |
| `SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` | off | Skip integer overflow checks when decoding (smaller/faster, less safe) |

The object API also selects an integer-width profile via
`SOFAB_OBJECT_DESCR_PROFILE` (`SOFAB_OBJECT_DESCR_SMALL` / `_MEDIUM` (default) /
`_BIG` = `uint8_t` / `uint16_t` / `uint32_t` descriptor members), and can be
dropped entirely with the `SOFAB_DISABLE_OBJECT_API` CMake option.

The C++ wrapper honors these switches: type-dispatch capabilities
(`FP64`/`INT64`/`ARRAY`) become a `static_assert` only when a disabled type is
used, while the structural ones (`FIXLEN`/`SEQUENCE`) underpin most of the C++
surface and are rejected outright with a `#error`.

> **`SOFAB_DISABLE_INT64_SUPPORT` — change only if you know what you are doing.**
> It shrinks 64-bit varint math (smaller/faster on 32-bit MCUs) but has wire- and
> API-level side effects:
> - **Wire compatibility:** the format is width-agnostic, so messages whose values
>   all fit in 32 bits stay byte-identical and interoperable. A value beyond 32-bit
>   range from a 64-bit peer is **rejected** as malformed
>   (`SOFAB_RET_E_INVALID_MSG`) — never silently truncated.
> - **ABI:** the value types appear in public signatures, so 32-bit and 64-bit
>   builds are **not** ABI-compatible — don't mix them.
> - **Field ids:** `SOFAB_ID_MAX` shrinks to `UINT32_MAX >> 3`, since the field
>   header is a varint of `(id << 3) | type`.
> - **Conformance:** the shipped test vectors include 64-bit values and won't
>   decode in this mode.

## Build & test

Build with CMake and a C99 / C++20 toolchain:

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests come in two layers:

- **Unit tests** (`test_c` / `test_cpp`) — hand-written C (Unity) and C++ (Catch2)
  tests covering encoder, decoder and object API, including the three-valued decode
  outcome (truncation → `SOFAB_RET_INCOMPLETE`) and error paths (over-wide varints,
  unbalanced sequences, overflow → `SOFAB_RET_E_INVALID_MSG`). Run in the
  full-feature ("max") build.
- **Conformance-vector suite** (`test_vectors_c`) — a language-agnostic set of
  vectors in [`assets/test_vectors.json`](assets/test_vectors.json), each pairing
  a message with its exact serialized bytes (format in
  [`assets/test_vectors_README.md`](assets/test_vectors_README.md); generator in
  [`test/vectorgen`](test/vectorgen)). This repo is the authoritative source of
  that file.

The vector suite also builds as a standalone, **feature-flag-tolerant** runner
(`sofab_vectortest`): each vector declares its `requires`, so a `SOFAB_DISABLE_*`
build skips the vectors it can't handle and reports `run` / `skipped` counts. CI
runs it across a [feature-flag](#feature-flags) matrix. To run a reduced config:

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

## Benchmarks

Two benchmark tools are built for both C and C++ (via `SOFAB_ENABLE_BENCH`, on by
default). The corelib is compiled into each at `-O3`, independent of the
size-optimized (`-Os`) library the rest of the project links.

| Tool | Measures | Use it for |
| - | - | - |
| `perf_c` / `perf_cpp` | intrinsic cost in **CPU cycles/operation** plus throughput | comparing algorithmic cost across changes or languages |
| `bench_c` / `bench_cpp` | practical **throughput in MB/s** on the current hardware | seeing real-world speed on a concrete target |

Convenience targets build and run them:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --build build --target run_bench            # throughput (MB/s), C and C++
cmake --build build --target run_perf             # per-op cost (cycles/op + MB/s)
cmake --build build --target run_bench_callgrind  # instructions/op under Callgrind (needs valgrind)
```

The Callgrind instruction-count report is machine-independent, so its C-vs-C++
numbers are directly comparable across machines.

### Footprint

Because the C core never allocates, its `.data`/`.bss` are `0.0KB` and the whole
cost is `.text` (flash). Tables below are the size of the built static library
(`size libsofabuffers.a`), always `-Os`; CI reproduces them.

**Full configuration**

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~2.7KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~2.8KB | 0.0KB | 0.0KB |
| RV32IMC | ~3.6KB | 0.0KB | 0.0KB |
| atmega8 | ~6.3KB | 0.0KB | 0.0KB |

**Minimal configuration** — `SOFAB_DISABLE_FIXLEN_SUPPORT`,
`SOFAB_DISABLE_ARRAY_SUPPORT`, `SOFAB_DISABLE_SEQUENCE_SUPPORT`,
`SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` and the `SOFAB_OBJECT_DESCR_SMALL` profile:

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

SofaBuffers ships two same-format C++ codecs for opposite ends of the spectrum.
This repo (`corelib-c-cpp`) is the **footprint / embedded** choice;
[`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp) is the **throughput**
choice.

- **`corelib-c-cpp` (this repo)** — a heap-free C99 object API plus a header-only
  C++20 wrapper over it. For small embedded devices where C is essential and every
  byte counts, and IoT-class C++ devices that forbid exceptions and
  `std::iostream`. Decoding is deferred-copy into caller-owned, address-stable
  storage.
- **`corelib-cpp`** — a from-scratch pure-C++20 implementation exposing the *same*
  `sofab::` surface, tuned for raw throughput on desktop/server. It reads
  immediately from a cursor over contiguous memory and can hand back zero-copy
  `std::string_view`s.

Pick this repo for bare-metal C, MCU-class C++, or a shared C/C++ wire format;
pick `corelib-cpp` on a hosted C++20 target that wants maximum speed.

Approximate head-to-head figures from the benchmark arena (best-of-5, comparable
only within a language):

| Use case | Library | vs. | Throughput | Bare-metal Cortex-M flash |
| - | - | - | - | - |
| Embedded **C** | `corelib-c-cpp` (C API) | nanopb | ~2× | ~3.5&nbsp;KB vs ~6.6&nbsp;KB |
| Embedded **C++** | `corelib-c-cpp` (C++ wrapper) | EmbeddedProto | ~2.1× | ~6.8&nbsp;KB vs ~9.4&nbsp;KB |
| Throughput **C++** | `corelib-cpp` (pure C++20) | protobuf | ~1.5× (~434-byte wire) | — (desktop/server) |

Both C++ ports share the `sofab::` API; the practical differences show up in the
decode-buffer contract:

| Topic | This library (C-backed) | `corelib-cpp` (pure C++20) |
| - | - | - |
| Decode-buffer lifetime | Deferred: `read()` *binds* the destination; bytes are filled by a later `feed()`, so destinations must be address-stable and outlive decoding | Immediate copy from a cursor over contiguous memory |
| `read(std::string &)` | Must **pre-size** the string; reads into existing storage | Auto-sizes via `assign` |
| `read(std::string_view &)` | Not available | Zero-copy read; the view aliases the source buffer |
| Sequence-of-varlen helpers | `read(std::vector<std::string>&)` and `read(std::vector<std::vector<uint8_t>>&)` | Not available |
| Heap-free field types | `FixedString<N>`, `FixedBytes<N>`, `InlineVector<T,N>` | — |
| Build-time capability flags | Honors the C core's `SOFAB_DISABLE_*` switches (`FP64`/`INT64`/`ARRAY` → `static_assert` on use; `FIXLEN`/`SEQUENCE` → hard `#error`) | None — every type always compiled in |
