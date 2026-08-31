<p align="center"><img src="assets/sofabuffers_logo.png" alt="SofaBuffers" height="140"></p>

# SofaBuffers

<b>Structured Objects For Anyone</b><br>
<i>... so optimized, feels amazing.</i>

[Would you like to know more?](https://github.com/sofa-buffers)

## SofaBuffers C/C++ library

[![CI](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/ci.yml)
[![C coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sofa-buffers/corelib-c-cpp/badges/coverage-c.json)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-online-blue)](https://sofa-buffers.github.io/corelib-c-cpp/)

[GitHub repository](https://github.com/sofa-buffers/corelib-c-cpp)

The footprint-optimized SofaBuffers (*Sofab*) codec: a **heap-free C99 object
API** plus a header-only **C++20 wrapper** (`sofab/sofab.hpp`) over the same C
core. It packs structured fields into a caller-owned buffer and decodes them
through a small, callback-driven streaming decoder — no allocator, no
dependencies, no code generator required. It runs from bare-metal MCUs (C) up to
IoT-class devices (the C++ wrapper), and is the authoritative source of the shared
conformance vectors in [`assets/test_vectors.json`](assets/test_vectors.json).

For desktop/server C++ where throughput beats code size, see the sibling
pure-C++20 port [`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp) and
[Choosing between the two C/C++ corelibs](#choosing-between-the-two-cc-corelibs).

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

| Target | Toolchain | Runs |
| - | - | - |
| x86_64 (little endian) | GCC and Clang | build + full test suite |
| AArch64 | GCC | build + test suite under QEMU |
| MIPS (big endian) | GCC | build + test suite under QEMU |
| PowerPC (big endian) | GCC | build + test suite under QEMU |
| RISC-V 64 (little endian) | GCC | build + test suite under QEMU |
| Cortex-M0/M3/M7/M23/M55 | arm-none-eabi | bare-metal build |
| RISC-V 32 (rv32i, rv32imc, rv32ec) | riscv64-unknown-elf | bare-metal build |
| RL78 | LLVM for RL78 | bare-metal build |
| AVR / ATmega | avr-gcc | bare-metal build, on demand |

Every target builds all four configurations — `full`, `full-strict`, `minimal`
and `minimal-noobj` — and reports separately. The CI badge above covers all of
them; the [CI run](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/ci.yml)
shows the per-target result.

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
| No allocator | The codec allocates nothing: every context, decoder and buffer is caller-provided. The C core has no heap at all; in C++ the heap appears only where you choose a growable destination. |
| No dependencies | No third-party libraries, so it drops into any toolchain. |
| Streaming **out** | `sofab_ostream` writes into a small caller buffer, invoking a flush callback when it fills — a message can exceed available RAM. |
| Streaming **in** | `sofab_istream` is a callback-driven decoder fed arbitrary byte chunks; large payloads arrive in pieces. |
| Reserve-offset | `sofab_ostream_init` takes a start offset leaving room for a lower-layer header (saves a copy). |
| Usable without a generator | The field-level API is explicit enough to write by hand; the descriptor-driven `object` API stays optional. |
| C++ without surprises | The wrapper reports errors through a sticky `Result` instead of throwing, and avoids `std::iostream`. |
| Portable | Plain C99 / C++20 with explicit endianness handling. |
| Small footprint | `SOFAB_DISABLE_*` switches drop whole code paths; `-Os` gets down to ~1&nbsp;KB of `.text` (see [Footprint](#footprint)). |

## Usage

Four use cases — serialize a message that fits one buffer, serialize one too large
for it, deserialize a whole message, deserialize one arriving in chunks — plus the
generated-code path that wraps them. The examples use the **C core**; the
header-only **C++ wrapper** mirrors each through `sofab::OStreamInline` /
`sofab::IStreamObject`.

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

A field is skipped just the same when the `read_*` bound for it does not match
the wire type (MESSAGE_SPEC §7.3): the decode continues and the destination keeps
its previous value. That is not an error — it is what lets peers on different
schema revisions keep talking. `sofab_istream_skipped()` counts it.

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
`incomplete()` (partial), and `invalid()` (malformed). It has a fourth,
`limitExceeded()`, for a receiver ceiling crossed on a schema-unbounded field —
see [Memory handling](#memory-handling).

Sequences nest at most `SOFAB_MAX_DEPTH` (255) deep; deeper is
`SOFAB_RET_E_INVALID_MSG`. The limit is enforced on the skip path, where the depth
counter is a `uint8_t`; actively-decoded nesting is bounded instead by the number
of caller-provided decoder handles.

A `fixlen` field's declared length and an array's element count are bounded by
`SOFAB_FIXLEN_MAX` / `SOFAB_ARRAY_MAX`; a message declaring more is rejected with
`SOFAB_RET_E_INVALID_MSG`, and so is a `sofab_ostream_write_*` call that asks for
more (`SOFAB_RET_E_ARGUMENT`). The wire format puts both ceilings at
2,147,483,647 and lets a constrained profile lower them to 65,535. **This port
takes that allowance where — and only where — `size_t` is too narrow to hold the
wider one:**

| target | `SOFAB_FIXLEN_MAX` / `SOFAB_ARRAY_MAX` |
| - | - |
| `size_t` 32-bit or wider (ARM, RISC-V, x86, …) | 2,147,483,647 |
| `size_t` 16-bit (AVR / ATmega) | 65,535 |

Both are derived from `SIZE_MAX` rather than listed per target, because the
decoder narrows the declared length and count into a `size_t`: a ceiling above
`SIZE_MAX` would admit a value the target cannot represent and truncate it
instead of rejecting it. Override either with `-DSOFAB_FIXLEN_MAX=…` if a profile
wants the constrained ceiling on a wide target; a value that does not fit in
`size_t` fails the build.

### Code generator

`sofabgen` is the schema compiler. For **C** it targets the descriptor-driven
`object` API — a plain struct plus `_encode` / `_decode`, so one generic
transcoder serves every message and flash stays small:

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

For **C++** (`sofabgen --lang cpp`) it emits a struct deriving `sofab::Message`
(the `sofab::OStreamMessage` + `sofab::IStreamMessage` pair) with `encode()` /
`decode()` helpers over the same wire format:

```cpp
Point pt; pt.x = 3; pt.y = 4;
std::vector<uint8_t> wire = pt.encode();
Point got = Point::decode(wire.data(), wire.size());   // got.x == 3, got.y == 4
```

### Feature flags

The full wire format ships by default; the C core can be trimmed at compile time.
What each switch is worth in bytes is measured under [Footprint](#footprint). A
switch also **narrows what the build accepts on the wire** — read the callout
below the table before enabling one.

**Prefer the CMake option over the bare macro.** Every switch except the one
marked *macro only* is a CMake option that `src/CMakeLists.txt` applies `PUBLIC`,
so the library and every consumer of its headers are configured identically. Two
of these macros change a public struct's layout, so a library and a consumer that
disagree is a silent ABI mismatch, not a warning. Passing the macro through
`CMAKE_C_FLAGS` also breaks on the cross-toolchain files under `utils/`, which
`set()` that variable and discard the command line.

| Switch | Set with | Default | Effect |
| - | - | - | - |
| `SOFAB_DISABLE_FIXLEN_SUPPORT` | CMake option | off | Drop fixed-length fields: floats, strings, and blobs |
| `SOFAB_DISABLE_ARRAY_SUPPORT` | CMake option | off | Drop array fields (scalar arrays and fixed-length arrays) |
| `SOFAB_DISABLE_SEQUENCE_SUPPORT` | CMake option | off | Drop nested sequence framing |
| `SOFAB_DISABLE_LAZY_SEQ_SUPPORT` | CMake option | off | Drop the hold-back sequence openers (`..._begin_lazy` / `..._end_keep`) and the pending-run state in `sofab_ostream_t`. Takes back 276&nbsp;B of ARMv6-m `.text`, 36&nbsp;B of RAM per stream and 6&nbsp;Ir per typical encode — see [Sequence framing](#sequence-framing-and-the-hold-back-window); a pure-C consumer encoding through `sofab_object_encode()` never needs them. **Changes the `sofab_ostream_t` layout** (and is rejected by the C++ wrapper) |
| `SOFAB_DISABLE_FP64_SUPPORT` | CMake option | off | Drop 64-bit float (`fp64`); auto-defined where `double` is not 8 bytes |
| `SOFAB_DISABLE_INT64_SUPPORT` | CMake option | off | Narrow scalar varints from 64-bit to 32-bit (drops the `u64`/`i64` helpers) |
| `SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` | CMake option | off | Skip integer overflow checks when decoding (smaller/faster, less safe) |
| `SOFAB_DISABLE_OBJECT_API` | CMake option | off | Exclude the descriptor-driven object API (`object.c`) and leave the bare stream corelib |

> **A switch that removes a wire construct makes the decoder *reject* messages
> that carry it.** `SOFAB_DISABLE_FIXLEN_SUPPORT`, `_ARRAY_`, `_SEQUENCE_`,
> `_FP64_` and `_INT64_` take the construct out of the format this build speaks,
> not just out of the API: such a message is `SOFAB_RET_E_INVALID_MSG`, terminally
> — every later `sofab_istream_feed` returns it and no callback fires, until
> `sofab_istream_init` resets the context.
>
> **That includes fields you never asked for.** The decoder is schema-agnostic, so
> an array in an unknown id is refused just like one in a field you read. A full
> build skips both ([MESSAGE_SPEC §7.3]); a reduced build cannot, because the code
> that steps over the construct is the code the switch removed. Keeping it —
> parsing only in order to skip — costs about 375&nbsp;B of ARMv6-m `.text` in the
> *Minimal* profile.
>
> **The switch is therefore an interop bound.** Such a build only talks to peers
> that never emit the construct, in any field — including fields a later schema
> revision adds. Enable it where both ends of the link are yours and the wire
> profile is fixed, not on a receiver exposed to messages you do not control.
>
> Nothing is silently dropped, but the rejection lands *mid-message*: fields
> before the offending one have already reached your callback.
> `SOFAB_RET_E_INVALID_MSG` condemns the whole message — discard the partially
> filled destinations.

[MESSAGE_SPEC §7.3]: https://github.com/sofa-buffers/documentation/blob/main/MESSAGE_SPEC.md

> **`SOFAB_DISABLE_LAZY_SEQ_SUPPORT` changes the wire output rather than removing
> a wire type.** An all-default sequence field is framed empty instead of omitted
> (MESSAGE_SPEC §2) — well-formed, same value, accepted by every decoder, but no
> longer canonical. It drops an encoder mechanism `sofab_object_encode()` never
> uses, so it is a footprint switch for pure-C consumers; the C++ wrapper rejects
> it with an `#error`.

Two switches **tune** rather than remove:

| Switch | Set with | Default | Effect |
| - | - | - | - |
| `SOFAB_LAZY_SEQ_DEPTH` | **macro only** | `8` | How many nested sequence headers can be held back at once — this profile's **documented hold-back bound**, see [Sequence framing](#sequence-framing-and-the-hold-back-window). Costs 4&nbsp;B of RAM per output stream per level; must be **1…255** (the run counter is a `uint8_t`, and a build outside that range is rejected with an `#error`) |
| `SOFAB_OBJECT_DESCR_PROFILE` | CMake cache variable | `SOFAB_OBJECT_DESCR_MEDIUM` | Integer width of the object descriptor's members: `SOFAB_OBJECT_DESCR_SMALL` / `_MEDIUM` / `_BIG` = `uint8_t` / `uint16_t` / `uint32_t`. It sizes the **descriptor tables in your code**, not the library — the library's own `.text` barely moves and `SMALL` even costs a few bytes there (see [Footprint](#footprint)). Also in a public header, hence `PUBLIC` |

Two knobs are **opt-IN** (off by default in this footprint corelib — see below):

| Switch | Set with | Default | Effect |
| - | - | - | - |
| `SOFAB_ENABLE_STRICT_UTF8` | CMake option | off | Enable strict UTF-8 validation of `string` fields (see below); off by default so the validator costs zero `.text`/`.rodata`. Resolves to the boolean `SOFAB_STRICT_UTF8`, which a direct `-DSOFAB_STRICT_UTF8=1` sets outright and wins over both knobs; the legacy `SOFAB_DISABLE_STRICT_UTF8` still forces it off |
| `SOFAB_ENABLE_SKIP_COUNTER` | CMake option | off | Count fields skipped because their wire type contradicted the read bound for them (§7.3), readable with `sofab_istream_skipped()`; a pure diagnostic no decode path reads, costing 18&nbsp;B of `.text` when on. Resolves to `SOFAB_SKIP_COUNTER`, which `-DSOFAB_SKIP_COUNTER=1` sets outright |

**Strict UTF-8 (`SOFAB_STRICT_UTF8`, off by default).** This is a
footprint/embedded corelib, so the strict UTF-8 check **defaults OFF** — the
constrained-profile allowance in
[CORELIB_PLAN §6.4](https://github.com/sofa-buffers/documentation/blob/main/CORELIB_PLAN.md)
(a documented non-strict build; the validator, `utf8.c`, and every gated site
compile to nothing, for zero `.text`/`.rodata` cost). **With the check OFF a
`string`'s wire bytes are stored verbatim (byte-container behavior), never
lossy** — invalid bytes are neither validated nor silently replaced/dropped.
Opt in with `-DSOFAB_ENABLE_STRICT_UTF8` (or a direct `-DSOFAB_STRICT_UTF8=1`);
conformance/fuzzer builds and this repo's CI enable it explicitly.

A `string` is UTF-8
([MESSAGE_SPEC §8](https://github.com/sofa-buffers/documentation/blob/main/MESSAGE_SPEC.md));
`blob` is the type for opaque bytes. With the check **on**, a `string` whose
bytes are not valid UTF-8 is rejected **symmetrically**: decoding a
*materialized* (read, not skipped) invalid string yields `SOFAB_RET_E_INVALID_MSG`,
and encoding one with `sofab_ostream_write_fixlen(..., SOFAB_FIXLENTYPE_STRING)`
yields `SOFAB_RET_E_ARGUMENT`. The validator (`sofab_utf8_valid`) is a real one —
it rejects overlong encodings (incl. `C0 80`), surrogates, and code points above
`U+10FFFF`, while allowing embedded `U+0000`. It is a **validation policy, never a
wire-format switch** (it only decides accept-vs-reject and is never lossy), so
peers with different settings interoperate on all valid data.

The C++ wrapper honors these switches: type-dispatch capabilities
(`FP64`/`INT64`/`ARRAY`) become a `static_assert` only when a disabled type is
used, while the structural ones (`FIXLEN`/`SEQUENCE`/`LAZY_SEQ`) underpin most of
the C++ surface and are rejected outright with a `#error`.

> **`SOFAB_DISABLE_INT64_SUPPORT` has wire- and API-level side effects.** It
> shrinks 64-bit varint math, smaller and faster on 32-bit MCUs, but:
> - **Wire:** values that fit in 32 bits stay byte-identical; a wider value from a
>   64-bit peer is **rejected** (`SOFAB_RET_E_INVALID_MSG`), never truncated.
> - **ABI:** the value types are in public signatures — 32-bit and 64-bit builds
>   are not ABI-compatible.
> - **Field ids:** `SOFAB_ID_MAX` shrinks to `UINT32_MAX >> 3`.
> - **Conformance:** the shipped vectors carry 64-bit values and will not decode.

### Sequence framing and the hold-back window

A nested structure is a **sequence**: a start marker, the child fields, an end
marker. Whether that frame reaches the wire at all depends on the *position*
([MESSAGE_SPEC §2](https://github.com/sofa-buffers/documentation/blob/main/MESSAGE_SPEC.md)):

- A sequence-typed **field** equal to its declared default is **omitted entirely**,
  not framed empty. So an all-default message encodes to **zero bytes**, and a
  zero-byte input decodes to the all-default value.
- A wrapper-array **element** is **always framed**, even when all-default: element
  presence carries a dynamic array's length (§5.1).
- An empty frame stays **valid input** — the non-canonical encoding of the same
  value, which every decoder normalizes away.

The two message layers here reach that outcome differently:

- **C, `sofab_object_encode()`** — the descriptor knows the whole object, so it
  tests each field against its default *before* opening anything: no window, no
  bound, canonical at every depth.
- **C++ / generated code** — the predicate is spread across individual writes, so
  the output stream decides. `..._write_sequence_begin_lazy()`
  (`OStream::sequenceBeginLazy`) **holds the header back**; the first field written
  into any enclosing sequence emits the whole run; `..._sequence_end()` drops a
  frame that never got content, `..._sequence_end_keep()` forces one out for an
  element position. Held-back ids are stream state, not buffer content, so a small
  output buffer still yields identical bytes.

> **The bound: `SOFAB_LAZY_SEQ_DEPTH` (default 8).** This is a **heap-free**
> corelib — it cannot grow the pending run on demand, so it bounds it, which
> [CORELIB_PLAN §6](https://github.com/sofa-buffers/documentation/blob/main/CORELIB_PLAN.md)
> permits a heap-free profile *provided the bound is documented*. In practice:
>
> - **Up to 8 nested lazily-opened sequences, output is canonical** — an
>   all-default sequence field is omitted.
> - **Deeper than that the run is committed and the sequence is framed eagerly.**
>   An all-default one then leaves a two-byte empty frame on the wire —
>   well-formed and the same value, so a bounded and an unbounded encoder
>   interoperate; what it is not is byte-identical.
> - Raise it with `-DSOFAB_LAZY_SEQ_DEPTH=N` if a schema nests sequence *fields*
>   deeper than 8 and byte-exact canonical output matters; each level costs
>   4&nbsp;B of RAM per stream. Nesting itself is bounded by `SOFAB_MAX_DEPTH`
>   (255), and exceeding the hold-back window is never an error.
>
> Corelibs that can allocate hold back to the full `SOFAB_MAX_DEPTH` and are
> canonical at every depth. The C object API above already is; only the raw-stream
> path carries the window.

**What it costs.** The commit check sits in the one function every writer funnels
through, so a default build pays it *per field* — including a pure-C consumer that
never opens a lazy sequence. Callgrind, `bench_c` at `-O3`, against the same build
with `-DSOFAB_DISABLE_LAZY_SEQ_SUPPORT=ON`:

| Workload | default | `SOFAB_DISABLE_LAZY_SEQ_SUPPORT` | delta |
| - | -: | -: | -: |
| encode: typical message | 966 Ir/op | 960 Ir/op | **+6 Ir/op (+0.6 %)** |
| encode: composite | 16 164 Ir/op | 16 069 Ir/op | +95 Ir/op (+0.6 %) |
| encode: u64 array (1000) | 125 999 Ir/op | 125 998 Ir/op | +1 Ir/op (+0.001 %) |
| decode: typical message | 2 109 Ir/op | 2 109 Ir/op | none |

The cost is per *field header*, so a message of many small fields feels it and one
of few wide ones does not; decoding is untouched. With the 276&nbsp;B of ARMv6-m
`.text` and 36&nbsp;B of per-stream RAM under [Footprint](#footprint), that is the
whole price, and `SOFAB_DISABLE_LAZY_SEQ_SUPPORT` takes all of it back for a build
that only uses the descriptor-driven encoder.

`bench_c` measures the stream path with the eager opener, so these deltas are the
per-write commit check alone; the hold-back's own work is what the `encode:
composite` row adds, that being the message with an omitted all-default field.

## Memory handling

The **C core never allocates** — every context, decoder and buffer is
caller-provided. The C++ wrapper is heap-free too with inline buffers /
`FixedString` / `FixedBytes` / `InlineVector`; only `OStream(buflen)` and the
`std::string` / `std::vector` read overloads use the heap.

**Encode (ostream) — the buffer is caller-provided.** `sofab_ostream_init()` takes
a writable buffer the stream never allocates, copies or frees; it advances a
cursor. `offset` reserves room at the front for a lower-layer header. When the
buffer fills (or on `sofab_ostream_flush()`) an optional flush callback drains it;
with no callback a full buffer returns `SOFAB_RET_E_BUFFER_FULL`.

**`SOFAB_MIN_OUTPUT_BUFFER` is `1`.** The smallest buffer the encoder accepts
*for streaming*: every write goes through a byte-at-a-time push, so a field
header, a `fixlen_word`, a varint value or a float element may each straddle a
flush. A buffer installed **with** a flush callback must satisfy
`buflen - offset >= SOFAB_MIN_OUTPUT_BUFFER`; one installed **without** has no
minimum at all — it either holds the message or reports
`SOFAB_RET_E_BUFFER_FULL`, so a caller sizing from a generated `MAX_SIZE` gets an
exact fit.

**No pass-through.** A flush callback is **only ever handed the output buffer it
was installed with**, never foreign memory — on every flush of every message. A
`string` or `blob` payload is copied through that buffer like anything else,
however large it is.

**Decode (istream) — deferred-copy binding.** A `read_*()` / `read()` call copies
nothing: it records only *where* the value goes. Later `feed()` calls write into
that destination. Two rules follow:

1. **Destinations must be address-stable and outlive decoding**, since the pointer
   is filled on a later `feed()`. C++ `read(std::string&)` must therefore be
   **pre-sized**; `FixedString` / `FixedBytes` / `InlineVector` are safe, their
   inline storage never moving.
2. **Data is copied into your memory, not aliased.** Payload words are not
   guaranteed aligned on the wire, so values are copied into your typed storage —
   alignment- and endianness-safe, bounded to your buffers. Oversized or malformed
   fields are `SOFAB_RET_E_INVALID_MSG` — in the C++ wrapper an oversized field
   has three possible answers, one per ceiling it broke, in the table below;
   unbound fields are skipped untouched.

A fed chunk is **borrowed only for the duration of `feed()`** and may be reused
the moment it returns; what a bound destination has not received yet is carried
in the stream context, not in library-owned heap memory — there is none.

**The C API has no unbounded field, so it carries no receiver caps.** Every C
read is handed a destination that already exists together with its size:
`sofab_istream_read_string(ctx, var, varlen)`, `..._read_blob(ctx, var, varlen)`
and `..._read_array(ctx, var, element_count, element_size, opt)`. The object API
is the same model expressed as descriptors — `SOFAB_OBJECT_FIELD_ARRAY` derives
its element count from `sizeof(field) / sizeof(field[0])` and a blob or string
field its length from `sizeof(field)`. There is therefore no shape in which a
field arrives at the C decoder without a bound, and a wire length or count above
that bound is `SOFAB_RET_E_INVALID_MSG`. `SOFAB_RET_E_LIMIT_EXCEEDED` is declared
in `sofab.h` because `sofab_ret_t` is the shared C/C++ code table, and **no C API
function returns it**.

**The C++ wrapper does have unbounded fields, and both bounds for them are
yours.** A `std::string`, `std::vector` or growable wrapper-array destination has
no capacity of its own, so a field whose schema declares no `maxlen` / `count`
would otherwise be sized by the wire alone. Neither number is this library's to
hold: CORELIB_PLAN §6.2.1 gives the receiver caps to generated code — "the
visitor decides. The codec never invents a limit of its own and never clamps to
one" — and MESSAGE_SPEC §7 gives it the schema bounds, because "the corelib
cannot know the schema". What this library contributes is the **report and the
category**: the `size` / `count` your handler already receives, and a verdict it
can set.

**You state the number; the read performs the comparison.** §6.2.1 fixes the
*provenance* of a limit, not the *site* of the check — "a corelib **MAY** take a
limit as an argument and perform the check itself, and a port that does is
conformant" — and this library does, because the check has to run **behind** the
MESSAGE_SPEC §7.3 tag test (a field whose wire type contradicts the read is
skipped, and a skipped field is never capped) and **before** anything is sized
from the wire. Only the read can see both. Handing the number in makes that a
property of the structure rather than of every caller's discipline.

Each of `readString` / `readBlob` / `readArray` therefore has **two entry
points**, and which one a field uses is decided by which ceiling the schema left
it:

```cpp
void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t count) noexcept override
{
    // the schema declares a maxlen/count -> the plain read takes it, and its
    // violation is InvalidMessage (MESSAGE_SPEC §7.1). A receiver cap must not
    // reach a field the schema already bounds (§6.2.1).
    if (id == 1) is.readString(name, size, SCHEMA_MAXLEN);

    // the schema declares nothing -> the …Capped read takes the receiver cap,
    // and its breach is LimitExceeded: well-formed bytes this deployment
    // declines to hold.
    if (id == 2) is.readStringCapped(note, size, SOFAB_MAX_DYN_STRING_LEN);
    if (id == 3) is.readArrayCapped(vals, count, SOFAB_MAX_DYN_ARRAY_COUNT);

    // an inline destination publishes a capacity of its own, which is a ceiling
    // the sender cannot move, so it needs neither.
    if (id == 4) is.readString(tag, size);      // tag is a FixedString<N>
}
```

The cap parameter is **required and unsigned**, so "no cap" has no spelling: §6.2.1
says a codec "**MUST NOT** read an omitted argument as *unlimited*", and a
`dynCap = -1` default is exactly that. A read handed **no ceiling at all** — no
schema bound, no cap, and a growable destination — is refused as
`sofab::Error::InvalidArgument` rather than decoded: nothing is wrong with the
bytes and no limit was configured to raise, so what is missing is part of the
**call**. State one of the two ceilings, or bind an inline destination.

The same split applies to a **growable wrapper array**, which announces no count
and delivers no callback at the element index — there is no point at which your
handler could check, so its collector carries the numbers you set and applies them
to the element **index**, before the container is extended. Its collector is the
static helper layer (§6.6.1) rather than the codec: the generated layer owns it,
and no codec path calls it.

```cpp
seq.cap = -1;                                                   // the schema declares no count ...
seq.dynCap     = sofab::DynCap{SOFAB_MAX_DYN_ARRAY_COUNT};      // ... so this bounds the element index
seq.dynElemMax = sofab::DynCap{SOFAB_MAX_DYN_STRING_LEN};       // and this the element length
```

`sofab::DynCap` is why an omitted member cannot mean *unlimited*: a
default-constructed one is **unstated**, and reaching it on a schema-unbounded
field is `InvalidArgument`, not an uncapped decode. It cannot be built from a
signed value either, so the old `-1` cannot re-enter as `SIZE_MAX`.

A refused field ends the decode, and which answer `feed()` returns says what was
wrong:

| what was exceeded | code | who applies it |
| - | - | - |
| a `maxlen` / `count` the schema declares | `sofab::Error::InvalidMessage` | the read, from the bound you passed |
| a receiver cap, on a field the schema leaves unbounded | `sofab::Error::LimitExceeded` | the `…Capped` read, from the cap you passed |
| neither — the destination handed over is too short | `sofab::Error::InvalidArgument` | the library (§6.6.3) |
| neither — **no ceiling was stated at all** | `sofab::Error::InvalidArgument` | the library (§6.2.1) |

The last two rows are the only bounds this library judges without a number from
you, and neither is a limit: one is the size of the storage you handed over, the
other is the absence of any ceiling to judge by. Both are refused rather than
grown into or truncated to.

All of them are terminal. A field the handler skips is never capped.

## Build & test

Build with CMake and a C99 / C++20 toolchain:

```sh
cmake -S . -B build
cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure
```

Three suites run under CTest:

- **`test_c` / `test_cpp`** — Unity and Catch2 unit tests over encoder, decoder and
  object API, including the three-valued decode outcome and the error paths. Built
  in the full-feature ("max") configuration only.
- **`test_vectors_c`** — the shared conformance vectors in
  [`assets/test_vectors.json`](assets/test_vectors.json), each pairing a message
  with its exact serialized bytes (format in
  [`assets/test_vectors_README.md`](assets/test_vectors_README.md), generator in
  [`test/vectorgen`](test/vectorgen)). **This repo is the authoritative source of
  that file** for the whole family.
- **`test_readme_structure`** — reads this README and holds it to CORELIB_PLAN §9:
  the section list and order, the badge block, the §9.5 examples, the §6.4/§9.6
  facts, the §6.1.1 name set, and that every in-document link resolves.

The vector suite also builds standalone and **feature-flag-tolerant**
(`sofab_vectortest`): each vector declares its `requires`, so a `SOFAB_DISABLE_*`
build skips what it cannot handle and reports `run` / `skipped` counts. CI runs it
across a [feature-flag](#feature-flags) matrix:

```sh
cmake -S . -B build -DSOFAB_ENABLE_CPP=OFF -DSOFAB_DISABLE_INT64_SUPPORT=ON
cmake --build build --target sofab_vectortest   # the unit tests are max-only
./build/test/c/sofab_vectortest
```

### Useful CMake options

Build-shaping options; the wire-feature switches are listed under
[Feature flags](#feature-flags) and are CMake options too.

| Option | Default | Description |
| - | - | - |
| `SOFAB_BUILD_TESTS` | `ON` | Build the C/C++ test suites (off for a package build — it skips the Unity/Catch2 `FetchContent`) |
| `SOFAB_ENABLE_CPP` | `ON` | Build the C++ tests |
| `SOFAB_ENABLE_CPP_SMOKE` | `OFF` | Build the Catch2-free C++ wrapper smoke test (for reduced configs) |
| `SOFAB_ENABLE_BENCH` | `ON` | Build the benchmarks (`bench_c`/`bench_cpp`, `perf_c`/`perf_cpp`) |
| `SOFAB_ENABLE_COVERAGE` | `OFF` | Enable code coverage instrumentation (`-O0 -g --coverage`) |
| `SOFAB_ENABLE_FUZZ` | `OFF` | Enable fuzzing instrumentation (sanitizers) |
| `SOFAB_ENABLE_DOXYGEN` | `OFF` | Build the `doc` target (API documentation) |
| `SOFAB_ENABLE_VECTORGEN` | `OFF` | Build the JSON test-vector generator (see `test/vectorgen`) |
| `SOFAB_INSTALL` | `ON` | Generate the install and CMake package-config rules (turn off when embedding via `add_subdirectory`) |

## Benchmarks

The three tools
[BENCH_SPEC](https://github.com/sofa-buffers/documentation/blob/main/BENCH_SPEC.md)
requires of every `corelib-*`, built for both C and C++ (`SOFAB_ENABLE_BENCH`, on
by default). The corelib is compiled into each at `-O3`, independent of the `-Os`
library the rest of the project links.

| Tool | Measures | Use it for |
| - | - | - |
| `bench_c` / `bench_cpp` | practical **throughput in MB/s** on the current hardware | seeing real-world speed on a concrete target |
| `perf_c` / `perf_cpp` | intrinsic cost in **CPU cycles/operation** plus throughput | comparing algorithmic cost across changes or languages |
| `bench/run_callgrind.sh` | **instructions retired per operation** (Callgrind `Ir/op`) | the number to quote: deterministic and independent of the host's clock and scheduler, so it compares across machines — and the signal a CI regression gate should use |

Convenience targets build and run them:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
cmake --build build --target run_bench            # throughput (MB/s), C and C++
cmake --build build --target run_perf             # per-op cost (cycles/op + MB/s)
cmake --build build --target run_bench_callgrind  # instructions/op under Callgrind (needs valgrind)
```

All three run BENCH_SPEC's shared datasets, so the numbers compare directly
against every other port: a 1000-element `u64` array, the 7-field `typical`
message (37 B), the 12-field `perf` message (170 B), an unbounded **1 MB blob**
(one-shot, streamed through a 4096-byte buffer with a flush sink, and decoded in
4096-byte chunks) and the **composite** message (956 B), whose wrapper array,
non-ASCII UTF-8, depth-3 nesting, omitted all-default field and two-byte field
header reach paths the flat datasets never touch. The last two encoded sizes
(1,000,005 and 956 B) are cross-port parity checks; `bench_c` refuses to print a
table if the first does not hold.

Read the `blob 1MB` rows only in `Ir/op`: five of that message's bytes are
metadata and a million are payload, so its MB/s is the machine's memory bandwidth.
They are also the only rows that exercise the divisible-run path at all. There is
no `encode: blob 1MB passthrough` row, in this port or any other: pass-through was
withdrawn, and a payload run is always copied through the installed output buffer
(see [Memory handling](#memory-handling)).

### Footprint

Because the C core never allocates, `.data`/`.bss` are `0.0KB` and the whole cost
is `.text` (flash). The tables are `size libsofabuffers.a` at `-Os`, regenerated by
[`tools/footprint.sh`](tools/footprint.sh). CI builds one job per architecture and
configuration; `atmega8` is the exception, its workflow being on manual dispatch
until the test image links again.

**Full configuration**

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~3.7KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~3.8KB | 0.0KB | 0.0KB |
| RV32IMC | ~4.8KB | 0.0KB | 0.0KB |
| atmega8 | ~8.1KB | 0.0KB | 0.0KB |

**Full configuration, strict UTF-8 on** — the only rows where the validator
(`utf8.c`) is compiled in. The delta over *Full* above is its entire
`.text`/`.rodata` cost (~0.2–0.5&nbsp;KB), which the default build does not pay:

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~4.0KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~4.0KB | 0.0KB | 0.0KB |
| RV32IMC | ~5.1KB | 0.0KB | 0.0KB |
| atmega8 | ~8.6KB | 0.0KB | 0.0KB |

The [hold-back framing](#sequence-framing-and-the-hold-back-window) is part of
those *Full* rows: 276&nbsp;B on ARMv6-m, 512&nbsp;B on atmega8. A pure-C consumer
encoding only through `sofab_object_encode()` takes it back out with
`SOFAB_DISABLE_LAZY_SEQ_SUPPORT` — ARMv6-m returns to 3551&nbsp;B, `sofab_ostream_t`
shrinks from 56&nbsp;B to 20&nbsp;B per stream, and a typical encode drops
6&nbsp;Ir/op. The *Minimal* rows below disable sequences outright and are
unaffected.

**Minimal configuration** — `SOFAB_DISABLE_FIXLEN_SUPPORT`,
`SOFAB_DISABLE_ARRAY_SUPPORT`, `SOFAB_DISABLE_SEQUENCE_SUPPORT`,
`SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` and the `SOFAB_OBJECT_DESCR_SMALL` profile:

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~1.0KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~1.1KB | 0.0KB | 0.0KB |
| RV32IMC | ~1.4KB | 0.0KB | 0.0KB |
| atmega8 | ~2.7KB | 0.0KB | 0.0KB |

Same minimal configuration, additionally without `object.c`
(`SOFAB_DISABLE_OBJECT_API`):

| Architecture | .text | .data | .bss |
| - | - | - | - |
| ARMv6-m | ~0.7KB | 0.0KB | 0.0KB |
| ARMv7-m+fp.dp | ~0.7KB | 0.0KB | 0.0KB |
| RV32IMC | ~0.9KB | 0.0KB | 0.0KB |
| atmega8 | ~1.8KB | 0.0KB | 0.0KB |

#### What each switch is worth

The four configurations above answer *how small can it get*. They do not answer
*what is this one switch worth*, which is the question you have when you need
exactly one feature gone. Each row below is the **full** configuration plus that
one switch on ARMv6-m — the smallest row above, and the one an MCU budget is
usually measured against. Same `-Os` build, same tool, and regenerated by the
same [`tools/footprint.sh`](tools/footprint.sh):

| Switch | `.text` | delta |
| - | -: | -: |
| *(full, the baseline)* | 3827&nbsp;B | — |
| `SOFAB_DISABLE_OBJECT_API` | 2254&nbsp;B | **−1573&nbsp;B** |
| `SOFAB_DISABLE_ARRAY_SUPPORT` | 2801&nbsp;B | **−1026&nbsp;B** |
| `SOFAB_DISABLE_SEQUENCE_SUPPORT` | 2984&nbsp;B | −843&nbsp;B |
| `SOFAB_DISABLE_FIXLEN_SUPPORT` | 3023&nbsp;B | −804&nbsp;B |
| `SOFAB_DISABLE_INT64_SUPPORT` | 3519&nbsp;B | −308&nbsp;B |
| `SOFAB_DISABLE_LAZY_SEQ_SUPPORT` | 3551&nbsp;B | −276&nbsp;B |
| `SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` | 3755&nbsp;B | −72&nbsp;B |
| `SOFAB_DISABLE_FP64_SUPPORT` | 3781&nbsp;B | −46&nbsp;B |
| `SOFAB_OBJECT_DESCR_PROFILE=…_BIG` | 3831&nbsp;B | +4&nbsp;B |
| `SOFAB_ENABLE_SKIP_COUNTER` | 3845&nbsp;B | +18&nbsp;B |
| `SOFAB_OBJECT_DESCR_PROFILE=…_SMALL` | 3847&nbsp;B | +20&nbsp;B |
| `SOFAB_ENABLE_STRICT_UTF8` | 4073&nbsp;B | +246&nbsp;B |

Three rows need a word:

- **The deltas do not add up to the *Minimal* rows.** They overlap — dropping
  arrays removes code dropping fixlen would also have removed — so a switch is
  worth *at most* its row here once another is on. The four configurations above
  are the measured combinations.
- **`SOFAB_OBJECT_DESCR_PROFILE` barely moves the library and `SMALL` makes it
  slightly bigger.** The profile sizes the descriptor members in **your** tables,
  which is where the saving lands; narrower members cost the library a few
  widening instructions. Choose it for the descriptors, not the corelib.
- **`SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK` buys 72&nbsp;B** by giving up a decode
  safety check. It is in *Minimal* because that profile targets trusted,
  schema-bounded links; it is a poor trade on untrusted input.

### Choosing between the two C/C++ corelibs

SofaBuffers ships two same-format C++ codecs for opposite ends of the spectrum.

- **`corelib-c-cpp` (this repo)** — the **footprint / embedded** choice: a
  heap-free C99 object API plus a header-only C++20 wrapper. For MCUs where C is
  essential and every byte counts, and IoT-class C++ that forbids exceptions and
  `std::iostream`. Decoding is deferred-copy into caller-owned, address-stable
  storage.
- **[`corelib-cpp`](https://github.com/sofa-buffers/corelib-cpp)** — the
  **throughput** choice: pure C++20 exposing the same `sofab::` surface, parsing
  in place over the caller's buffer, so no destination has to survive a chunk.
  Neither port has a borrowing destination.

Pick this repo for bare-metal C, MCU-class C++ or a shared C/C++ wire format;
pick `corelib-cpp` on a hosted target that wants maximum speed.

#### What the speed difference actually is

Instruction counts from the shared Callgrind tooling — deterministic and
machine-independent, all three built at `-O3`; **lower is better**. This repo's
two columns were re-measured together on the current tree; the `corelib-cpp`
column is the reading published in
[its own README](https://github.com/sofa-buffers/corelib-cpp#instruction-counts-callgrind).

| Workload | C (this) | C++ wrapper (this) | `corelib-cpp` |
| - | -: | -: | -: |
| encode: u64 array (1000) | 125 999 | 126 028 | **35 046** |
| encode: typical message | 966 | 1 063 | **226** |
| encode: blob 1MB one-shot | 10 000 162 | 10 000 191 | **1 000 026** |
| encode: blob 1MB streaming | **10 004 819** | 10 009 790 | 13 009 127 |
| encode: composite | 16 164 | 16 501 | **11 514** |
| decode: u64 array (1000) | 300 432 | 300 438 | **43 839** |
| decode: typical message | 2 109 | 2 113 | **1 275** |
| decode: blob 1MB | 25 011 323 | 25 011 331 | **3 654 639** |
| decode: composite | 32 168 | 36 538 | **22 417** |
| decode: composite skip-all | 25 411 | 25 416 | **7 671** |

`corelib-cpp` runs **1.4× to 10× fewer instructions**, widest where a payload
moves in bulk: it establishes a varint window once and then moves whole 64-bit
words, and its one-shot `blob` write is a `memcpy` at one instruction per byte,
where this core pushes every payload byte through the same bounds-checked path
(~10 instructions per byte encoding, ~25 decoding) and keeps per-field bookkeeping
for its deferred-copy contract. That contract is the point of this repo, so the
gap is the deliberate trade.

**One row goes the other way.** On `encode: blob 1MB streaming` this port needs
**1.3× fewer** instructions (10.0 M against 13.0 M). `corelib-cpp` takes its
`memcpy` branch only while the run fits the output buffer and falls back to a
byte loop when it does not — a megabyte through 4096 bytes is entirely the
fallback. This core has no fast path to fall out of, so streaming costs it what
the one-shot write cost (+0.05 %).

Approximate head-to-head figures from the benchmark arena (best-of-5, comparable
only within a language):

| Use case | Library | vs. | Throughput | Bare-metal Cortex-M flash |
| - | - | - | - | - |
| Embedded **C** | `corelib-c-cpp` (C API) | nanopb | ~2.1× | ~3.6&nbsp;KB vs ~6.6&nbsp;KB |
| Embedded **C++** | `corelib-c-cpp` (C++ wrapper) | EmbeddedProto | ~2.3× | ~6.5&nbsp;KB vs ~9.3&nbsp;KB |
| Throughput **C++** | `corelib-cpp` (pure C++20) | protobuf | ~1.3× (434 vs 494-byte wire) | — (desktop/server) |

The `corelib-cpp` arena row predates several rounds of varint and hot-path work
there and has not been re-run, so read it as a floor rather than a current
figure. The two embedded rows are unaffected — this repo's codecs have not
changed.

Both C++ ports share the `sofab::` API; the practical differences show up in the
decode-buffer contract:

| Topic | This library (C-backed) | `corelib-cpp` (pure C++20) |
| - | - | - |
| Decode-buffer lifetime | Deferred: `read()` *binds* the destination; bytes are filled by a later `feed()`, so destinations must be address-stable and outlive decoding | Immediate: parses in place over the caller's buffer and copies each value out before `feed()` returns, so no destination has to survive the call |
| `read(std::string &)` | Must **pre-size** the string; reads into existing storage | Auto-sizes via `assign` |
| `read(std::string_view &)` | Not available | Not available either — deliberately, and asking for one is a compile error naming the owning alternatives |
| Wrapper-array collectors | `FixedStringSeq` / `FixedBlobSeq` / `StringSeq` / `BlobSeq` for string and blob elements, `FixedMessageSeq` / `MessageSeq` (in `sofab/seq.hpp`) for struct, union and row elements, plus `read(std::vector<std::string>&)` and `read(std::vector<std::vector<uint8_t>>&)` | `StringSeq` / `BlobSeq` / `MessageSeq`, same placement rule; the fixed-capacity twins are this port's, since the heap-free containers are what they fill |
| Heap-free field types | `FixedString<N>`, `FixedBytes<N>`, `InlineVector<T,N>` | The same three, deliberately identical in name and behaviour, so generated code for a bounded field is the same either way |
| Build-time capability flags | Honors the C core's `SOFAB_DISABLE_*` switches (`FP64`/`INT64`/`ARRAY` → `static_assert` on use; `FIXLEN`/`SEQUENCE` → hard `#error`) | None — every type always compiled in |
| Strict UTF-8 (§6.4) | **Off** by default (constrained-profile allowance), opt in with `SOFAB_ENABLE_STRICT_UTF8` | **On** by default |
