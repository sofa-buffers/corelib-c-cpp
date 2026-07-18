/*!
 * @file vectors_main.c
 * @brief Standalone runner for the shared conformance-vector suite.
 *
 * Unlike the hand-written unit tests (which run only in the full-feature "max"
 * build), this runner is feature-flag tolerant: the shared engine skips vectors
 * that need a capability this build was compiled without (SOFAB_DISABLE_*). That
 * lets CI run the same vector file across every feature configuration. It has no
 * Unity/Catch2 dependency so it links in the most reduced builds.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab_test_vectors.h"

#include <stdio.h>

#ifndef SOFAB_TEST_VECTORS_PATH
#error "SOFAB_TEST_VECTORS_PATH must point at assets/test_vectors.json"
#endif

int main(void)
{
    sofab_test_vectors_result_t r;
    int rc = sofab_test_vectors_run_all(SOFAB_TEST_VECTORS_PATH, &r);

    printf("[vectors] %d vectors, %d run, %d skipped, %d checks, %d failures\n",
           r.vectors, r.vectors - r.skipped, r.skipped, r.checks, r.failures);
    printf("[invalid_utf8] %d negative vectors, %d checks\n",
           r.invalid_vectors, r.invalid_checks);

    if (!r.loaded)
    {
        printf("  load error: %s\n", r.first_error);
        return 1;
    }
    if (r.vectors <= 0)
    {
        printf("  no vectors found\n");
        return 1;
    }
    if (r.vectors - r.skipped <= 0)
    {
        printf("  every vector was skipped — nothing exercised\n");
        return 1;
    }
    if (r.failures)
    {
        printf("  first failure: %s\n", r.first_error);
        return 1;
    }
    return rc == 0 ? 0 : 1;
}
