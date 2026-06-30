# SofaBuffers `corelib-c-cpp` — Conformance Gap Analysis & Remediation Plan

Audit of the C99 / C++20 reference implementation against the language-independent
specification (`CORELIB_PLAN.md`), with particular attention to the **§13
Conformance Checklist**. This repository is the *authoritative source* for the
shared `test_vectors.json` and the reference implementation, but it must still
satisfy the checklist; it is judged on that basis here.

The only change introduced by this audit is this document. No implementation,
test, config, asset, or README file was modified.

## Summary

| Status | Count |
|--------|-------|
| PASS | 16 |
| PARTIAL | 2 |
| GAP | 0 |
| **Total** | **18** |

Both PARTIAL items concern the **GitHub Actions workflows (§12)**: the required
behaviour is implemented and wired into the README, but it is spread across
descriptively-named workflow files rather than the canonical `ci.yml` / `docs.yml`,
and CI builds only the Debug configuration on the native x86_64 legs. Everything
else — wire format, streaming, API, error model, generated-object layer, tests,
assets, README, benchmarks, and the devcontainer — conforms.

## Per-checklist-item findings

| # | Item (§13) | Status | Evidence | Notes |
|---|------------|--------|----------|-------|
| 1 | All public symbols under the `sofab` namespace (§6) | PASS | C uses the `sofab_` prefix throughout (`src/include/sofab/sofab.h`, `ostream.h`, `istream.h`, `object.h`); C++ uses `namespace sofab` (`src/include/sofab/sofab.hpp:78`). | Namespace spelled exactly `sofab`. |
| 2 | API version constant/getter returns `1` (§6) | PASS | `#define SOFAB_API_VERSION 1` (`sofab.h:32`); `inline constexpr int API_VERSION = 1` (`sofab.hpp:81`); asserted by `static_assert(sofab::API_VERSION == 1 ...)` (`test/cpp/test_istream.cpp:310`, `test/cpp/test_ostream.cpp:83`). | — |
| 3 | Varint & zig-zag encode/decode match §4.1–4.2 | PASS | `_varint_encode` (`src/ostream.c:82`); `_varint_decode` with width/overflow guard (`src/istream.c:77-111`); `_zigzag_encode` 64-bit width (`ostream.c:36`); `_zigzag_decode` (`istream.c:60`). Byte output validated by `test_vectors.json`. | Overlong/overflow varints rejected with `E_INVALID_MSG`. |
| 4 | Field header `(id<<3)\|type` and all 8 wire types (§4.3) | PASS | `_type_encode` packs `(var<<3)\|type` (`ostream.c:110`); `sofab_type_t` enumerates 0x0–0x7 (`sofab.h:48-58`); decoder dispatch over all eight in the IDLE state (`istream.c:335-379`). | Tag values are normative and exact. |
| 5 | Fixlen word `(length<<3)\|fixlen_type`, LE floats, UTF-8 strings without terminator, blobs (§4.6) | PASS | `sofab_ostream_write_fixlen` emits `_type_encode(datalen,type)` (`ostream.c:298`); big-endian float byte-reversal on write (`ostream.c:303-323`) and read (`istream.c:644-656`); string written via `strlen`, no terminator (`ostream.h:299-303`); blob path (`ostream.h:317`). | Subtypes fp32/fp64/string/blob handled; reserved subtypes rejected on decode (`istream.c:581`). |
| 6 | Integer arrays + fixlen arrays with a single shared fixlen word; no dynamic subtypes in fixlen arrays (§4.7–4.8) | PASS | `write_array_of_unsigned/signed` (`ostream.c:330,383`); `write_array_of_fixlen` writes count then one shared `_type_encode(element_size,type)` word (`ostream.c:457-465`); decoder reads the shared word once via `ARRAY_COUNT`→`FIXLEN_LEN` (`istream.c:724-751`). | Encoder blocks dynamic subtypes only via `assert(type <= SOFAB_FIXLENTYPE_FP64)` (`ostream.c:450`), which is compiled out under `NDEBUG`; decode has no explicit reject for a string/blob-tagged fixlen array. Hardening note, not a wire-format defect. |
| 7 | Sequence start/end framing, fresh ID scope, single-byte `0x07` end, skip-by-walking with depth tracking, reject nesting beyond `MAX_DEPTH`=255 (§4.9) | PASS | `write_sequence_begin/end` (`ostream.c:500,514`); end is `(0<<3)\|7 == 0x07`; fresh scope via parent-linked decoder chain (`istream.c:791-809`); skip walks with `skip_depth`, rejecting at `UINT8_MAX` (255) → `E_INVALID_MSG` (`istream.c:397-404`); 256-level rejection test (`test/c/test_istream.c:1802` `test_msg_invalid_nested_sequence_depth`). | No public named `SOFAB_MAX_DEPTH` constant (the §6.2 normative value 255 is implicit in the `uint8_t skip_depth`); encoder does not itself cap nesting depth. Minor. |
| 8 | Streaming encode into a smaller-than-message buffer via flush callback / sink, with mid-stream buffer swap (§5.1) | PASS | `_push_byte` flushes when full and resumes (`ostream.c:52-73`); `sofab_ostream_buffer_set` swaps the buffer (`ostream.c:230`); start `offset` honoured (`ostream.c:191-205`); buffer-full without callback returns `E_BUFFER_FULL`. Chunked-encode tested with 1/3/7-byte buffers (`test/shared/sofab_test_vectors.c`). | C++ mirrors via `OStream`/`OStreamInline` + `setBuffer` (`sofab.hpp:551-663`). |
| 9 | Streaming decode via `feed` of arbitrarily small chunks, push-callback / pull-read, lazy binding, auto-skip (§5.2) | PASS | Byte-driven resumable state machine in `sofab_istream_feed` (`istream.c:288-759`); `read_*` only binds a destination lazily (`sofab_istream_read_field`, `istream.c:761`); unbound fields and whole sub-sequences auto-skip. One-byte-at-a-time decode tested (`test/shared/sofab_test_vectors.c`). | — |
| 10 | Result/error reporting follows §6.3 baseline codes (return codes in C/embedded) | PASS | `sofab_ret_t` = OK / E_ARGUMENT / E_USAGE / E_BUFFER_FULL / E_INVALID_MSG (`sofab.h:38-45`), mapping to None/InvalidArgument/UsageError/BufferFull/InvalidMessage; C++ `enum class Error` mirrors them (`sofab.hpp:89-96`). Built `-fno-exceptions` (`CMakeLists.txt:29`). | Return-code model, correct for C / embedded targets. |
| 11 | Streaming primitives suffice for a thin generated-object layer with a dead-simple API that also streams in chunks; one-shot helpers are thin wrappers (§6.1) | PASS | C descriptor transcoder is built purely on the stream API: `sofab_object_encode` calls `sofab_ostream_write_*` (`src/object.c:84-234`) and `sofab_object_field_cb` binds `sofab_istream_read_*` / `read_sequence` (`object.c:236-347`). C++ `OStreamMessage`/`IStreamMessage` + `OStreamObject::serialize()` (one-shot) and `IStreamObject` feed-driven decode (`sofab.hpp:682-1253`). | One-shot encode is `OStreamObject::serialize()`; decode is feed/`->`-driven (no single `T::deserialize(bytes)` static), but both one-shot and chunked paths exist over the same primitives. |
| 12 | All shared test vectors pass encode + decode, plus chunked, roundtrip, malformed, skip (§7) | PASS | Shared engine runs encode, chunked-encode (1/3/7 B), decode, chunked-decode (1 B), skip-ids (+ chunked), roundtrip (`test/shared/sofab_test_vectors.c`); float compares are bit-exact (`eq32`/`eq64`, same file). Malformed cases (truncated/overlong varint, id overflow, oversized fixlen/array, zero-count array, bad fixlen type, extra sequence-end, depth>255) in `test/c/test_istream.c:380-593,1763-1842`. 67 vectors in `assets/test_vectors.json`. | CI runs the suite across the feature matrix (`build-feature-matrix.yaml`). |
| 13 | `assets/` populated per §8 (branding + `test_vectors.json`) | PASS | `assets/sofabuffers_logo.png`, `assets/sofabuffers_icon.png`, `assets/test_vectors.json` (format `sofabuffers-test-vectors` v1, 67 vectors across scalar/array/sequence/composite/skip groups incl. id boundaries), `assets/test_vectors_README.md`. | This repo is the authoritative generator of the vectors. |
| 14 | README follows the family format with badges and required sections (§9) | PASS | Centered logo + tagline + org link, `## SofaBuffers C/C++ library` with coverage + Docs + per-target build badges, `## Why this design` table, Usage (basic encode/decode **and** flush-callback streaming larger than the buffer), API summary, Feature flags table, Build & test, Benchmarks (`README.md:1-477`). | No package-manager install command (C/C++ has no canonical registry, so the §6 `SofaBuffers` package-name / `cargo add`-style install is not literally applicable); a "consume as a dependency" snippet (FetchContent/add_subdirectory) would close the §9-item-2 install nuance. Minor. |
| 15 | `perf` (CPU-independent) and `bench` (MB/s) tools present and runnable (§10) | PASS | `bench/c/perf.c`, `bench/c/bench.c`, `bench/cpp/perf.cpp`, `bench/cpp/bench.cpp`; CMake `run_perf` / `run_bench` / `run_bench_callgrind` targets (`bench/CMakeLists.txt:41-61`); documented in `README.md:405-428`. | `BENCH_SPEC.md` (referenced by §10 as the cross-language source of truth) is not present in the repo; checklist item only requires the tools, so PASS, but adding/​linking `BENCH_SPEC.md` would complete §10. Minor. |
| 16 | `.devcontainer/` with all required files; extensions incl. `anthropic.claude-code`; `.devcontainer/.env` gitignored (§11) | PASS | `Dockerfile` (Ubuntu 24.04 + toolchain + `gh` + Node LTS + `@anthropic-ai/claude-code`), `build.sh`, `start.sh` (loads `.env` via `--env-file`, warns if absent), `attach.sh`, `devcontainer.json` (cpptools/cmake + `anthropic.claude-code`, `--env-file` in `runArgs`), `.env.example` (documented `GH_TOKEN`). Image/container name `cpp-devcontainer` (`build.sh:6`, `start.sh:17`). `.env` is gitignored (`.devcontainer/.gitignore:6`, root `.gitignore:4`) and **not tracked** (verified via `git ls-files`). | — |
| 17 | `ci.yml` builds and tests on push and PR; matrix where versions matter; coverage uploaded and badge in README (§12.1) | PARTIAL | Push+PR CI exists as `build-gcc-x86_64.yaml` and `build-clang-x86_64.yaml`; compiler/feature/arch matrix in `build-feature-matrix.yaml` / `build-cpp-feature-matrix.yaml` (`fail-fast: false`); coverage in `coverage.yaml` (gcovr → badge JSON pushed to `badges` branch) wired into `README.md:12`. | No file literally named `ci.yml`; native x86_64 legs configure **Debug only** (spec §12.1 step 4 wants both Debug *and* Release — Release is only exercised on the PowerPC cross job); no toolchain dependency cache enabled; coverage is self-hosted on a `badges` branch rather than Codecov ("or equivalent", acceptable). |
| 18 | `docs.yml` generates HTML docs and publishes to GitHub Pages via Actions-based deploy; Docs badge links to the site (§12.2) | PARTIAL | `build-doxygen.yaml` (workflow display name "Docs"): Doxygen build → `actions/upload-pages-artifact@v3` → `actions/deploy-pages@v4`, deploy gated to push-to-`main`, with `pages: write` / `id-token: write` permissions; Docs badge → `https://sofa-buffers.github.io/corelib-c-cpp/` (`README.md:13`). No `gh-pages` branch used. | No file literally named `docs.yml`; the workflow also triggers on `pull_request` (build-only; deploy is gated), whereas §12.2 specifies push-to-`main` only. Behaviour is otherwise compliant. |

