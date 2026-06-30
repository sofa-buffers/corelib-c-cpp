# SofaBuffers `corelib-c-cpp` — Conformance Gap Analysis & Remediation Plan

Audit of the C99 / C++20 reference implementation against the language-independent
specification (`CORELIB_PLAN.md`), with particular attention to the **§13
Conformance Checklist**. This repository is the *authoritative source* for the
shared `test_vectors.json` and the reference implementation, but it must still
satisfy the checklist; it is judged on that basis here.

The only change introduced by this audit is this document. No implementation,
test, config, asset, or README file was modified.

## Spec revision

- **Audited against** the updated `CORELIB_PLAN.md` (commit **dcb85d6** — *zero-length
  arrays & empty sequences now legal*), dated **2026-06-30**.
- **What changed vs the previous revision of this analysis:**
  - §4.7: `element_count` range widened to `0 .. 2,147,483,647`. A **zero-count
    integer array** (unsigned or signed) is now valid/well-formed on the wire —
    exactly `[ header_varint ] [ element_count_varint = 0 ]`. Absent-vs-empty is a
    code-generator concern, **not** wire-level.
  - §4.8: a **zero-count fixlen array** carries **no `fixlen_word` and no payload** —
    exactly `[ header_varint ] [ element_count_varint = 0 ]`.
  - §4.9: an **empty sequence** (`sequence start` immediately followed by `0x07`) is
    legal and a decoder **MUST** accept it.
  - **Consequence for this audit:** the previous revision treated count-0 arrays as
    *malformed* and scored the array/test items PASS on that basis. That premise is
    now inverted — rejecting a zero-count array on encode or decode is
    **non-conformant**. Item 6 drops PASS → **GAP**, item 12 drops PASS → **PARTIAL**.
    Empty-sequence support was re-verified and is fully conformant (item 7 stays PASS).

## Summary

| Status | Count |
|--------|-------|
| PASS | 14 |
| PARTIAL | 3 |
| GAP | 1 |
| **Total** | **18** |

The new GAP (item 6) is the zero-length-array regression introduced by the spec
change: both the encoder and the decoder actively reject `element_count == 0`, the
fixlen-array encoder would emit a stray `fixlen_word` for an empty array (violating
§4.8), and a dedicated test *asserts* the now-illegal rejection. Empty **sequences**,
by contrast, are encoded, decoded, and covered by shared vectors correctly. The two
remaining PARTIALs are the pre-existing **GitHub Actions workflow** naming/trigger
deviations (§12), unchanged by this spec revision. Everything else — varint/zig-zag,
field header, fixlen scalars, non-empty arrays, sequence framing, streaming on both
sides, API/error model, generated-object layer, assets, README, benchmarks, and the
devcontainer — conforms.

## Zero-length / empty-sequence support at a glance

| Construct | Encode | Decode | Tests | Shared vectors |
|-----------|--------|--------|-------|----------------|
| Zero-count unsigned-int array (§4.7) | **Rejected** — `assert(element_count > 0)` (`src/ostream.c:338`); under `NDEBUG` would emit a correct `[hdr][0]` | **Rejected** — `if (count == 0) return E_INVALID_MSG` (`src/istream.c:718-722`) | **Wrong** — `test_msg_invalid_array_count_zero` asserts rejection (`test/c/test_istream.c:546-569`) | **None** — no zero-count array vector generated (`test/vectorgen/gen_vectors.c`) or present in `assets/test_vectors.json` |
| Zero-count signed-int array (§4.7) | **Rejected** — `assert(element_count > 0)` (`src/ostream.c:391`) | **Rejected** — same `count == 0` guard (`src/istream.c:718-722`) | Covered only by the negative test above (target type i8) | **None** |
| Zero-count fixlen array (§4.8, no `fixlen_word`/payload) | **Rejected + would be malformed** — `assert(element_count > 0)` (`src/ostream.c:446`); even bypassed, it unconditionally writes the `fixlen_word` (`src/ostream.c:462`), violating §4.8 | **Rejected** before reaching fixlen handling (`src/istream.c:718-722`); if reached, `ARRAY_COUNT`→`FIXLEN_LEN` (`src/istream.c:748-749`) would read a `fixlen_word` that is not on the wire | none | **None** |
| Empty sequence (§4.9) | **OK** — `write_sequence_begin` then `write_sequence_end`, no min-child assert (`src/ostream.c:500,514`) | **OK** — start pushes a child decoder, `0x07` pops it; no min-child assumption (`src/istream.c`) | Exercised via shared engine | **Yes** — `empty_sequence`, `nested_empty_sequences`, `empty_sequence_between_fields` generated (`test/vectorgen/gen_vectors.c:593,601,611`) and present in `assets/test_vectors.json` |

