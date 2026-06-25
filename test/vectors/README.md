# SofaBuffers test vectors

`test_vectors.json` is a language-agnostic conformance suite for the SofaBuffers
wire format. Each vector pairs a **message structure + values** with the **exact
serialized bytes** the C encoder produces, so any other implementation (e.g.
[`corelib-rs`](https://github.com/sofa-buffers/corelib-rs)) can re-encode each
message and assert it matches — or decode the bytes and assert it recovers the
values.

The vectors are derived from the happy-path encoder tests in
[`test/c/test_ostream.c`](../c/test_ostream.c).

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

`test_vectors.json` is produced by `gen_vectors.c`, which replays each message
through the real C encoder — the structure and the bytes therefore can never
drift apart. To regenerate after changing the encoder or the vector set:

```sh
cmake -S . -B build -DSOFAB_ENABLE_VECTORS=ON
cmake --build build --target generate-vectors
```

CTest case `test_vectors_up_to_date` fails if the committed JSON no longer
matches the generator output.