## Remediation Plan

Ordered by severity. All items are PARTIAL; there are no GAP-level (missing-capability)
defects. Acceptance criteria are written so each can be verified independently.

### 1. (Medium) CI is not named `ci.yml` and skips the Release configuration (item 17, §12.1)

**Problem.** The checklist and §12.1 call for a `ci.yml` that builds **and tests in
both Debug and Release** on every push and PR. The functional CI is split across
`build-gcc-x86_64.yaml` / `build-clang-x86_64.yaml`, and those native legs configure
`-DCMAKE_BUILD_TYPE=Debug` only (`build-gcc-x86_64.yaml:26`, `build-clang-x86_64.yaml:27`).
A Release build is exercised only on the PowerPC cross job, so a Release-only
regression on the primary toolchains (e.g. `-Os` + `-fno-exceptions`/`assert`-stripped
behaviour, such as the fixlen-array subtype `assert` in item 6) could pass CI.

**Fix.**
- Add a canonical `.github/workflows/ci.yml` (or rename/consolidate the existing
  build workflows into it) that runs on `push` and `pull_request` to `main`.
- Use a `strategy.matrix` over `{ compiler: [gcc, clang] } × { build_type: [Debug, Release] }`
  with `fail-fast: false`.
- For each leg: configure, `cmake --build`, then `ctest --output-on-failure`.
- Enable the toolchain/dependency cache (e.g. `ccache` or `actions/cache` keyed on the
  CMake config) per §12.1.