## Per-checklist-item findings

| # | Item (§13) | Status | Evidence | Notes |
|---|------------|--------|----------|-------|
| 1 | All public symbols under the `sofab` namespace (§6) | PASS | C uses the `sofab_` prefix throughout (`src/include/sofab/sofab.h`, `ostream.h`, `istream.h`, `object.h`); C++ uses `namespace sofab` (`src/include/sofab/sofab.hpp:78`). | Namespace spelled exactly `sofab`. |
| 2 | API version constant/getter returns `1` (§6) | PASS | `#define SOFAB_API_VERSION 1` (`sofab.h:32`); `inline constexpr int API_VERSION = 1` (`sofab.hpp:81`); asserted by `static_assert(sofab::API_VERSION == 1 ...)` (`test/cpp/test_istream.cpp:310`, `test/cpp/test_ostream.cpp:83`). | — |
| 3 | Varint & zig-zag encode/decode match §4.1–4.2 | PASS | `_varint_encode` (`src/ostream.c:82`); `_varint_decode` with width/overflow guard (`src/istream.c:77-111`); `_zigzag_encode` 64-bit width (`ostream.c:36`); `_zigzag_decode` (`istream.c:60`). Byte output validated by `test_vectors.json`. | Overlong/overflow varints rejected with `E_INVALID_MSG`. |
| 4 | Field header `(id<<3)\|type` and all 8 wire types (§4.3) | PASS | `_type_encode` packs `(var<<3)\|type` (`ostream.c:110`); `sofab_type_t` enumerates 0x0–0x7 (`sofab.h:48-58`); decoder dispatch over all eight in the IDLE state (`istream.c:335-379`). | Tag values are normative and exact. |
| 5 | Fixlen word `(length<<3)\|fixlen_type`, LE floats, UTF-8 strings without terminator, blobs (§4.6) | PASS | `sofab_ostream_write_fixlen` emits `_type_encode(datalen,type)` (`ostream.c:298`); big-endian float byte-reversal on write (`ostream.c:303-323`) and read (`istream.c:644-656`); string written via `strlen`, no terminator (`ostream.h:299-303`); blob path (`ostream.h:317`). Empty string/blob (`length == 0`) round-trip via `string_empty` / `blob_empty` vectors. | Subtypes fp32/fp64/string/blob handled; reserved subtypes rejected on decode (`istream.c:581`). |
| 6 | Integer arrays + fixlen arrays with a single shared fixlen word; no dynamic subtypes in fixlen arrays (§4.7–4.8) | **GAP** | Non-empty arrays are correct: `write_array_of_unsigned/signed` (`ostream.c:330,383`); `write_array_of_fixlen` writes count then one shared `_type_encode` word (`ostream.c:457-465`); decoder reads the shared word once via `ARRAY_COUNT`→`FIXLEN_LEN` (`istream.c:724-751`). **But zero-count arrays (now legal per §4.7/§4.8) are rejected on both sides:** encoder `assert(element_count > 0)` (`ostream.c:338,391,446`); decoder `if (count == 0) return E_INVALID_MSG` (`istream.c:718-722`). The fixlen-array encoder unconditionally emits a `fixlen_word` (`ostream.c:462`), so even an assert-disabled build cannot produce the §4.8 zero-count form (which must carry **no** `fixlen_word`). | A decoder that rejects a well-formed zero-count array is non-conformant under the updated spec; the encoder cannot produce one, and the fixlen zero-count rule (no `fixlen_word`) is unimplemented. Dynamic-subtype rejection for fixlen arrays is still only the `NDEBUG`-strippable `assert(type <= SOFAB_FIXLENTYPE_FP64)` (`ostream.c:450`); carried forward as a hardening note. |
| 7 | Sequence start/end framing, fresh ID scope, single-byte `0x07` end, skip-by-walking with depth tracking, reject nesting beyond `MAX_DEPTH`=255 (§4.9) | PASS | `write_sequence_begin/end` (`ostream.c:500,514`); end is `(0<<3)\|7 == 0x07`; fresh scope via parent-linked decoder chain (`istream.c:791-809`); skip walks with `skip_depth`, rejecting at `UINT8_MAX` (255) → `E_INVALID_MSG` (`istream.c:397-404`); 256-level rejection test (`test/c/test_istream.c:1802`). **Empty sequences (now explicitly legal, §4.9) are supported:** encode imposes no min-child constraint, decode pops cleanly on `0x07`, and `empty_sequence` / `nested_empty_sequences` / `empty_sequence_between_fields` vectors are generated and pass encode/decode/skip/chunked. | No public named `SOFAB_MAX_DEPTH` constant (the §6.2 value 255 is implicit in `uint8_t skip_depth`); encoder does not itself cap nesting depth. Minor. |
| 8 | Streaming encode into a smaller-than-message buffer via flush callback / sink, with mid-stream buffer swap (§5.1) | PASS | `_push_byte` flushes when full and resumes (`ostream.c:52-73`); `sofab_ostream_buffer_set` swaps the buffer (`ostream.c:230`); start `offset` honoured (`ostream.c:191-205`); buffer-full without callback returns `E_BUFFER_FULL`. Chunked-encode tested with 1/3/7-byte buffers (`test/shared/sofab_test_vectors.c`). | C++ mirrors via `OStream`/`OStreamInline` + `setBuffer` (`sofab.hpp:551-663`). |
| 9 | Streaming decode via `feed` of arbitrarily small chunks, push-callback / pull-read, lazy binding, auto-skip (§5.2) | PASS | Byte-driven resumable state machine in `sofab_istream_feed` (`istream.c:288-759`); `read_*` only binds a destination lazily (`sofab_istream_read_field`, `istream.c:761`); unbound fields and whole sub-sequences auto-skip. One-byte-at-a-time decode tested (`test/shared/sofab_test_vectors.c`). | — |
| 10 | Result/error reporting follows §6.3 baseline codes (return codes in C/embedded) | PASS | `sofab_ret_t` = OK / E_ARGUMENT / E_USAGE / E_BUFFER_FULL / E_INVALID_MSG (`sofab.h:38-45`), mapping to None/InvalidArgument/UsageError/BufferFull/InvalidMessage; C++ `enum class Error` mirrors them (`sofab.hpp:89-96`). Built `-fno-exceptions` (`CMakeLists.txt:29`). | Return-code model, correct for C / embedded targets. |
| 11 | Streaming primitives suffice for a thin generated-object layer with a dead-simple API that also streams in chunks; one-shot helpers are thin wrappers (§6.1) | PASS | C descriptor transcoder is built purely on the stream API: `sofab_object_encode` calls `sofab_ostream_write_*` (`src/object.c:84-234`) and `sofab_object_field_cb` binds `sofab_istream_read_*` / `read_sequence` (`object.c:236-347`). C++ `OStreamMessage`/`IStreamMessage` + `OStreamObject::serialize()` (one-shot) and `IStreamObject` feed-driven decode (`sofab.hpp:682-1253`). | The layer inherits the item-6 zero-count-array limitation (`read_array` also asserts `element_count > 0`, `istream.c:780`), so a generated repeated-field of length 0 cannot round-trip until item 6 is fixed. Otherwise both one-shot and chunked paths exist over the same primitives. |
| 12 | All shared test vectors pass encode + decode, plus chunked, roundtrip, malformed, skip (§7) | **PARTIAL** | Shared engine runs encode, chunked-encode (1/3/7 B), decode, chunked-decode (1 B), skip-ids (+ chunked), roundtrip (`test/shared/sofab_test_vectors.c`); float compares bit-exact (`eq32`/`eq64`). Malformed cases (truncated/overlong varint, id overflow, oversized fixlen/array, bad fixlen type, extra sequence-end, depth>255) in `test/c/test_istream.c`. 67 vectors in `assets/test_vectors.json`. | **Two spec-conformance defects:** (1) `test_msg_invalid_array_count_zero` (`test/c/test_istream.c:546-569`) asserts a zero-count array is *rejected* — it now enforces non-conformant behaviour. (2) Neither `gen_vectors.c` nor `assets/test_vectors.json` contains any **zero-count integer or fixlen array** vector; because this repo is the authoritative generator, every downstream port's shared suite also lacks that coverage. Empty-sequence vectors are present and correct. |
| 13 | `assets/` populated per §8 (branding + `test_vectors.json`) | PASS | `assets/sofabuffers_logo.png`, `assets/sofabuffers_icon.png`, `assets/test_vectors.json` (format `sofabuffers-test-vectors` v1, 67 vectors across scalar/array/sequence/composite/skip groups incl. id boundaries and empty sequences), `assets/test_vectors_README.md`. | Assets present and complete *as files*; the missing zero-count-array **vector coverage** is tracked under item 12, not here. This repo is the authoritative generator of the vectors. |
| 14 | README follows the family format with badges and required sections (§9) | PASS | Centered logo + tagline + org link, `## SofaBuffers C/C++ library` with coverage + Docs + per-target build badges, `## Why this design` table, Usage (basic encode/decode **and** flush-callback streaming larger than the buffer), API summary, Feature flags table, Build & test, Benchmarks (`README.md:1-477`). | No package-manager install command (C/C++ has no canonical registry); a FetchContent/add_subdirectory "consume as a dependency" snippet would close the §9-item-2 install nuance. Minor. |
| 15 | `perf` (CPU-independent) and `bench` (MB/s) tools present and runnable (§10) | PASS | `bench/c/perf.c`, `bench/c/bench.c`, `bench/cpp/perf.cpp`, `bench/cpp/bench.cpp`; CMake `run_perf` / `run_bench` / `run_bench_callgrind` targets (`bench/CMakeLists.txt:41-61`); documented in `README.md:405-428`. | `BENCH_SPEC.md` (referenced by §10) is not present in the repo; checklist item only requires the tools, so PASS, but adding/linking it would complete §10. Minor. |
| 16 | `.devcontainer/` with all required files; extensions incl. `anthropic.claude-code`; `.devcontainer/.env` gitignored (§11) | PASS | `Dockerfile` (Ubuntu 24.04 + toolchain + `gh` + Node LTS + `@anthropic-ai/claude-code`), `build.sh`, `start.sh` (loads `.env` via `--env-file`, warns if absent), `attach.sh`, `devcontainer.json` (cpptools/cmake + `anthropic.claude-code`, `--env-file` in `runArgs`), `.env.example` (documented `GH_TOKEN`). Image/container name `cpp-devcontainer`. `.env` gitignored (`.devcontainer/.gitignore:6`, root `.gitignore:4`) and not tracked. | — |
| 17 | `ci.yml` builds and tests on push and PR; matrix where versions matter; coverage uploaded and badge in README (§12.1) | PARTIAL | Push+PR CI exists as `build-gcc-x86_64.yaml` and `build-clang-x86_64.yaml`; compiler/feature/arch matrix in `build-feature-matrix.yaml` / `build-cpp-feature-matrix.yaml` (`fail-fast: false`); coverage in `coverage.yaml` (gcovr → badge JSON on `badges` branch) wired into `README.md:12`. | No file literally named `ci.yml`; native x86_64 legs configure **Debug only** (§12.1 step 4 wants Debug *and* Release — Release only on cross jobs); no toolchain dependency cache; coverage is self-hosted on a `badges` branch rather than Codecov ("or equivalent", acceptable). Unchanged by this spec revision. |
| 18 | `docs.yml` generates HTML docs and publishes to GitHub Pages via Actions-based deploy; Docs badge links to the site (§12.2) | PARTIAL | `build-doxygen.yaml` (display name "Docs"): Doxygen → `actions/upload-pages-artifact@v3` → `actions/deploy-pages@v4`, deploy gated to push-to-`main`, `pages: write` / `id-token: write` perms; Docs badge → `https://sofa-buffers.github.io/corelib-c-cpp/` (`README.md:13`). No `gh-pages` branch. | No file literally named `docs.yml`; the workflow also triggers on `pull_request` (build-only; deploy gated), whereas §12.2 specifies push-to-`main` only. Behaviour otherwise compliant. Unchanged by this spec revision. |

