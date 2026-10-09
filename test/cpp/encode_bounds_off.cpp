/*!
 * @file encode_bounds_off.cpp
 * @brief Compile-only: SOFAB_DISABLE_ENCODE_BOUNDS turns sofab::ENCODE_BOUNDS off.
 *
 * Built as an object of its own (never linked into a test binary, so the two
 * values of the constant never meet in one program). Generated code writes
 * each encode bound check as `if (sofab::ENCODE_BOUNDS && ...)`, so this is
 * the switch that compiles all of them away.
 *
 * SPDX-License-Identifier: MIT
 */

#define SOFAB_DISABLE_ENCODE_BOUNDS
#include "sofab/sofab.hpp"

static_assert(SOFAB_CPP_HAVE_ENCODE_BOUNDS == 0, "the opt-out macro is honoured");
static_assert(!sofab::ENCODE_BOUNDS, "the opt-out turns the generated encode bound checks off");
