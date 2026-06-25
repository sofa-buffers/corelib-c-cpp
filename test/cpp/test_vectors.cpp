/*!
 * @file test_vectors.cpp
 * @brief Validates the encoder/decoder against the shared conformance vectors.
 *
 * Runs the same shared engine as the C harness (test/shared/sofab_test_vectors)
 * inside the C++ test binary, so the vectors are exercised in the C++ build too.
 * Positive/roundtrip conformance only; malformed/error paths are covered by the
 * hand-written tests in test_istream.cpp / test_istream.c.
 *
 * SPDX-License-Identifier: MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "sofab_test_vectors.h"

TEST_CASE("shared conformance vectors: encode/decode/roundtrip/chunked", "[vectors]")
{
    sofab_test_vectors_result_t r;
    sofab_test_vectors_run_all(SOFAB_TEST_VECTORS_PATH, &r);

    INFO("vectors=" << r.vectors << " checks=" << r.checks
                    << " failures=" << r.failures << " first_error=" << r.first_error);

    REQUIRE(r.loaded);
    REQUIRE(r.vectors > 0);
    REQUIRE(r.checks > 0);
    CHECK(r.failures == 0);
}
