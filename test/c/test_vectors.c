/*!
 * @file test_vectors.c
 * @brief Validates the encoder/decoder against the shared conformance vectors.
 *
 * Loads assets/test_vectors.json and runs encode / decode / roundtrip / chunked
 * scenarios for every vector (see test/shared/sofab_test_vectors.h). This is the
 * positive/roundtrip conformance suite required by ARCHITECTURE.md; malformed and
 * error-path behaviour is covered separately by the hand-written tests in
 * test_istream.c.
 *
 * The whole file is compiled to a no-op unless SOFAB_TEST_VECTORS is defined
 * (host / hosted-Linux builds only — bare-metal targets have no filesystem).
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

#if defined(SOFAB_TEST_VECTORS)

#include "sofab_test_vectors.h"

#include <stdio.h>

static void test_shared_test_vectors(void)
{
    sofab_test_vectors_result_t r;
    sofab_test_vectors_run_all(SOFAB_TEST_VECTORS_PATH, &r);

    printf("  [vectors] %d vectors, %d checks, %d failures\n", r.vectors, r.checks, r.failures);

    TEST_ASSERT_TRUE_MESSAGE(r.loaded, r.first_error);
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, r.vectors, "no vectors found in test_vectors.json");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, r.failures, r.first_error);
}

int test_vectors_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_shared_test_vectors);
    return UNITY_END();
}

#else /* !SOFAB_TEST_VECTORS */

int test_vectors_main(void)
{
    return 0; /* shared vectors require a filesystem; skipped on this target */
}

#endif /* SOFAB_TEST_VECTORS */