## Remediation Plan

Ordered by severity. Acceptance criteria are written so each can be verified independently.

### 1. (High) Zero-length arrays are rejected on encode and decode; fixlen zero-count form is malformed (item 6, §4.7–4.8)

**Problem.** Under the updated spec a zero-count array is valid wire data and a decoder
**MUST** accept it. Today:
- The decoder explicitly returns `SOFAB_RET_E_INVALID_MSG` for `element_count == 0`
  (`src/istream.c:718-722`) — it rejects a well-formed message.
- All three encoders assert `element_count > 0` (`src/ostream.c:338,391,446`), so an empty
  array cannot be produced in an `assert`-enabled build.
- The fixlen-array encoder unconditionally writes the shared `fixlen_word`
  (`src/ostream.c:462`). For `element_count == 0`, §4.8 requires **no** `fixlen_word` and
  **no** payload, so even an `NDEBUG` build would emit a malformed empty fixlen array.
- The decoder's `FIXLEN_LEN` path (`src/istream.c:748-749`) assumes a `fixlen_word` always
  follows the count, so a correct zero-count fixlen array would be misparsed if the
  `count == 0` guard were simply removed.

**Fix.**
- Remove the `count == 0` early-reject in `_DECODER_STATE_ARRAY_COUNT`
  (`src/istream.c:718-722`). For a zero-count array, deliver the field event and then
  return to `_DECODER_STATE_IDLE` **without** entering element/`fixlen_word` parsing.
  Specifically, for `SOFAB_TYPE_FIXLENARRAY` with `count == 0`, do **not** transition to
  `_DECODER_STATE_FIXLEN_LEN` (there is no `fixlen_word` on the wire).
