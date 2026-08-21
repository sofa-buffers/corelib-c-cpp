/*!
 * @file vectors_main.c
 * @brief Standalone runner for the shared conformance-vector suite.
 *
 * Unlike the hand-written unit tests (which run only in the full-feature "max"
 * build), this runner is feature-flag tolerant: a vector needing a capability
 * this build was compiled without (SOFAB_DISABLE_*) is run as a NEGATIVE case —
 * the build must reject it — instead of being dropped. That lets CI run the same
 * vector file across every feature configuration, and makes the reduced builds
 * assert the contract that defines them. It has no Unity/Catch2 dependency so it
 * links in the most reduced builds.
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

    printf("[vectors] %d vectors, %d decoded, %d asserted rejected, %d checks, %d failures\n",
           r.vectors, r.vectors - r.rejected, r.rejected, r.checks, r.failures);
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
    if (r.checks <= 0)
    {
        printf("  no checks ran — nothing exercised\n");
        return 1;
    }
    if (r.failures)
    {
        printf("  first failure: %s\n", r.first_error);
        return 1;
    }
    return rc == 0 ? 0 : 1;
}
