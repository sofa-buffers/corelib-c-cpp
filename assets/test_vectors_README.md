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
      "serialized": { "length": 6, "hex": "0220560e4940" }   // ground truth
    }
  ]
}
```

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
| `sequence_end`    | —                                           | close the current sequence |

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

### Optional `skip_ids`

A vector may carry a top-level `"skip_ids": [..]` array — field ids a receiver is
expected to **skip** during decoding (simulating optional fields it doesn't care
about). The harness uses it to drive a *skip-ids* decode scenario: it leaves those
ids unread at every nesting level (so the decoder auto-skips the field — for any
wire type — and the whole sub-sequence, at any nesting depth, when the id names a
sequence), then verifies the remaining fields still decode and the message is
fully consumed. **Fields are only ever skipped when `skip_ids` is present**;
vectors without it just don't run that scenario.

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