- Relax the encoder asserts (`src/ostream.c:338,391,446`) to `element_count >= 0`, and in
  `sofab_ostream_write_array_of_fixlen` skip emitting the `fixlen_word` and the element
  loop entirely when `element_count == 0` (emit only header + count).
- Relax `sofab_istream_read_array`'s `assert(element_count > 0)` (`src/istream.c:780`) so a
  generated repeated-field of length 0 can bind a zero-length destination.

**Files.** `src/istream.c`, `src/ostream.c` (and, for parity, the C++ inline mirrors in
`src/include/sofab/sofab.hpp`).

**Acceptance criteria.**
- Encoding a zero-count unsigned, signed, and fixlen array each produces exactly
  `[ header_varint ] [ 0x00 ]` (no trailing bytes for any of the three; in particular no
  `fixlen_word`).
- Feeding each of those byte sequences to the decoder yields a field event and resumes
  cleanly on the next field — no `E_INVALID_MSG`.
- Round-trip (encode → decode) of a zero-count array of each kind succeeds.

### 2. (High) Tests enforce the old "zero-count is malformed" rule and lack zero-count array coverage (item 12, §7; vectors §8)

**Problem.** `test_msg_invalid_array_count_zero` (`test/c/test_istream.c:546-569`) asserts a
zero-count array decodes to `SOFAB_RET_E_INVALID_MSG` — it now encodes non-conformant
behaviour and will block the item-1 fix. Separately, neither `test/vectorgen/gen_vectors.c`
nor the generated `assets/test_vectors.json` contains a zero-count integer or fixlen array
vector. Because this repo is the **authoritative generator** of the shared vectors, that
omission propagates to every other `corelib-*` port's conformance suite.

