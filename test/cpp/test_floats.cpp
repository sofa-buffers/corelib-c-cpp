/*!
 * @file test_floats.cpp
 * @brief SofaBuffers test for the float-array bit-pattern equality (floats.hpp)
 *
 * SPDX-License-Identifier: MIT
 *
 * The generated encoder omits a field iff it equals its default, and floats
 * round-trip bit for bit (CORELIB_PLAN §4.6), so "equals" is a comparison of
 * IEEE-754 bit patterns. Nothing on the wire shows an array compared by value
 * instead: the shared vectors carry the bytes, not the omission decision. The
 * -0.0 / NaN cases are therefore pinned here.
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <span>
#include <vector>

namespace
{

template <typename T> struct Bits;
template <> struct Bits<float> { using U = uint32_t; };
template <> struct Bits<double> { using U = uint64_t; };

template <typename T>
typename Bits<T>::U bitsOf(T v)
{
    typename Bits<T>::U u;
    std::memcpy(&u, &v, sizeof u);
    return u;
}

template <typename T>
T fromBits(typename Bits<T>::U u)
{
    T v;
    std::memcpy(&v, &u, sizeof v);
    return v;
}

//! The specification, written as a plain loop: no IEEE ==, no memcmp.
template <typename T>
bool refEqual(const std::vector<T> &a, const std::vector<T> &b)
{
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (bitsOf(a[i]) != bitsOf(b[i])) { return false; }
    }
    return true;
}

//! Deterministic xorshift64, so a failure reproduces.
struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    uint64_t next()
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
};

template <typename T>
std::vector<T> ramp(std::size_t n)
{
    std::vector<T> v(n);
    for (std::size_t i = 0; i < n; ++i) { v[i] = static_cast<T>(i) * T(0.5) + T(1); }
    return v;
}

} // namespace

TEMPLATE_TEST_CASE("bitsEqual: empty, single, equal, self", "[floats]", float, double)
{
    using T = TestType;
    const std::vector<T> none;
    CHECK(sofab::bitsEqual(none, none));
    CHECK(sofab::bitsEqual(none, std::vector<T>{}));
    CHECK(sofab::bitsEqual<T>(nullptr, 0, nullptr, 0));

    CHECK(sofab::bitsEqual(std::vector<T>{T(1.5)}, std::vector<T>{T(1.5)}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{T(1.5)}, std::vector<T>{T(2.5)}));

    const auto v = ramp<T>(5);
    CHECK(sofab::bitsEqual(v, v));
    CHECK(sofab::bitsEqual(v, ramp<T>(5)));
}

TEMPLATE_TEST_CASE("bitsEqual: -0.0 differs from +0.0 at every position", "[floats]", float, double)
{
    using T = TestType;
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{T(-0.0), T(1.5)}, std::vector<T>{T(0.0), T(1.5)}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{T(1.0), T(-0.0), T(2.0)},
                                 std::vector<T>{T(1.0), T(0.0), T(2.0)}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{T(0.0), T(1.5), T(-0.0)},
                                 std::vector<T>{T(0.0), T(1.5), T(0.0)}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{T(0.0)}, std::vector<T>{T(-0.0)}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{T(-0.0)}, std::vector<T>{T(0.0)}));
    CHECK(sofab::bitsEqual(std::vector<T>{T(-0.0)}, std::vector<T>{T(-0.0)}));
    // The IEEE comparison it replaces calls them equal; that is the bug.
    CHECK(std::vector<T>{T(-0.0), T(1.5)} == std::vector<T>{T(0.0), T(1.5)});
}

TEMPLATE_TEST_CASE("bitsEqual: NaN equals itself only bit for bit", "[floats]", float, double)
{
    using T = TestType;
    using U = typename Bits<T>::U;
    const T qnan = std::numeric_limits<T>::quiet_NaN();
    const T snan = std::numeric_limits<T>::signaling_NaN();
    const U qbits = bitsOf(qnan);
    const T payload = fromBits<T>(qbits ^ U(1)); // differs on every NaN encoding (MIPS legacy has the low bit set)
    const T negnan = fromBits<T>(qbits | (U(1) << (sizeof(U) * 8 - 1)));

    CHECK(sofab::bitsEqual(std::vector<T>{qnan}, std::vector<T>{qnan}));
    CHECK(sofab::bitsEqual(std::vector<T>{qnan}, std::vector<T>{fromBits<T>(qbits)}));
    CHECK(sofab::bitsEqual(std::vector<T>{snan}, std::vector<T>{snan}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{qnan}, std::vector<T>{payload}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{qnan}, std::vector<T>{negnan}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{qnan}, std::vector<T>{snan}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{qnan}, std::vector<T>{T(1)}));
    // IEEE == says a NaN never equals itself; the bit pattern says it does.
    CHECK(qnan != qnan);
}

TEMPLATE_TEST_CASE("bitsEqual: infinities and subnormals", "[floats]", float, double)
{
    using T = TestType;
    const T inf = std::numeric_limits<T>::infinity();
    const T tiny = std::numeric_limits<T>::denorm_min();
    CHECK(sofab::bitsEqual(std::vector<T>{inf, -inf}, std::vector<T>{inf, -inf}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{inf}, std::vector<T>{-inf}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{-inf}, std::vector<T>{inf}));
    CHECK(sofab::bitsEqual(std::vector<T>{tiny, -tiny}, std::vector<T>{tiny, -tiny}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{tiny}, std::vector<T>{T(0)}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{tiny}, std::vector<T>{-tiny}));
    CHECK_FALSE(sofab::bitsEqual(std::vector<T>{tiny}, std::vector<T>{fromBits<T>(2)}));
}

TEMPLATE_TEST_CASE("bitsEqual: a length mismatch is unequal, either way round", "[floats]", float, double)
{
    using T = TestType;
    const std::vector<T> two{T(1), T(2)};
    const std::vector<T> three{T(1), T(2), T(3)};
    const std::vector<T> none;
    CHECK_FALSE(sofab::bitsEqual(two, three));
    CHECK_FALSE(sofab::bitsEqual(three, two));
    CHECK_FALSE(sofab::bitsEqual(none, two));
    CHECK_FALSE(sofab::bitsEqual(two, none));
    // The pointer form compares the lengths first, so a short b is never read
    // out to a's length.
    CHECK_FALSE(sofab::bitsEqual<T>(three.data(), 3, two.data(), 2));
    CHECK_FALSE(sofab::bitsEqual<T>(two.data(), 2, three.data(), 3));
}

TEMPLATE_TEST_CASE("bitsEqual: one differing element in a long array", "[floats]", float, double)
{
    using T = TestType;
    for (std::size_t n : {std::size_t{65}, std::size_t{256}, std::size_t{4097}})
    {
        const auto base = ramp<T>(n);
        CHECK(sofab::bitsEqual(base, ramp<T>(n)));
        for (std::size_t at : {std::size_t{0}, n / 2, n - 1})
        {
            auto other = base;
            other[at] = fromBits<T>(bitsOf(other[at]) ^ 1); // the last mantissa bit
            CHECK_FALSE(sofab::bitsEqual(base, other));
            CHECK_FALSE(sofab::bitsEqual(other, base));

            auto zero = base;
            auto negz = base;
            zero[at] = T(0.0);
            negz[at] = T(-0.0);
            CHECK_FALSE(sofab::bitsEqual(zero, negz));
        }
    }
}

TEMPLATE_TEST_CASE("bitsEqual: takes the containers a generated default test passes", "[floats]", float, double)
{
    using T = TestType;
    const sofab::InlineVector<T, 3> inl = {T(0.0), T(1.5)};
    CHECK(sofab::bitsEqual(inl, sofab::InlineVector<T, 3>{T(0.0), T(1.5)}));
    CHECK_FALSE(sofab::bitsEqual(inl, sofab::InlineVector<T, 3>{T(-0.0), T(1.5)}));

    const std::vector<T> vec = {T(0.0), T(1.5)};
    CHECK(sofab::bitsEqual(vec, std::vector<T>{T(0.0), T(1.5)}));
    CHECK_FALSE(sofab::bitsEqual(vec, std::vector<T>{T(-0.0), T(1.5)}));

    // A mutable field against a constant default of a different container type.
    static constexpr std::array<T, 2> dflt = {T(0.0), T(1.5)};
    CHECK(sofab::bitsEqual(inl, dflt));
    CHECK(sofab::bitsEqual(vec, std::span<const T>(dflt)));
    CHECK(sofab::bitsEqual(vec, std::initializer_list<T>{T(0.0), T(1.5)}));
    CHECK_FALSE(sofab::bitsEqual(vec, std::initializer_list<T>{T(0.0), T(-1.5)}));
}

TEMPLATE_TEST_CASE("bitsEqual: agrees with a plain bit loop on random input", "[floats]", float, double)
{
    using T = TestType;
    using U = typename Bits<T>::U;
    Rng rng;
    for (int round = 0; round < 2000; ++round)
    {
        const std::size_t n = rng.next() % 40;
        std::vector<T> a(n);
        for (auto &x : a) { x = fromBits<T>(static_cast<U>(rng.next())); } // any pattern, NaNs included
        std::vector<T> b = a;
        switch (rng.next() % 4)
        {
            case 0: break;                                              // identical
            case 1: if (n) { b[rng.next() % n] = fromBits<T>(static_cast<U>(rng.next())); } break;
            case 2: if (n) { b[rng.next() % n] = -b[rng.next() % n]; } break;
            default: b.push_back(T(0)); break;                          // length differs
        }
        CHECK(sofab::bitsEqual(a, b) == refEqual(a, b));
        CHECK(sofab::bitsEqual(b, a) == refEqual(b, a));
    }
}