- Keep the feature-matrix and cross-arch workflows as additional legs (they exceed the
  baseline and are valuable).

**Files.** New `.github/workflows/ci.yml`; optionally retire/redirect
`build-gcc-x86_64.yaml`, `build-clang-x86_64.yaml`. (Do not change `assets/`,
`README.md` badge target may be repointed to the new workflow.)

**Acceptance criteria.**
- A workflow file named `ci.yml` exists and triggers on push and PR to `main`.
- The full `ctest` suite runs and passes in **both** Debug and Release for GCC and Clang.
- A dependency/build cache is configured and shows cache hits on re-runs.
- The README CI/coverage badge points at the canonical CI workflow.

### 2. (Low) Docs workflow is not named `docs.yml` and also triggers on pull requests (item 18, §12.2)

**Problem.** §12.2 specifies a `docs.yml` that runs **on push to `main` only**. The
implementation lives in `build-doxygen.yaml` (display name "Docs") and additionally
triggers on `pull_request` (`build-doxygen.yaml:6-7`). The deploy job is correctly
gated to push-to-`main`, so the live site is never overwritten by a PR, but the trigger
set still diverges from the spec and the file name is non-canonical.

**Fix.**
- Rename `build-doxygen.yaml` → `.github/workflows/docs.yml`.
- Remove the `pull_request` trigger (keep `push: branches: [main]`), or, if PR-time
  "docs still build" validation is desired, document that deviation explicitly.