**Fix.**
- Convert `test_msg_invalid_array_count_zero` into a positive test (zero-count array decodes
  successfully and resumes on the next field), or replace it with new positive cases.
- Add encoder/decoder/roundtrip unit tests for zero-count unsigned, signed, and fixlen
  arrays (mirroring the existing empty-sequence tests).
- Add generator entries in `gen_vectors.c` — e.g. `array_u32_empty`, `array_i32_empty`,
  `array_fp32_empty` (`[hdr][0]` with no `fixlen_word`) — and regenerate
  `assets/test_vectors.json` so the shared suite gains the coverage. (Regeneration is a
  follow-up build step, not part of this docs-only audit.)

**Files.** `test/c/test_istream.c`, `test/c/test_ostream.c`, `test/vectorgen/gen_vectors.c`,
`assets/test_vectors.json` (regenerated output).

**Acceptance criteria.**
- No test asserts that a zero-count array is malformed.
- The shared `test_vectors.json` contains at least one zero-count vector for an unsigned
  array, a signed array, and a fixlen (fp32/fp64) array, and the encode + decode + chunked
  + roundtrip suite passes on all of them.

### 3. (Medium) CI is not named `ci.yml` and skips the Release configuration (item 17, §12.1)

**Problem.** §12.1 calls for a `ci.yml` that builds **and tests in both Debug and Release**
on every push and PR. The functional CI is split across `build-gcc-x86_64.yaml` /
`build-clang-x86_64.yaml`, and those native legs configure `-DCMAKE_BUILD_TYPE=Debug` only.
A Release build is exercised only on the PowerPC cross job, so a Release-only regression on
the primary toolchains (e.g. `-Os` + `assert`-stripped behaviour — note this is exactly how
the item-6 array asserts vanish) could pass CI.

