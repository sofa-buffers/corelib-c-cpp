/*!
 * @file test_encode_bounds.cpp
 * @brief The encode-side bound refusal hook and the static storage's clamp.
 *
 * A value past a bound only the schema knows (a string's maxlen, an array's
 * count) is refused at encode by the GENERATED code, which calls
 * OStreamImpl::rejectArgument: the corelib holds no bound, it only latches
 * InvalidArgument so the stream verdict reports it. The static storage
 * (FixedString / FixedBytes / InlineVector) clamps at ASSIGNMENT instead, by
 * contract: the first N bytes or elements are kept, the rest is dropped, and a
 * string is never cut inside a UTF-8 character.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

static_assert(sofab::ENCODE_BOUNDS, "encode bound checks are ON unless SOFAB_DISABLE_ENCODE_BOUNDS");

namespace
{
    /* A message whose serialize() refuses its value the way generated code does. */
    struct RefusingMsg : sofab::OStreamMessage
    {
        std::string_view s;
        sofab::OStreamImpl::Result serialize(sofab::OStreamImpl &os) const noexcept override
        {
            (void)os.write(0, uint32_t{1});
            if (sofab::ENCODE_BOUNDS && s.size() > 4) { return os.rejectArgument(); }
            return os.write(1, s);
        }
        sofab::OStreamImpl::Result run(sofab::OStreamImpl &os) const noexcept { return serialize(os); }
    };
}

TEST_CASE("rejectArgument: the Result and the stream verdict carry InvalidArgument")
{
    std::array<uint8_t, 32> buf{};
    sofab::OStreamView os{buf.data(), buf.size()};
    (void)os.write(1, uint32_t{1});
    const size_t before = os.bytesUsed();
    auto r = os.rejectArgument();
    CHECK(!r.ok());
    CHECK(r == sofab::Error::InvalidArgument);
    CHECK(!os.ok());
    CHECK(os.error() == sofab::Error::InvalidArgument);
    CHECK(os.bytesUsed() == before);
    (void)os.write(2, uint32_t{2});
    CHECK(os.error() == sofab::Error::InvalidArgument);
}

TEST_CASE("rejectArgument: the first latched failure is kept")
{
    std::array<uint8_t, 2> buf{};
    sofab::OStreamView os{buf.data(), buf.size()};
    (void)os.write(1, std::string_view{"too long for two bytes"});
    REQUIRE(os.error() == sofab::Error::BufferFull);
    auto r = os.rejectArgument();
    CHECK(r == sofab::Error::InvalidArgument);
    CHECK(os.error() == sofab::Error::BufferFull);
}

TEST_CASE("rejectArgument: a refusing serialize() fails the encode, a value at its bound encodes")
{
    RefusingMsg m;
    m.s = "abcde";
    std::array<uint8_t, 32> buf{};
    sofab::OStreamView bad{buf.data(), buf.size()};
    CHECK(!m.run(bad).ok());
    CHECK(bad.error() == sofab::Error::InvalidArgument);

    m.s = "abcd";
    sofab::OStreamView good{buf.data(), buf.size()};
    CHECK(m.run(good).ok());
    CHECK(good.ok());
    CHECK(good.bytesUsed() == 8);
}

TEST_CASE("FixedString: an over-long value is clamped at a UTF-8 character boundary")
{
    CHECK(sofab::FixedString<4>{"xxxxx"}.view() == "xxxx");

    sofab::FixedString<4> two{"xxx\xC3\xA9"};               /* xxx + U+00E9 */
    CHECK(two.view() == "xxx");
    CHECK(two.c_str()[3] == '\0');

    CHECK(sofab::FixedString<4>{std::string_view{"\xC3\xA9\xC3\xA9\xC3\xA9"}}.view() == "\xC3\xA9\xC3\xA9");

    sofab::FixedString<4> four;
    four = std::string_view{"a\xF0\x9F\x98\x80"};            /* a + U+1F600 */
    CHECK(four.view() == "a");

    CHECK(sofab::FixedString<5>{"a\xF0\x9F\x98\x80"}.size() == 5);
    CHECK(sofab::FixedString<3>{"\xF0\x9F\x98\x80"}.empty());
    CHECK(sofab::FixedString<2>{"a\xE2\x82\xAC"}.view() == "a"); /* a + U+20AC */

    sofab::FixedString<4> edge;
    edge = "\xC3\xA9xyz";
    CHECK(edge.view() == "\xC3\xA9xy");
}

TEST_CASE("InlineVector / FixedBytes: past N the value is dropped, the held ones stay")
{
    sofab::InlineVector<uint32_t, 3> v;
    for (uint32_t x : {1u, 2u, 3u, 4u, 5u}) v.push_back(x);
    REQUIRE(v.size() == 3);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);

    sofab::InlineVector<sofab::FixedString<4>, 3> t;
    for (const char *x : {"a", "b", "c", "d"}) t.push_back(sofab::FixedString<4>{x});
    const sofab::FixedString<4> keep{"zz"};
    t.push_back(keep);
    REQUIRE(t.size() == 3);
    CHECK(t[2].view() == "c");

    sofab::InlineVector<uint16_t, 2> w{7, 8, 9};
    CHECK(w.size() == 2);
    CHECK(w[1] == 8);

    sofab::FixedBytes<4> b{1, 2, 3, 4, 5};
    for (int i = 0; i < 3; ++i) b.push_back(9);
    CHECK(b.size() == 4);
    CHECK(b.data()[3] == 4);
}