- Leave the Pages pipeline (`upload-pages-artifact@v3` → `deploy-pages@v4`,
  `pages: write` / `id-token: write`) unchanged — it already matches §12.2.

**Files.** Rename to `.github/workflows/docs.yml`; no other file changes required
(the README Docs badge already targets `https://sofa-buffers.github.io/corelib-c-cpp/`).

**Acceptance criteria.**
- A workflow file named `docs.yml` exists.
- It triggers on push to `main` (no `pull_request` trigger, unless the deviation is
  documented in the workflow header).
- Doxygen HTML is uploaded and deployed to GitHub Pages via the Actions deployment
  (no `gh-pages` branch); the Docs badge resolves to the published site.

## Low-severity hardening notes (not checklist failures)

These do not block conformance but would tighten alignment with the spec text:

- **Expose `SOFAB_MAX_DEPTH` (§6.2 / item 7).** The 255-deep nesting limit is currently
  implicit in the `uint8_t skip_depth` field. Add a named public constant
  (`#define SOFAB_MAX_DEPTH 255`) and, optionally, an encoder-side depth check in
  `sofab_ostream_write_sequence_begin` so over-deep encoding is reported rather than
  relying on the peer decoder to reject it.
- **Decode-side reject for dynamic subtypes in fixlen arrays (item 6).** The encoder's
  `assert(type <= SOFAB_FIXLENTYPE_FP64)` (`src/ostream.c:450`) is compiled out under
  `NDEBUG`; consider returning `SOFAB_RET_E_ARGUMENT` on encode and rejecting a
  string/blob-tagged fixlen array with `SOFAB_RET_E_INVALID_MSG` on decode for a
  defence-in-depth guarantee independent of build flags.
- **Add / link `BENCH_SPEC.md` (§10).** The plan names `BENCH_SPEC.md` as the
  cross-language source of truth for benchmark workloads; it is absent from this repo.
  Adding it (or linking the canonical copy) makes the `perf`/`bench` numbers formally
  comparable across ports.
- **README dependency-install snippet (§9 item 2).** Add a short "consume as a
  dependency" example (CMake `FetchContent` / `add_subdirectory` / install target) to
  cover the spec's install-command requirement, since C/C++ has no package registry for
  the `SofaBuffers` package name.