**Fix.**
- Add a canonical `.github/workflows/ci.yml` (or consolidate the existing build workflows
  into it) running on `push` and `pull_request` to `main`.
- Use `strategy.matrix` over `{ compiler: [gcc, clang] } × { build_type: [Debug, Release] }`
  with `fail-fast: false`; per leg configure, `cmake --build`, then `ctest --output-on-failure`.
- Enable a toolchain/dependency cache (`ccache` or `actions/cache`) per §12.1.
- Keep the feature-matrix and cross-arch workflows as additional legs.

**Files.** New `.github/workflows/ci.yml`; optionally retire `build-gcc-x86_64.yaml`,
`build-clang-x86_64.yaml`; README CI/coverage badge may be repointed.

**Acceptance criteria.**
- A workflow named `ci.yml` triggers on push and PR to `main`.
- The full `ctest` suite passes in **both** Debug and Release for GCC and Clang.
- A build cache is configured and shows hits on re-runs.

### 4. (Low) Docs workflow is not named `docs.yml` and also triggers on pull requests (item 18, §12.2)

**Problem.** §12.2 specifies a `docs.yml` that runs **on push to `main` only**. The
implementation lives in `build-doxygen.yaml` and additionally triggers on `pull_request`.
The deploy job is correctly gated to push-to-`main`, so the live site is never overwritten
by a PR, but the trigger set and file name still diverge from the spec.

**Fix.**
- Rename `build-doxygen.yaml` → `.github/workflows/docs.yml`.
- Remove the `pull_request` trigger (keep `push: branches: [main]`), or document the
  PR-time "docs still build" deviation explicitly.
- Leave the Pages pipeline unchanged — it already matches §12.2.

**Files.** Rename to `.github/workflows/docs.yml`; no other change (README Docs badge already
targets the published site).

**Acceptance criteria.**
- A workflow named `docs.yml` exists, triggers on push to `main` (no `pull_request` unless
  the deviation is documented), and deploys Doxygen HTML to GitHub Pages via the Actions
  deployment (no `gh-pages` branch).

## Low-severity hardening notes (not checklist failures)

- **Expose `SOFAB_MAX_DEPTH` (§6.2 / item 7).** The 255-deep nesting limit is implicit in
  the `uint8_t skip_depth` field. Add a named public constant (`#define SOFAB_MAX_DEPTH 255`)
  and, optionally, an encoder-side depth check in `sofab_ostream_write_sequence_begin`.
- **Decode-side reject for dynamic subtypes in fixlen arrays (item 6).** The encoder's
  `assert(type <= SOFAB_FIXLENTYPE_FP64)` (`src/ostream.c:450`) is compiled out under
  `NDEBUG`; consider returning `SOFAB_RET_E_ARGUMENT` on encode and rejecting a
  string/blob-tagged fixlen array with `SOFAB_RET_E_INVALID_MSG` on decode, independent of
  build flags.
- **Add / link `BENCH_SPEC.md` (§10).** The plan names `BENCH_SPEC.md` as the cross-language
  source of truth for benchmark workloads; it is absent from this repo.
- **README dependency-install snippet (§9 item 2).** Add a short "consume as a dependency"
  example (CMake `FetchContent` / `add_subdirectory` / install target).
