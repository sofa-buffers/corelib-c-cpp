# SofaBuffers test vectors

**[`test_vectors.json`](test_vectors.json)** (next to this file) is a
language-agnostic conformance suite for the SofaBuffers wire format. Each vector
pairs a **message structure + values** with the **exact serialized bytes**.

Another implementation (e.g. [`corelib-rs`](https://github.com/sofa-buffers/corelib-rs))
can load this file and, for every vector, either:

- re-encode the message from `fields` and assert the output equals `serialized.hex`, or
- decode `serialized.hex` and assert it recovers the values in `fields`.

That's all most consumers need — just read the JSON. It is machine-generated; to
rebuild it, see the generator in [`../test/vectorgen`](../test/vectorgen).

The file is **not only vectors.** Three further top-level blocks sit beside them —
[`invalid_utf8`](#negative-vectors--invalid_utf8), whose cases are keyed by a
byte string a valid encoder cannot produce,
[`sequence_growth`](#growth-cases--sequence_growth), whose cases are keyed by a
delivery sequence of element ids rather than by bytes at all, and
[`header_limits`](#header-ceiling-cases--header_limits), whose cases are keyed by
a **partial** byte string and carry a required **verdict**. A port runs every
block its `requires` gating does not exclude. All are backward-compatible: a
consumer that reads only `vectors` ignores them.

## File format

```jsonc
{
  "format": "sofabuffers-test-vectors",
  "version": 1,
  "vectors": [
    {
      "name": "fp32",
      "group": "scalar/float",
      "description": "...",
      "offset": 0,                       // start offset passed to the encoder
      "requires": ["fixlen"],            // OPTIONAL: capabilities this vector needs
      "skip_ids": [2, 4],                // OPTIONAL: field ids a receiver skips
      "fields": [                        // ordered encode operations (the structure + values)
        { "op": "fp32", "id": 0, "value": 3.1415 }
      ],
      "serialized": { "length": 6, "hex": "0220560e4940" },  // ground truth (dense)
      "serialized_sparse": { "length": 6, "hex": "0220560e4940" }  // same message, sparse-canonical
    }
  ]
}
```

### The two byte columns

Every vector carries both, and they answer different questions:

| column | what it is | who asserts it |
|---|---|---|
| `serialized` | **dense** — the `fields` ops replayed one-for-one through the primitive encoder. The authoritative ground truth, and the input for every decode/skip scenario. | this repo's corelib suite, and every other `corelib-*` |
| `serialized_sparse` | **sparse-canonical** ([MESSAGE_SPEC §2](https://github.com/sofa-buffers/documentation/blob/main/MESSAGE_SPEC.md)) — every leaf equal to its type default omitted, and a sequence left without content omitted rather than framed empty (so an all-default message is the **empty** byte string) | the **generator**'s per-language conformance drivers, which encode *generated objects* |

A corelib cannot produce the sparse form on its own: omission is decided against
each field's *declared default*, which lives in the schema, one layer above the
primitive API. So corelib suites assert `serialized` only — that is not a coverage
gap, it is the layer boundary. The two columns are equal whenever no field of the
vector is at its default.

### `fields` operations

| `op`              | Extra keys                                  | Meaning |
|-------------------|---------------------------------------------|---------|
| `unsigned`        | `id`, `value`                               | unsigned varint |
| `signed`          | `id`, `value`                               | zigzag signed varint |
| `boolean`         | `id`, `value` (`true`/`false`)              | boolean (unsigned 0/1) |
| `fp32` / `fp64`   | `id`, `value`                               | 32/64-bit float |
| `string`          | `id`, `value`                               | UTF-8 string |
| `blob`            | `id`, `value_hex`                           | binary blob |
| `array`           | `id`, `element_type`, `values`              | array; `element_type` ∈ `u8..u64`, `i8..i64`, `fp32`, `fp64` |
| `sequence_begin`  | `id`                                        | open a nested sequence |
| `sequence_end`    | `element` (optional, `true`)                | close the current sequence; `element` marks the closer of the element at a wrapper array's **last index** |

**`element` — the last element of a wrapper array.** The flag may appear on *any*
op, leaf or `sequence_end`, and marks the op at a wrapper array's **last element
index**. An op list carries no schema, so that position cannot be inferred, and it
is the one position the sparse column never drops (MESSAGE_SPEC §2/§5.1): a
wrapper carries no length, so the decoded length is *highest present id + 1* and
the last element is always written — a leaf as its (possibly default) value, a
sequence element as an **empty frame**. Every **interior** element equal to its
default is omitted instead, leaf and sequence element alike, leaving an id gap.
The flag matters solely for the `serialized_sparse` column; the dense pass writes
everything either way, so a driver that replays only `serialized` may ignore it.

The ops are the same for both byte columns; only the encoding differs. In the
`serialized_sparse` pass every default leaf op **that is not marked `element`** is
skipped and every `sequence_begin` is opened *lazily*, so a sequence whose ops all
dropped out carries no content and is omitted whole — header and end marker both,
unless its closer is marked `element`.

### Optional `requires`

A vector may carry a top-level `"requires": [..]` array naming the optional
library features it needs. The generator derives it automatically from the
vector's ops, values and ids, so it can't drift. A harness compiled without a
feature (a `SOFAB_DISABLE_*` flag) **skips** the vectors that require it, so the
same vector file can be run against every build configuration. Capability tags:

| tag        | a vector needs it when…                                            | disabled by |
|------------|-------------------------------------------------------------------|-------------|
| `fixlen`   | it has an fp32/fp64/string/blob (or a fixed-length array)          | `SOFAB_DISABLE_FIXLEN_SUPPORT` |
| `array`    | it has any array field                                             | `SOFAB_DISABLE_ARRAY_SUPPORT` |
| `sequence` | it has a nested sequence                                           | `SOFAB_DISABLE_SEQUENCE_SUPPORT` |
| `fp64`     | it has a 64-bit float (implies `fixlen`)                           | `SOFAB_DISABLE_FP64_SUPPORT` |
| `int64`    | a value/element is outside the 32-bit range, or a field id exceeds the 32-bit id cap | `SOFAB_DISABLE_INT64_SUPPORT` |

A vector with no special needs omits `requires` and runs in every build. The
standalone `sofab_vectortest` runner reports `run` vs `skipped` counts, and CI
builds it across the feature matrix.

> **`requires` only matters to consumers that can disable features.** An
> implementation that always supports the full wire format (i.e. has no way to
> compile features out) inherently supports every capability a vector can ask
> for, so it should **ignore `requires` and run all vectors**. The tags exist
> solely so a feature-reduced build can skip what it cannot represent.
> The one tag this does **not** cover is `dynamic_arrays`, which appears only on
> the [`sequence_growth`](#growth-cases--sequence_growth) block and describes how
> a port *allocates* rather than what it can parse — see there.

### Optional `skip_ids`

A vector may carry a top-level `"skip_ids": [..]` array — field ids a receiver is
expected to **skip** during decoding (simulating optional fields it doesn't care
about). The harness uses it to drive a *skip-ids* decode scenario: it leaves those
ids unread at every nesting level (so the decoder auto-skips the field — for any
wire type — and the whole sub-sequence, at any nesting depth, when the id names a
sequence), then verifies the remaining fields still decode and the message is
fully consumed. **Fields are only ever skipped when `skip_ids` is present**;
vectors without it just don't run that scenario.

#### The skip matrix — group `skip/matrix`

Skipping is driven by the **wire type** alone
([CORELIB_PLAN §4.3](https://github.com/sofa-buffers/documentation/blob/main/CORELIB_PLAN.md)),
and each type has its own length computation: a varint ends at its continuation
bit, a `fixlen` at the length in its `fixlen_word` (§4.6), an integer array after
`count` elements (§4.7), a fixlen array after `count × element_length` (§4.8), a
sequence at its end marker (§4.9). Every one of them can be off by a byte on its
own, and the symptom is always the same — the *next* field is read from the wrong
offset.

The 36 vectors in group `skip/matrix` are the full cross product over the ten
skippable constructs (the eight wire types, `fixlen` split by subtype because the
decoder branches on it, `sequence end` left out because it is a marker and not a
field a receiver can decline). Each vector is a chain of rows

```
[ read field of type P ] [ SKIPPED field of type S ] [ read unsigned anchor ]
```

with ids `3k` / `3k+1` / `3k+2`, so `skip_ids` is exactly the `3k+1` column. All
**100** (P, S) pairs are present across the 36 vectors. The anchor is the
detector: a skip that consumes one byte too few or too many leaves the decoder
inside it, and its value comparison fails. One anchor wire type is enough — what
a resync must land on is a field header, and every header is the same varint.

Vectors are grouped by the capability set their pair block needs
(`skip_matrix_<skipped tier>_after_<read tier>`, tiers: `varint`, `fixlen`,
`fp64`, `int_array`, `fixlen_array`, `sequence`), so a reduced build still runs
the part of the matrix it can represent instead of dropping one big all-types
vector whole. `boolean` is not in the matrix: §4.4 makes it an unsigned integer
on the wire, which the `varint` tier already covers.

The matrix varies only *which* construct is skipped behind *which* — it uses
small, non-empty payloads, top-level fields and an anchor behind every skip. The
remaining axes of a skip sit beside it, in group `skip`:

| Axis | Vectors |
|---|---|
| **zero-length payload** — `fixlen_word` with no payload; a count that ends the field; count `0` that still **keeps** the `fixlen_word` (§4.8) | `skip_empty_fixlen_payloads`, `skip_empty_int_arrays`, `skip_empty_fixlen_arrays` |
| **length/count needs two varint bytes** — 130-byte payloads and 130-element arrays, so a decoder that reads the length as one byte lands mid-payload | `skip_long_fixlen_payloads`, `skip_long_int_arrays`, `skip_long_fixlen_array` |
| **element width from the `fixlen_word`** — an fp64 array (element length 8), where the matrix uses fp32 (4) | `skip_fp64_array` |
| **three-byte header varint** — id `100000`, so `(id << 3 \| type)` spans three bytes (§4.3) | `skip_large_id` |
| **position: message start / message end** — nothing before the one, nothing behind the other, so the skip itself must reach a clean boundary (§5.2). One per wire-type state: varint, fixlen, integer array, fixlen array, sequence | `skip_at_message_edges`, `skip_fixlen_at_message_end`, `skip_int_array_at_message_end`, `skip_fixlen_array_at_message_end`, `skip_sequence_at_message_end` |
| **position: last field inside a sequence** — the resync lands on the end marker, not on a value-bearing header (§4.9) | `skip_before_sequence_end` |
| **nested / consecutive skips** — a skip inside a skipped sub-tree, at depth 2, and skips back to back | `nested_sequence_deep_skip`, `full_scale_example`, `skip_all_wire_types` |

### Negative vectors — `invalid_utf8`

Alongside the positive `vectors`, the file carries a **separate top-level
`invalid_utf8` array** of *negative* conformance cases: a `string` field (id `0`)
whose bytes are **not valid UTF-8**. They exist because a `string` is UTF-8
(MESSAGE_SPEC §8) and a strict corelib (`SOFAB_STRICT_UTF8`, CORELIB_PLAN §6.4)
rejects a non-UTF-8 `string` **symmetrically** — the positive vectors, being
valid encoder output, cannot express this.

```jsonc
{
  "name": "utf8_overlong_c0_80",
  "group": "invalid/utf8",
  "description": "...",
  "requires": ["fixlen"],
  "id": 0,                          // the string field id in serialized_hex
  "string_hex": "c080",            // the raw (invalid) string payload bytes
  "serialized_hex": "0212c080",    // the whole wire message: string field id 0
  "decode_outcome": "invalid",     // strict decode of serialized_hex -> INVALID
  "encode_outcome": "invalid_argument" // strict encode of string_hex -> InvalidArgument
}
```

Per entry, a **strict** implementation must:

- **decode** `serialized_hex` with the string **materialized** (read into a
  destination — a *skipped* field is never validated) and get the **INVALID**
  decode outcome — the same terminal class as any other malformed message, and
  chunk-boundary independent (a multi-byte sequence merely *split* at end-of-chunk
  stays INCOMPLETE; only a sequence still ill-formed once its complete declared
  payload has arrived is INVALID);
- **encode** `string_hex` as a `string` and get the **invalid-argument** error.

The seeds cover every overlong form (including `C0 80`, the Java "Modified UTF-8"
NUL), lone surrogates, code points above `U+10FFFF`, bare continuation / invalid
lead bytes (`0xFF`), and multi-byte sequences truncated at end-of-payload.

`invalid_utf8` is **backward-compatible**: a consumer that only reads `vectors`
ignores it and still passes every positive vector. A **non-strict** build (one
that compiled the check out, or has no strings) skips these — it cannot represent
the rejection — but its CI **must** still run the strict configuration. In this
footprint corelib the strict check defaults **OFF** (CORELIB_PLAN §6.4), so its
CI enables it explicitly (`-DSOFAB_ENABLE_STRICT_UTF8`) on a dedicated strict-ON
leg that runs these negative vectors; targets that ship strict ON by default get
it for free. `string_hex` / `serialized_hex` are lowercase hex, like
`serialized.hex`; the payload is placed at field id `0` with the `string` (fixlen
UTF-8) wire subtype.

### Growth cases — `sequence_growth`

A third top-level block, beside `vectors` and `invalid_utf8`. It carries the
**sequence-array growth cases** every growing port must run
([CORELIB_PLAN §7.2 item 8](https://github.com/sofa-buffers/documentation/blob/main/CORELIB_PLAN.md)).

**A growth case is a delivery sequence of element ids, not a byte string** —
which is why it cannot be a vector. A wrapper (sequence) array carries no element
count on the wire: its length is *highest present id + 1* (MESSAGE_SPEC §5.1), so
the size is known only once the array ends and the container **grows** as
elements arrive. It is the one allocation shape where growth is conformant
([generator ARCHITECTURE §9.5](https://github.com/sofa-buffers/generator/blob/main/docs/ARCHITECTURE.md)
shape B); everything with a count or length ahead of its payload — integer arrays
§4.7, fixlen arrays §4.8, `string`/`blob` — checks that word and allocates exactly
it, once. The positive suite is structurally blind to this: two ports that grow
differently emit **identical bytes** and reach **identical outcomes**, so no
`serialized.hex` can tell them apart. The port therefore **builds the message
itself** from `deliver` and asserts `expect`.

```jsonc
{
  "name": "growth_index_at_cap_minus_one",
  "group": "growth/index",
  "description": "...",
  "requires": ["sequence", "fixlen", "dynamic_arrays"],
  "field_id": 0,                    // the wrapper-array field's id in the top-level scope
  "element_type": "string",         // "string" | "struct"
  "deliver": [                      // elements delivered, in the given order
    { "id": 0,           "value": "a" },
    { "id_from_cap": -1, "value": "b" }
  ],
  "expect": {
    "outcome": "complete",          // "complete" | "limit_exceeded"
    "length_from_cap": 0            // the resulting container length
  }
}
```

**Indices are cap-relative.** The receiver cap `max_dyn_array_count` is per-target
**configuration** — CORELIB_PLAN §6.2.1 and ARCHITECTURE §9.5 deliberately fix no
family-wide number, because an element count that is trivial on a server is brutal
in C. So a case never names an absolute boundary: `id_from_cap` and
`length_from_cap` are **offsets added to the cap**, and each port substitutes its
own configured value when it runs the block (`-1` → `cap - 1`, `0` → `cap`).
`id` and `length` are absolute. Every case assumes a cap of **at least 4**; a port
configured below that raises it for the block's run.

| key | where | meaning |
|---|---|---|
| `field_id` | case | the wrapper array field's id in the top-level scope |
| `element_type` | case | `string` — the element is a leaf; `struct` — the element is a framed sub-sequence carrying one `unsigned` field at id `0`, whose value is the case's `value` |
| `deliver[].id` / `.id_from_cap` | element | the element index, absolute or cap-relative (exactly one of the two) |
| `deliver[].value` | element | the element's value; a JSON string for `string`, a JSON integer for `struct` |
| `expect.outcome` | case | `complete`, or `limit_exceeded` for the receiver-cap policy rejection (CORELIB_PLAN §6.3) |
| `expect.length` / `.length_from_cap` | case | the resulting container length, absolute or cap-relative (`complete` only) |
| `expect.default_ids` | case | ids that must hold the **element default** (`complete` only) |
| `expect.terminal` | case | the rejection is terminal — further input cannot lift it (`limit_exceeded` only) |
| `expect.max_length` | case | the container must **not** have been extended past this (`limit_exceeded` only) |

Assert the **container length** and the **outcome**, and nothing else — no
allocator instrumentation. That is what makes the cases portable across eleven
languages (CORELIB_PLAN §7.2 item 8).

An empty `deliver` means the wrapper is framed **empty** — the explicit-empty
form of MESSAGE_SPEC §5.1, with no element at all and therefore length `0`.

**Both element types are covered on the boundary cases.** A `string` element
reaches the container through the collector's leaf path, a `struct` element
through its sequence path, and a port can get one right and the other wrong. The
expectations are identical for both, deliberately: the bound is the element
**index**, never the element kind.

#### Gating — `requires: ["dynamic_arrays"]`

`dynamic_arrays` is a **profile** capability, not a `SOFAB_DISABLE_*` build
switch: a port declares it when its wrapper-array containers **grow at decode
time**. **Statically bounded profiles** — C, C++ `corelib: c-cpp`, Rust `no_std` —
are capacity-bound by construction, never grow, and so do not declare it and do
not run this block (ARCHITECTURE §9.5; CORELIB_PLAN §7.2 item 8: such a port
"states that instead").

> This is the one exception to the "ignore `requires` and run everything" rule
> above. That rule is about *wire constructs* a reduced build cannot represent —
> a port supporting the full wire format supports them all. `dynamic_arrays` says
> something else: not what the port can parse, but **how it allocates**. A
> statically bounded port parses every one of these messages perfectly and still
> cannot exhibit growth, so it must honour this tag even though it honours no
> other.

An unsatisfied `dynamic_arrays` means **skip**, never *reject*. A build that
compiled a wire construct out rejects any message carrying it, so an unsatisfied
`fixlen`/`array`/`sequence`/`fp64`/`int64` tag on a *vector* turns that vector
into a negative case. Nothing of the sort applies here: a growth case's message
is an ordinary, well-formed wrapper array that a statically bounded port decodes
perfectly. Only the *growth* it is asserting is out of reach. Do not wire these
cases into a rejection path.

**`corelib-c-cpp` authors these cases and does not execute them.** It generates
this file and is statically bounded, so it is on the excluded side of its own
gating. **The expected values therefore come from ARCHITECTURE §9.5 and
CORELIB_PLAN §7.2 item 8 — not from what that library does.** If you run the
block against `corelib-c-cpp` and see failures, the gating was bypassed; do not
"fix" the file to match the one implementation that structurally cannot run it.

#### Growth geometry is not asserted here

A conformant decoder grows **to at least `id + 1`**, not exactly `id + 1`, so a
sparse array does not cost O(n²) copies (ARCHITECTURE §9.5 shape B). That is the
one property in this block a length-and-outcome assertion cannot reach — it needs
the language's own allocation-counting facility. Test it where the language
offers one; where it does not, **say so in the port's README** rather than
reporting the case as passed (CORELIB_PLAN §7.2 item 8).

`sequence_growth` is **backward-compatible**: a consumer that only reads
`vectors` ignores it and still passes every positive vector.

### Header-ceiling cases — `header_limits`

A fourth top-level block. It carries the **truncated over-ceiling header**: bytes
that *declare* a length or count and then **end**, with not one payload byte
behind them.

```
02 a2 06   then EOF
^^ id 0, wire type 2 (fixlen)
   ^^^^^ length word (100 << 3) | 2  ->  a 100-byte STRING is declared
           ... and the message ends.
```

A conformant decoder answers **at that word** — before the payload is asked for
— so the answer is the ceiling's and it is **terminal**. `INCOMPLETE` is wrong
here, and not merely unhelpful:

* **CORELIB_PLAN §6.2.1's enforcement point imports MESSAGE_SPEC §5.2.3's reason
  by name** — "at the count/length header … *for the same reason `INVALID` is
  decided there*". §5.2.3's reason is that a decoder deferring until the payload
  arrives hits end-of-input first and mis-reports malformed input as
  `INCOMPLETE`. A decoder that compares at the header and *then* answers
  `INCOMPLETE` produces exactly the outcome that reason exists to prevent.
* **§6.3 calls the rejection terminal**, while §5.2.1 defines `INCOMPLETE` as the
  outcome more bytes *can* change and §5.2.4 has a streaming caller read it as
  "feed me the next chunk". After a ceiling has fired, that is a false statement
  about the state.
* ARCHITECTURE §9.5: *"a claimed oversize fails fast even if the payload never
  arrives."*

**Why this cannot be a vector.** The `vectors` block is round-trip only — it
carries no verdict field at all. It says "these fields encode to these bytes"; it
cannot say "these bytes must be rejected, with category X". `invalid_utf8` is the
wrong subject and `sequence_growth` is the right *shape* one axis over (the
wrapper-array index, which has no header word to bind). Hence a block of its own.

```jsonc
{
  "name": "header_string_over_cap",
  "group": "limits/header",
  "description": "...",
  "requires": ["fixlen", "receiver_caps"],
  "field_id": 0,                      // the field's id in the top-level scope
  "declared": 100,                    // the length/count the header claims
  "limits": { "max_dyn_string_len": 16 },   // configure this for the case's run
  "serialized": "02a206",             // the header, and nothing after it
  "expect": { "outcome": "limit_exceeded", "terminal": true }
}
```

#### Which ceiling speaks is the subject

The two are **opposite answers on the same word**, and a case carries `schema` or
`limits` — never both, because §6.2.1 forbids applying a receiver cap to a field
the schema already bounds.

| the case states | the ceiling | a breach is | why |
|---|---|---|---|
| `"schema": { "maxlen": N }` | the schema bound | `invalid` | the schema says these bytes are invalid (MESSAGE_SPEC §7.1) |
| `"limits": { "max_dyn_…": N }` | the receiver cap | `limit_exceeded` | the bytes are well-formed; this receiver declines to hold that much (§6.2.1) |

`header_string_schema_bounded` and `header_string_over_cap` carry the **identical
bytes** and differ only in which ceiling the case configures — that pair is what
keeps the two categories apart. A port that routes both to one category passes
every other case in the block and fails that one.

| key | meaning |
|---|---|
| `field_id` | the field's id in the top-level scope |
| `declared` | the length (`string`/`blob`) or element count (`array`) the header claims |
| `limits` | the §6.2.1 receiver cap to configure for this case — `max_dyn_string_len`, `max_dyn_blob_len` or `max_dyn_array_count` |
| `schema` | the schema `maxlen` to declare for this case, instead of a cap |
| `serialized` | lowercase hex of the **header alone**; the message ends there |
| `chunks` | OPTIONAL: feed the bytes as these separate chunks rather than in one call |
| `expect.outcome` | `limit_exceeded`, `invalid`, or `incomplete` |
| `expect.terminal` | the rejection is terminal — a further feed re-raises rather than consuming (absent on `incomplete`, which is precisely the state more bytes can lift) |

#### Bounds are absolute here, not cap-relative

The opposite of [`sequence_growth`](#growth-cases--sequence_growth), and for a
concrete reason. There the port **builds** the message from a delivery list, so a
cap-relative index can be substituted at run time. Here the case **is** a fixed
byte string: the declared length is baked into the varint, so the case must
instead *tell* the port which ceiling to configure. `limits`/`schema` are that
instruction — not a family-wide claim about anyone's deployment. A port restores
its own configuration after the block.

#### Every rejection is paired with its in-cap control

`header_string_in_cap` is the same shape at a length the ceiling **admits**, and
it must still answer `incomplete`. Without it the block proves nothing: a port
that rejects every short read passes all six rejection cases and is badly broken.
Treat a missing control as a bug in the block, not an omission.

#### Gating — `requires: ["receiver_caps"]`

`receiver_caps` is a **profile** capability, like `dynamic_arrays` and unlike the
wire-construct tags: a port declares it when its generated code carries §6.2.1
receiver caps *distinct from* schema bounds. A profile that refuses
schema-unbounded fields at generate time has no such cap and skips those cases —
but still runs the `schema`-bounded pair, which needs no cap and is tagged
accordingly.

**In this block an unsatisfied `requires` tag means SKIP, for every tag.** That
differs from a *vector*, where an unsatisfied wire-construct tag turns the vector
into a negative case (the reduced build rejects the construct it was compiled
without). The distinction is that these cases already assert a rejection *with a
specific category*: a build that cannot represent the construct rejects it for an
unrelated reason and would appear to pass while testing nothing.

**What each port owes:** feed `serialized` (or `chunks`) under the stated
`limits`/`schema`, assert `expect.outcome`, and where `terminal` is set, assert
that a further feed re-raises rather than consuming.

#### `corelib-c-cpp` authors these cases; only part of the family runs them

As with `sequence_growth`, **the expected values come from CORELIB_PLAN §6.2.1 /
§6.3 and MESSAGE_SPEC §5.2 — not from what this library does.**

Worth stating plainly, because it is easy to get backwards: the C++ wrapper here
**does** implement this ceiling and answers `LimitExceeded` at the length word
(`readStringCapped` / `readBlobCapped`, and see
[`../test/cpp/test_receiver_limits.cpp`](../test/cpp/test_receiver_limits.cpp),
"the cap is enforced at the length header, before the allocation"). What this
repo lacks is a **C++ runner for the JSON blocks** — the shared vector engine is
plain C, and the plain-C API has no §6.2.1 receiver cap at all (its
`SOFAB_FIXLEN_MAX` is a format ceiling, whose breach is `INVALID`). So the block
is authored here and executed elsewhere, exactly like `sequence_growth`, and for
an adjacent but distinct reason. See the note at the top of
[`../test/shared/sofab_test_vectors.h`](../test/shared/sofab_test_vectors.h).

`header_limits` is **backward-compatible**: a consumer that only reads `vectors`
ignores it and still passes every positive vector.

### Decode scenarios the harness runs per vector

`encode`, `chunked-encode` (1/3/7-byte buffers), `decode`, `chunked-decode`
(one byte at a time), `skip-ids` (+ a chunked variant, only when `skip_ids` is
present), and `roundtrip`.

### Conventions

- **Byte order:** little-endian.
- **`serialized.hex`** is the authoritative ground truth (lowercase hex of the
  whole message).
- **Integers** are decimal JSON number literals spanning the full `u64`/`i64`
  range.
- **Floats** are JSON numbers when finite; `±infinity` is encoded as the strings
  `"inf"` / `"-inf"`. `NaN` is intentionally excluded because its bit pattern is
  not portable across architectures.
