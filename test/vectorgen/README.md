# SofaBuffers test vectors

**The artifact this tool produces is [`assets/test_vectors.json`](../../assets/test_vectors.json)**
— a language-agnostic conformance suite for the SofaBuffers wire format. Each
vector pairs a **message structure + values** with the **exact serialized bytes**.

Another implementation (e.g. [`corelib-rs`](https://github.com/sofa-buffers/corelib-rs))
can load this file and, for every vector, either:

- re-encode the message from `fields` and assert the output equals `serialized.hex`, or
- decode `serialized.hex` and assert it recovers the values in `fields`.

That's all most consumers need — just read the JSON. The `gen_vectors.c` program
next to it is **only a helper** used to produce the JSON (see
[Regenerating](#regenerating)).

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

### Conventions

- **Byte order:** little-endian.
- **`serialized.hex`** is the authoritative ground truth (lowercase hex of the
  whole message).
- **Integers** are decimal JSON number literals spanning the full `u64`/`i64`
  range.
- **Floats** are JSON numbers when finite; `±infinity` is encoded as the strings
  `"inf"` / `"-inf"`. `NaN` is intentionally excluded because its bit pattern is
  not portable across architectures.

## Regenerating

You only need this if you change the encoder or the set of vectors.

`gen_vectors.c` is a developer helper tool, not a unit test. It describes each
message as a list of encode operations and **replays it through the real
SofaBuffers C encoder** — so the `fields` it prints and the `serialized.hex` it
prints come from the same run and cannot disagree. The vectors mirror the
happy-path cases in [`test/c/test_ostream.c`](../c/test_ostream.c).

```sh
cmake -S . -B build -DSOFAB_ENABLE_VECTORGEN=ON
cmake --build build --target generate-vectors      # rewrites assets/test_vectors.json
git diff assets/test_vectors.json                  # review, then commit
```

The tool is gated behind `-DSOFAB_ENABLE_VECTORGEN=ON` (default off) so it never
affects the normal library, test, or cross-compile builds.
