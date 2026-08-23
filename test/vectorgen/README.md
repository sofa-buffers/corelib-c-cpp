# SofaBuffers test-vector generator

`gen_vectors.c` is a developer helper tool (not a unit test) that produces the
conformance suite **[`assets/test_vectors.json`](../../assets/test_vectors.json)**.

**The format and contents of that file are documented next to it, in
[`assets/test_vectors_README.md`](../../assets/test_vectors_README.md).** This
page only covers regenerating it.

## How it works

The tool describes each message as a list of encode operations and **replays it
through the real SofaBuffers C encoder** — so the `fields` it prints and the
`serialized.hex` it prints come from the same run and cannot disagree. It also
derives each vector's `requires` capability tags from the ops/values/ids, so the
suite can be run against reduced (`SOFAB_DISABLE_*`) builds. The vectors mirror
the happy-path cases in [`../c/test_ostream.c`](../c/test_ostream.c).

Two top-level blocks are **not** replayed through the encoder, because no encoder
run can produce them: `invalid_utf8` (a valid encoder cannot emit an invalid-UTF-8
`string`) and `sequence_growth` (a growth case is keyed by a delivery sequence of
element ids, not by bytes). Both are hand-authored seed tables —
`emit_invalid_utf8()` and `emit_sequence_growth()` — and their expected values
come from the specs, not from this library's behaviour. `sequence_growth` in
particular is authored here and **executed elsewhere**: this repo is statically
bounded and its own `requires` gating excludes it. See
[`assets/test_vectors_README.md`](../../assets/test_vectors_README.md).

Each message is replayed **twice**, producing the file's two byte columns: the
dense `serialized` (every op, sequences opened eagerly) and the sparse-canonical
`serialized_sparse` (default leaves dropped and sequences opened *lazily*, so one
left without content is omitted whole — MESSAGE_SPEC §2). Only `serialized` is
asserted by this repo's own suite; `serialized_sparse` is for the generator's
per-language conformance drivers, which encode generated objects. See
[`assets/test_vectors_README.md`](../../assets/test_vectors_README.md).

## Regenerating

You only need this if you change the encoder or the set of vectors.

```sh
cmake -S . -B build -DSOFAB_ENABLE_VECTORGEN=ON
cmake --build build --target generate-vectors      # rewrites assets/test_vectors.json
git diff assets/test_vectors.json                  # review, then commit
```

The tool is gated behind `-DSOFAB_ENABLE_VECTORGEN=ON` (default off) so it never
affects the normal library, test, or cross-compile builds.
