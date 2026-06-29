/*!
 * @file test_istream.cpp
 * @brief SofaBuffers test for input stream C++ API
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <array>
#include <valarray>
#include <vector>
#include <span>
#include <cstdint>
#include <limits>

//

#if 0
static void hexdump(const void *data, size_t size)
{
    const unsigned char *byte = (const unsigned char *)data;
    for (size_t i = 0; i < size; i += 16)
    {
        printf("%08zx  ", i);
        for (size_t j = 0; j < 16; j++)
        {
            if (i + j < size)
                printf("%02x ", byte[i + j]);
            else
                printf("   ");
        }
        printf(" ");
        for (size_t j = 0; j < 16; j++)
        {
            if (i + j < size)
            {
                unsigned char c = byte[i + j];
                printf("%c", (c >= 32 && c <= 126) ? c : '.');
            }
        }
        printf("\n");
    }
}
#endif

//

class SimpleObject2 : public sofab::IStreamMessage
{
    struct Data
    {
        uint32_t id = 0;
        float value = 0;
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t size, size_t count) noexcept override
    {
        (void)size;
        (void)count;

        switch (id)
        {
            case 1:
                istream.read(data_.id);
                break;
            case 2:
                istream.read(data_.value);
                break;
        }
    }

public:
    Data* operator ->() noexcept
    {
        return &data_;
    }

    const Data* operator ->() const noexcept
    {
        return &data_;
    }
};

class DynArrayOfUnsigned : public sofab::IStreamMessage
{
    struct Data
    {
        std::vector<uint32_t> values;
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t size, size_t count) noexcept override
    {
        (void)id;
        (void)size;
        (void)count;

        data_.values.emplace_back(0);
        istream.read(data_.values.back());
    }

public:
    Data* operator ->() noexcept
    {
        return &data_;
    }

    const Data* operator ->() const noexcept
    {
        return &data_;
    }
};

class DynArrayOfStrings : public sofab::IStreamMessage
{
    struct Data
    {
        std::vector<std::string> values;
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t size, size_t count) noexcept override
    {
        (void)id;
        (void)size;
        (void)count;

        data_.values.emplace_back(size, '\0');
        istream.read(data_.values.back());
    }

public:
    Data* operator ->() noexcept
    {
        return &data_;
    }

    const Data* operator ->() const noexcept
    {
        return &data_;
    }
};

// Exercises the wrapper's blob and variable-length-array decode entry points:
//   - a scalar blob via read(void*, size_t)            (BLOB type tag)
//   - a sequence of strings via read(vector<string>&)
//   - a sequence of blobs via read(vector<vector<uint8_t>>&)
// All three are the paths the deferred C decoder used to drop bytes on or crash.
class BlobAndVarArrays : public sofab::IStreamMessage
{
    struct Data
    {
        std::vector<uint8_t>              blob;
        std::vector<std::string>          strings;
        std::vector<std::vector<uint8_t>> blobs;
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t size, size_t count) noexcept override
    {
        (void)count;

        switch (id)
        {
            case 1:
                data_.blob.resize(size);
                istream.read(data_.blob.data(), data_.blob.size());
                break;
            case 2:
                istream.read(data_.strings);
                break;
            case 3:
                istream.read(data_.blobs);
                break;
        }
    }

public:
    Data* operator ->() noexcept
    {
        return &data_;
    }

    const Data* operator ->() const noexcept
    {
        return &data_;
    }
};

class FullObject : public sofab::IStreamMessage
{
public:
    struct Data
    {
        int8_t	    i8 = 0;
        uint8_t     u8 = 0;
        int16_t	    i16 = 0;
        uint16_t    u16 = 0;
        int32_t	    i32 = 0;
        uint32_t    u32 = 0;
        int64_t	    i64 = 0;
        uint64_t    u64 = 0;
        bool	    boolean = false;
        float	    fp32 = 0.0f;
        double	    fp64 = 0.0;
        std::string	str;

        std::vector<int8_t> i8Array{};
        std::array<uint8_t, 5> u8Array{};
        std::array<int16_t, 5> i16Array{};
        std::array<uint16_t, 5> u16Array{};
        std::array<int32_t, 5> i32Array{};
        std::array<uint32_t, 5> u32Array{};
        std::array<int64_t, 5> i64Array{};
        std::array<uint64_t, 5> u64Array{};
        // std::array<bool, 5> boolArray{};
        std::array<float, 5> fp32Array{};
        std::array<double, 5> fp64Array{};
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t size, size_t count) noexcept override
    {
        (void)size;
        (void)count;

        switch (id)
        {
            case 1: istream.read(data_.i8); break;
            case 2: istream.read(data_.u8); break;
            case 3: istream.read(data_.i16); break;
            case 4: istream.read(data_.u16); break;
            case 5: istream.read(data_.i32); break;
            case 6: istream.read(data_.u32); break;
            case 7: istream.read(data_.i64); break;
            case 8: istream.read(data_.u64); break;
            case 9: istream.read(data_.boolean); break;
            case 10: istream.read(data_.fp32); break;
            case 11: istream.read(data_.fp64); break;
            case 12:
                data_.str.resize(std::min(size, static_cast<size_t>(32)));
                istream.read(data_.str);
                break;

            case 13:
                data_.i8Array.resize(std::min(size, static_cast<size_t>(32)));
                istream.read(data_.i8Array);
                break;

            case 14: istream.read(data_.u8Array); break;
            case 15: istream.read(data_.i16Array); break;
            case 16: istream.read(data_.u16Array); break;
            case 17: istream.read(data_.i32Array); break;
            case 18: istream.read(data_.u32Array); break;
            case 19: istream.read(data_.i64Array); break;
            case 20: istream.read(data_.u64Array); break;
            // case 21: istream.read(data_.boolArray); break;
            case 22: istream.read(data_.fp32Array); break;
            case 23: istream.read(data_.fp64Array); break;
        }
    }
};

// Nested-message decode: a child IStreamMessage embedded in a parent. The parent
// binds the child via istream.read(child) on a sequence field, exercising the
// IStreamImpl::read<T> InputMessage branch (sofab_istream_read_sequence).

class NestedChild : public sofab::IStreamMessage
{
public:
    struct Data
    {
        uint32_t id = 0;
        float    value = 0.0f;
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t, size_t) noexcept override
    {
        switch (id)
        {
            case 1: istream.read(data_.id); break;
            case 2: istream.read(data_.value); break;
        }
    }
};

class NestedParent : public sofab::IStreamMessage
{
public:
    struct Data
    {
        uint32_t    header = 0;
        NestedChild child;
        uint32_t    footer = 0;
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t, size_t) noexcept override
    {
        switch (id)
        {
            case 1: istream.read(data_.header); break;
            case 2: istream.read(data_.child); break;   // nested sequence
            case 3: istream.read(data_.footer); break;
        }
    }
};

//

static_assert(sofab::API_VERSION == 1, "API version mismatch");

//

TEST_CASE("IStream: inline feed buffer")
{
    uint8_t value = 0x55;

    sofab::IStreamInline istream{
        [&](sofab::id id, size_t size, size_t count) noexcept
        {
            (void)size;
            (void)count;

            if (id == 0)
            {
                istream.read(value);
            }
        }
    };

    const uint8_t buffer[] = {0x00, 0x7F};
    auto result = istream.feed(buffer, sizeof(buffer));
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(value == 127);
}

TEST_CASE("IStream: inline feed buffer stream")
{
    std::string value;

    sofab::IStreamInline istream{
        [&](sofab::id id, size_t size, size_t count) noexcept
        {
            (void)count;

            if (id == 0)
            {
                value.resize(size);
                istream.read(value);
            }
        }
    };

    const uint8_t buffer[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};

    for (size_t i = 0; i < sizeof(buffer); i++)
    {
        auto result = istream.feed(&buffer[i], 1);
        REQUIRE(result.code() == sofab::Error::None);
    }

    REQUIRE(value == "Hello Couch!");
}

TEST_CASE("IStream: object feed buffer")
{
    sofab::IStreamObject<SimpleObject2> istream;

    const uint8_t buffer[] = {0x08, 0x2a, 0x12, 0x20, 0x56, 0x0e, 0x49, 0x40};

    auto result = istream.feed(buffer, sizeof(buffer));
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(istream->id == 42);
    REQUIRE(istream->value == 3.1415f);
}

//
// Round-trip tests: serialize with the OStream API, feed the produced bytes
// back through the IStream API and verify the decoded values. These exercise
// the input/deserialization paths (signed varint / zigzag decode, fp64, bool,
// arrays of every type) that the byte-literal tests above leave untouched.
//

TEST_CASE("IStream: round-trip all scalar types into object")
{
    sofab::OStream ostream{256};

    ostream
        .write(1,  static_cast<int8_t>(-5))
        .write(2,  static_cast<uint8_t>(200))
        .write(3,  static_cast<int16_t>(-12345))
        .write(4,  static_cast<uint16_t>(60000))
        .write(5,  std::numeric_limits<int32_t>::min())
        .write(6,  std::numeric_limits<uint32_t>::max())
        .write(7,  std::numeric_limits<int64_t>::min())
        .write(8,  std::numeric_limits<uint64_t>::max())
        .write(9,  true)
        .write(10, 3.1415f)
        .write(11, 2.718281828459045)
        .write(12, std::string_view{"sofa"});

    const auto used = ostream.bytesUsed();

    sofab::IStreamObject<FullObject> istream;
    auto result = istream.feed(ostream.data(), used);

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(result);                                // IStream::Result::operator bool

    const auto &d = (*istream).data_;
    REQUIRE(d.i8  == static_cast<int8_t>(-5));
    REQUIRE(d.u8  == static_cast<uint8_t>(200));
    REQUIRE(d.i16 == static_cast<int16_t>(-12345));
    REQUIRE(d.u16 == static_cast<uint16_t>(60000));
    REQUIRE(d.i32 == std::numeric_limits<int32_t>::min());
    REQUIRE(d.u32 == std::numeric_limits<uint32_t>::max());
    REQUIRE(d.i64 == std::numeric_limits<int64_t>::min());
    REQUIRE(d.u64 == std::numeric_limits<uint64_t>::max());
    REQUIRE(d.boolean == true);
    REQUIRE(d.fp32 == 3.1415f);
    REQUIRE(d.fp64 == 2.718281828459045);
    REQUIRE(d.str == "sofa");
}

TEST_CASE("IStream: round-trip arrays of every type into object")
{
    sofab::OStream ostream{512};

    const std::array<uint8_t,  5> u8a  = {1, 2, 3, 4, 5};
    const std::array<int16_t,  5> i16a = {-1, -2, -3, -4, -5};
    const std::array<uint16_t, 5> u16a = {10, 20, 30, 40, 50};
    const std::array<int32_t,  5> i32a = {-100, -200, -300,
        std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()};
    const std::array<uint32_t, 5> u32a = {1, 2, 3, 0x80000000u,
        std::numeric_limits<uint32_t>::max()};
    const std::array<int64_t,  5> i64a = {-1, -2, -3,
        std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()};
    const std::array<uint64_t, 5> u64a = {1, 2, 3, 4,
        std::numeric_limits<uint64_t>::max()};
    const std::array<float,    5> f32a = {1.0f, 2.0f, 3.0f, -4.5f, 5.5f};
    const std::array<double,   5> f64a = {1.0, 2.0, 3.0, -4.5, 5.5};

    ostream
        .write(14, u8a)
        .write(15, i16a)
        .write(16, u16a)
        .write(17, i32a)
        .write(18, u32a)
        .write(19, i64a)
        .write(20, u64a)
        .write(22, f32a)
        .write(23, f64a);

    const auto used = ostream.bytesUsed();

    sofab::IStreamObject<FullObject> istream;
    auto result = istream.feed(ostream.data(), used);

    REQUIRE(result.code() == sofab::Error::None);

    const auto &d = (*istream).data_;
    REQUIRE(d.u8Array   == u8a);
    REQUIRE(d.i16Array  == i16a);
    REQUIRE(d.u16Array  == u16a);
    REQUIRE(d.i32Array  == i32a);
    REQUIRE(d.u32Array  == u32a);
    REQUIRE(d.i64Array  == i64a);
    REQUIRE(d.u64Array  == u64a);
    REQUIRE(d.fp32Array == f32a);
    REQUIRE(d.fp64Array == f64a);
}

TEST_CASE("IStream: round-trip repeated fields into dynamic vector")
{
    sofab::OStream ostream{64};

    ostream
        .write(0, 11u)
        .write(0, 22u)
        .write(0, 33u);

    const auto used = ostream.bytesUsed();

    sofab::IStreamObject<DynArrayOfUnsigned> istream;
    auto result = istream.feed(ostream.data(), used);

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(istream->values == std::vector<uint32_t>{11, 22, 33});
}

TEST_CASE("IStream: round-trip blob and variable-length-element arrays")
{
    sofab::OStream ostream{512};

    const std::vector<uint8_t> blob = {10, 20, 30, 40, 50};

    // Include empty, short (SSO) and a long (heap) string so the per-element
    // vector reallocation moves a mix of inline- and heap-stored elements -- the
    // exact path the transient-local decode pattern dangled on.
    const std::vector<std::string> strings = {
        "a", "", "hello",
        "this is a fairly long string well beyond small-string capacity"};

    const std::vector<std::vector<uint8_t>> blobs = {
        {1, 2, 3}, {}, {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

    // field 1: scalar blob
    ostream.write(1, blob.data(), static_cast<int32_t>(blob.size()));

    // field 2: sequence of string elements
    ostream.sequenceBegin(2);
    for (const auto &s : strings)
        ostream.write(7, std::string_view{s});
    ostream.sequenceEnd();

    // field 3: sequence of blob elements
    ostream.sequenceBegin(3);
    for (const auto &b : blobs)
        ostream.write(8, b.data(), static_cast<int32_t>(b.size()));
    ostream.sequenceEnd();

    const auto used = ostream.bytesUsed();

    SECTION("decode in one shot")
    {
        sofab::IStreamObject<BlobAndVarArrays> istream;
        auto result = istream.feed(ostream.data(), used);

        REQUIRE(result.code() == sofab::Error::None);
        REQUIRE(istream->blob == blob);
        REQUIRE(istream->strings == strings);
        REQUIRE(istream->blobs == blobs);
    }

    SECTION("decode one byte at a time")
    {
        // Forces the deferred decoder to suspend/resume mid-element, so the bound
        // targets must survive across many feed() calls.
        sofab::IStreamObject<BlobAndVarArrays> istream;

        for (size_t i = 0; i < used; i++)
        {
            auto result = istream.feed(ostream.data() + i, 1);
            REQUIRE(result.code() == sofab::Error::None);
        }

        REQUIRE(istream->blob == blob);
        REQUIRE(istream->strings == strings);
        REQUIRE(istream->blobs == blobs);
    }
}

TEST_CASE("IStream: fields without a matching read are skipped")
{
    sofab::OStream ostream{64};

    ostream
        .write(0, 42u)
        .write(1, 99u)
        .write(2, 7u);

    const auto used = ostream.bytesUsed();

    uint32_t got = 0;
    int calls = 0;

    sofab::IStreamInline istream{
        [&](sofab::id id, size_t, size_t) noexcept
        {
            calls++;

            // bind only field 1; fields 0 and 2 must be skipped cleanly
            if (id == 1)
            {
                istream.read(got);
            }
        }
    };

    auto result = istream.feed(ostream.data(), used);

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(calls == 3);
    REQUIRE(got == 99);
}

TEST_CASE("IStream: malformed input is rejected")
{
    SECTION("field id varint overflow")
    {
        const uint8_t buffer[] = {0xF8, 0xFF, 0xFF, 0xFF, 0x7F, 0x00};

        int calls = 0;
        sofab::IStreamInline istream{
            [&](sofab::id, size_t, size_t) noexcept { calls++; }
        };

        auto result = istream.feed(buffer, sizeof(buffer));

        REQUIRE(result.code() == sofab::Error::InvalidMessage);
        REQUIRE(result != sofab::Error::None);      // IStream::Result::operator!=
        REQUIRE_FALSE(result);                      // IStream::Result::operator bool
        REQUIRE(calls == 0);                        // id never completed, no callback
    }

    SECTION("unsigned varint value overflow")
    {
        const uint8_t buffer[] = {
            0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

        uint64_t value = 0;
        sofab::IStreamInline istream{
            [&](sofab::id id, size_t, size_t) noexcept
            {
                if (id == 0)
                {
                    istream.read(value);
                }
            }
        };

        auto result = istream.feed(buffer, sizeof(buffer));

        REQUIRE(result.code() == sofab::Error::InvalidMessage);
    }
}

//
// Nested-message round-trip: a parent with a scalar before and after a nested
// child sequence. Exercises the IStreamImpl::read<T> InputMessage branch both in
// one shot and fed one byte at a time (the nested decoder must suspend/resume at
// any byte boundary), plus a skip path where the parent ignores the sub-sequence.
//

TEST_CASE("IStream: round-trip nested message into object")
{
    sofab::OStream ostream{256};

    ostream
        .write(1, 7u)                   // parent header
        .sequenceBegin(2)               // nested child (fresh id scope)
            .write(1, 42u)              //   child id
            .write(2, 3.1415f)          //   child value
        .sequenceEnd()
        .write(3, 99u);                 // parent footer

    const auto used = ostream.bytesUsed();

    SECTION("decode in one shot")
    {
        sofab::IStreamObject<NestedParent> istream;
        auto result = istream.feed(ostream.data(), used);

        REQUIRE(result.code() == sofab::Error::None);

        const auto &d = (*istream).data_;
        REQUIRE(d.header == 7u);
        REQUIRE(d.child.data_.id == 42u);
        REQUIRE(d.child.data_.value == 3.1415f);
        REQUIRE(d.footer == 99u);
    }

    SECTION("decode one byte at a time")
    {
        sofab::IStreamObject<NestedParent> istream;

        for (size_t i = 0; i < used; i++)
        {
            auto result = istream.feed(ostream.data() + i, 1);
            REQUIRE(result.code() == sofab::Error::None);
        }

        const auto &d = (*istream).data_;
        REQUIRE(d.header == 7u);
        REQUIRE(d.child.data_.id == 42u);
        REQUIRE(d.child.data_.value == 3.1415f);
        REQUIRE(d.footer == 99u);
    }

    SECTION("an unread nested sequence is skipped and the parent resyncs")
    {
        uint32_t header = 0, footer = 0;
        int seqCalls = 0;

        sofab::IStreamInline istream{
            [&](sofab::id id, size_t, size_t) noexcept
            {
                if (id == 1)      istream.read(header);
                else if (id == 3) istream.read(footer);
                else if (id == 2) seqCalls++;   // sequence-start: left unread -> skipped
            }
        };

        auto result = istream.feed(ostream.data(), used);

        REQUIRE(result.code() == sofab::Error::None);
        REQUIRE(header == 7u);
        REQUIRE(footer == 99u);
        REQUIRE(seqCalls == 1);                 // only the top-level start fired
    }
}