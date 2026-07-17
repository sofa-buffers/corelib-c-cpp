/*!
 * @file sofab_test_vectors.h
 * @brief Shared conformance-vector engine for the SofaBuffers test harness.
 *
 * Loads the language-agnostic vector suite (assets/test_vectors.json) and, for
 * every vector, drives the real SofaBuffers C encoder/decoder through four
 * scenarios required by ARCHITECTURE.md:
 *
 *   - encode    : replay the vector's fields -> bytes, compare to serialized.hex
 *   - decode    : feed serialized.hex -> recover values, compare to fields
 *   - roundtrip : encode then decode, compare both ways
 *   - chunked   : encode through a tiny flushing buffer; decode one byte at a time
 *
 * The bulk is a POSITIVE/roundtrip suite. General malformed-input and error-path
 * behaviour (truncated varints, unbalanced sequences, overflow) is covered by
 * the hand-written tests in test/c/test_istream.c, since the shared vector file's
 * positive vectors carry only valid encoder output. The one exception is the
 * shared NEGATIVE conformance group: the file's top-level "invalid_utf8" array
 * (invalid-UTF-8 `string` payloads, see assets/test_vectors_README.md). Under a
 * strict build (SOFAB_STRICT_UTF8) the engine asserts each decodes to INVALID and
 * refuses to encode; a non-strict build counts but does not exercise them.
 *
 * The engine is plain C (linked into both the C/Unity and C++/Catch2 test
 * binaries). Both languages call sofab_test_vectors_run_all().
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_TEST_VECTORS_H
#define SOFAB_TEST_VECTORS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int  loaded;            /*!< 1 if the vector file parsed successfully */
    int  vectors;           /*!< number of positive vectors found */
    int  skipped;           /*!< vectors skipped: they require a feature this build lacks */
    int  checks;            /*!< number of (vector, scenario) checks run */
    int  failures;          /*!< number of failed checks (positive + negative) */
    int  invalid_vectors;   /*!< number of negative (invalid-UTF-8) vectors found */
    int  invalid_checks;    /*!< negative-vector checks run (0 in a non-strict build) */
    char first_error[256];  /*!< description of the first failure (or load error) */
} sofab_test_vectors_result_t;

/*!
 * @brief Load @p path and run every scenario over every vector.
 * @return 0 if the file loaded and all checks passed; non-zero otherwise.
 *         Details (counts, first failure) are written to @p out.
 */
int sofab_test_vectors_run_all(const char *path, sofab_test_vectors_result_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SOFAB_TEST_VECTORS_H */
