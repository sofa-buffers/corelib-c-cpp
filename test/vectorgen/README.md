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

## Regenerating

You only need this if you change the encoder or the set of vectors.

```sh
cmake -S . -B build -DSOFAB_ENABLE_VECTORGEN=ON
cmake --build build --target generate-vectors      # rewrites assets/test_vectors.json
git diff assets/test_vectors.json                  # review, then commit
```

The tool is gated behind `-DSOFAB_ENABLE_VECTORGEN=ON` (default off) so it never
affects the normal library, test, or cross-compile builds.
