/*!
 * @file test_ostream.cpp
 * @brief SofaBuffers test for output stream C++ API
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <array>
#include <cstdlib>
#include <cstring>
#include <valarray>
#include <vector>
#include <span>

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

class SimpleObject : public sofab::OStreamMessage
{
public:
    struct Data
    {
        uint32_t id = 0;
        float value = 0;
    } data_;

    static constexpr size_t _maxSize = 12;

    Data* operator ->() noexcept
    {
        return &data_;
    }

    const Data* operator ->() const noexcept
    {
        return &data_;
    }

    sofab::OStreamImpl::Result
    serialize(sofab::OStreamImpl &_ostream) const noexcept override
    {
        return _ostream
            .writeIf(1, data_.id, data_.id != 0)
            .writeIf(2, data_.value, data_.value != 0.0f)
        ;
    }
};

//

static_assert(sofab::API_VERSION == 1, "API version mismatch");

//

TEST_CASE("OStream: init internal buffer")
{
    sofab::OStream ostream{16};

    REQUIRE(ostream.bytesUsed() == 0);
    REQUIRE(ostream.data() != nullptr);
    REQUIRE(ostream.flush() == 0);
}

TEST_CASE("OStream: init inline buffer")
{
    sofab::OStreamInline<16> ostream;

    REQUIRE(ostream.bytesUsed() == 0);
    REQUIRE(ostream.data() != nullptr);
    REQUIRE(ostream.flush() == 0);
}

TEST_CASE("OStream: init object buffer")
{
    sofab::OStreamObject<SimpleObject> ostream;

    REQUIRE(ostream.bytesUsed() == 0);
    REQUIRE(ostream.data() != nullptr);
    REQUIRE(ostream.flush() == 0);
}

TEST_CASE("OStream: init object buffer with offset")
{
    sofab::OStreamObjectOffset<SimpleObject, 4> ostream;

    REQUIRE(ostream.bytesUsed() == 4);
    REQUIRE(ostream.data() != nullptr);
    REQUIRE(ostream.flush() == 4);
}

TEST_CASE("OStream: init external buffer")
{
    const size_t buflen = 16;
    auto buffer = std::make_shared<uint8_t[]>(buflen);
    sofab::OStream ostream{buffer, buflen};

    REQUIRE(ostream.bytesUsed() == 0);
    REQUIRE(ostream.data() == buffer.get());
    REQUIRE(ostream.flush() == 0);
}

TEST_CASE("OStream: init external buffer with offset")
{
    const size_t buflen = 16;
    auto buffer = std::make_shared<uint8_t[]>(buflen);
    sofab::OStream ostream{buffer, buflen, 4};

    REQUIRE(ostream.bytesUsed() == 4);
    REQUIRE(ostream.data() == buffer.get());
    REQUIRE(ostream.flush() == 4);
}

//

TEST_CASE("OStream: serialize chunks from external buffer")
{
    const uint8_t expected[] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    std::vector<std::shared_ptr<uint8_t[]>> chunks;

    const size_t buflen = 8;
    auto buffer = std::make_shared<uint8_t[]>(buflen);

    sofab::OStream ostream{
        [&](std::span<const uint8_t> data)
        {
            // consume current buffer
            (void)data;
            chunks.push_back(ostream.getBuffer());

            // allocate new buffer
            buffer = std::make_shared<uint8_t[]>(buflen);
            ostream.setBuffer(buffer, buflen);
        },
        buffer, buflen
    };

    auto result = ostream.write(0, SOFAB_SIGNED_MIN);
    REQUIRE(result.code() == sofab::Error::None);

    ostream.flush();

    const size_t total = sizeof(expected);
    const size_t expectedChunks = (total + buflen - 1) / buflen;
    REQUIRE(chunks.size() == expectedChunks);

    size_t offset = 0;
    for (size_t i = 0; i < chunks.size(); ++i)
    {
        size_t len = buflen;
        if (len > total - offset) len = total - offset;

        REQUIRE(std::memcmp(chunks[i].get(), expected + offset, len) == 0);
        offset += len;
    }
    REQUIRE(offset == total);
    REQUIRE(ostream.bytesUsed() == 0);
    REQUIRE(ostream.data() == buffer.get());
    REQUIRE(ostream.flush() == 0);
}

TEST_CASE("OStream: serialize chunks from inline buffer")
{
    const uint8_t expected[] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

    sofab::OStreamInline<16> ostream{
        [&expected](std::span<const uint8_t> data)
        {
            REQUIRE(data.size() == sizeof(expected));
            REQUIRE(std::memcmp(data.data(), expected, data.size()) == 0);
        }
    };

    auto result = ostream.write(0, SOFAB_SIGNED_MIN);
    REQUIRE(result.code() == sofab::Error::None);

    auto used = ostream.flush();
    REQUIRE(used == sizeof(expected));

    REQUIRE(ostream.bytesUsed() == 0);
    REQUIRE(ostream.data() != nullptr);
    REQUIRE(ostream.flush() == 0);
}

TEST_CASE("OStream: write object")
{
    sofab::OStreamObject<SimpleObject> ostream;
    ostream->id = 42;
    ostream->value = 3.1415;
    ostream.serialize();

    REQUIRE(ostream.bytesUsed() == 8);
    REQUIRE(ostream.data() != nullptr);
    REQUIRE(ostream.flush() == 8);
}

TEST_CASE("OStream: write object with flush callback")
{
    size_t serializedBytes = 0;

    sofab::OStreamObject<SimpleObject, 4> ostream{
        [&](std::span<const uint8_t> data)
        {
            serializedBytes += data.size();
        }
    };

    ostream->id = 42;
    ostream->value = 3.1415;
    ostream.serialize();

    REQUIRE(serializedBytes == 8);
    REQUIRE(ostream.bytesUsed() == 0);
    REQUIRE(ostream.data() != nullptr);
    REQUIRE(ostream.flush() == 0);
}

//

TEST_CASE("OStream: overflow by id via unsigned")
{
    sofab::OStream ostream{2};

    auto result = ostream.write(SOFAB_ID_MAX, 1);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via signed")
{
    sofab::OStream ostream{2};

    auto result = ostream.write(SOFAB_ID_MAX, -1);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via fixlen")
{
    sofab::OStream ostream{2};
    auto result = ostream.write(SOFAB_ID_MAX, 3.14f);

    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via array of unsigned")
{
    sofab::OStream ostream{2};
    const std::array<uint8_t, 3> array = {1, 2, 3};

    auto result = ostream.write(SOFAB_ID_MAX, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via array of signed")
{
    sofab::OStream ostream{2};
    const std::array<int8_t, 3> array = {-1, -2, -3};

    auto result = ostream.write(SOFAB_ID_MAX, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via array of fixlen")
{
    sofab::OStream ostream{2};
    const std::array<float, 3> array = {1, 2, 3};

    auto result = ostream.write(SOFAB_ID_MAX, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via sequence begin")
{
    sofab::OStream ostream{2};

    // The header is held back, so the overflow surfaces where it is emitted:
    // sequenceEndKeep() commits the run before writing the end marker.
    ostream.sequenceBeginLazy(SOFAB_ID_MAX);
    auto result = ostream.sequenceEndKeep();
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via sequence end")
{
    // Five bytes take the SOFAB_ID_MAX header exactly; the end marker is the byte
    // that no longer fits.
    sofab::OStream ostream{5};

    ostream.sequenceBeginLazy(SOFAB_ID_MAX);
    auto result = ostream.sequenceEndKeep();
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

//

TEST_CASE("OStream: overflow by unsigned value")
{
    sofab::OStream ostream{2};

    auto result = ostream.write(0, SOFAB_UNSIGNED_MAX);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by signed value")
{
    sofab::OStream ostream{2};

    auto result = ostream.write(0, SOFAB_SIGNED_MAX);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by fixlen length")
{
    sofab::OStream ostream{1};

    auto result = ostream.write(0, 3.14f);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by fixlen value")
{
    sofab::OStream ostream{2};

    auto result = ostream.write(0, 3.14f);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by array count via array of unsigned")
{
    sofab::OStream ostream{1};
    const std::array<uint8_t, 3> array = {1, 2, 3};

    auto result = ostream.write(0, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by array count via array of signed")
{
    sofab::OStream ostream{1};
    const std::array<int8_t, 3> array = {-1, -2, -3};

    auto result = ostream.write(0, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by array count via array of fixlen")
{
    sofab::OStream ostream{1};
    const std::array<float, 3> array = {1, 2, 3};

    auto result = ostream.write(0, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by array fixlen length")
{
    sofab::OStream ostream{2};
    const std::array<float, 3> array = {1, 2, 3};

    auto result = ostream.write(0, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by array fixlen value")
{
    sofab::OStream ostream{4};
    const std::array<float, 3> array = {1, 2, 3};

    auto result = ostream.write(0, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by array value via array of unsigned")
{
    sofab::OStream ostream{4};
    const std::array<uint8_t, 3> array = {1, 2, 3};

    auto result = ostream.write(0, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by array value via array of signed")
{
    sofab::OStream ostream{4};
    const std::array<int8_t, 3> array = {-1, -2, -3};

    auto result = ostream.write(0, array);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by fluent write")
{
    sofab::OStream ostream{4};

    auto result = ostream
        .write(0, 4711u)
        .write(1, -1234)
        .write(2, 3.14f);

    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by fluent writeIf")
{
    sofab::OStream ostream{4};

    auto result = ostream
        .writeIf(0, 4711u, true)
        .writeIf(1, -1234, true)
        .writeIf(2, 3.14f, true);

    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by fluent sequence begin")
{
    sofab::OStream ostream{3};

    // write() fills the buffer; both begins are held back, so the failure lands on
    // the call that emits them.
    auto result = ostream
        .write(0, 4711u)
        .sequenceBeginLazy(1)
        .sequenceBeginLazy(2)
        .sequenceEndKeep();

    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by fluent sequence end")
{
    sofab::OStream ostream{4};

    // Three bytes of payload leave one free: the committed header takes it, and the
    // end marker overflows.
    auto result = ostream
        .write(0, 4711u)
        .sequenceBeginLazy(1)
        .sequenceEndKeep();

    REQUIRE(result.code() == sofab::Error::BufferFull);
}

/*
 * testing invalid arg via incorrect array element size is not needed in C++,
 * because element size is deduced from type info
 */

TEST_CASE("OStream: id min")
{
    sofab::OStream ostream{2};

    auto result = ostream.write(0, 0u);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x00, 0x00};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: id max")
{
    sofab::OStream ostream{16};

    auto result = ostream.write(SOFAB_ID_MAX, 0u);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0xF8, 0xFF, 0xFF, 0xFF, 0x3F, 0x00};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: id overflow")
{
    sofab::OStream ostream{16};

    auto result = ostream.write((uint32_t)(SOFAB_ID_MAX) + 1, 0u);
    REQUIRE(result.code() == sofab::Error::InvalidArgument);
}

/*
 * varint serialization tests are covered in C tests
 */

TEST_CASE("OStream: write signed min")
{
    sofab::OStream ostream{16};

    auto result = ostream.write(0, SOFAB_SIGNED_MIN);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write signed max")
{
    sofab::OStream ostream{16};

    auto result = ostream.write(0, SOFAB_SIGNED_MAX);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x01, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
       REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write boolean")
{
    sofab::OStream ostream{16};

    auto result = ostream.write(0, true);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x00, 0x01};
       REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write fp32")
{
    sofab::OStream ostream{16};

    auto result = ostream.write<float>(0, 3.1415f);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x20, 0x56, 0x0E, 0x49, 0x40};
       REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write fp64")
{
    sofab::OStream ostream{16};

    // using float to double conversion to ensure payload test
    auto result = ostream.write<double>(0, 3.14159265f);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x41, 0x00, 0x00, 0x00, 0x60, 0xFB, 0x21, 0x09, 0x40};
       REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

//

TEST_CASE("OStream: write string")
{
    sofab::OStream ostream{16};

    std::string str = "Hello Couch!";

    auto result = ostream.write(0, str);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write const string")
{
    sofab::OStream ostream{16};

    const std::string str = "Hello Couch!";

    auto result = ostream.write(0, str);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write string_view")
{
    sofab::OStream ostream{16};

    const std::string str = "Hello Couch!";
    std::string_view sv{str};

    auto result = ostream.write(0, sv);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write const string_view")
{
    sofab::OStream ostream{16};

    const std::string str = "Hello Couch!";
    const std::string_view sv{str};

    auto result = ostream.write(0, sv);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write char pointer")
{
    sofab::OStream ostream{16};

    char buf[] = "Hello Couch!";
    char *str = buf;

    auto result = ostream.write(0, str);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write const char pointer")
{
    sofab::OStream ostream{16};

    const char *str = "Hello Couch!";

    auto result = ostream.write(0, str);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write char array")
{
    sofab::OStream ostream{16};

    char str[] = "Hello Couch!";

    auto result = ostream.write(0, str);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write const char array")
{
    sofab::OStream ostream{16};

    const char str[] = "Hello Couch!";

    auto result = ostream.write(0, str);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

//

TEST_CASE("OStream: write blob")
{
    sofab::OStream ostream{16};

    uint8_t blob[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    auto result = ostream.write(0, blob, sizeof(blob));
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x2B, 0x01, 0x02, 0x03, 0x04, 0x05};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write const blob")
{
    sofab::OStream ostream{16};

    const uint8_t blob[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    auto result = ostream.write(0, blob, sizeof(blob));
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x02, 0x2B, 0x01, 0x02, 0x03, 0x04, 0x05};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

//

TEST_CASE("OStream: write array of unsigned")
{
    sofab::OStream ostream{16};
    const std::array<uint32_t, 5> array = {1, 2, 3, 0x80000000, UINT32_MAX};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x80, 0x80, 0x80, 0x80, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write array of signed")
{
    sofab::OStream ostream{16};
    const std::array<int32_t, 5> array = {-1, -2, -3, INT32_MIN, INT32_MAX};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write array of float")
{
    sofab::OStream ostream{32};
    const std::array<float, 5> array = {1.0f, 2.0f, 3.0f,
        -std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00,
        0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write array of double")
{
    sofab::OStream ostream{64};
    const std::array<double, 5> array = {1.0, 2.0, 3.0,
        -std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write valarray of unsigned")
{
    sofab::OStream ostream{16};
    const std::valarray<uint32_t> array = {1, 2, 3, 0x80000000, UINT32_MAX};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x80, 0x80, 0x80, 0x80, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write valarray of signed")
{
    sofab::OStream ostream{16};
    const std::valarray<int32_t> array = {-1, -2, -3, INT32_MIN, INT32_MAX};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write valarray of float")
{
    sofab::OStream ostream{32};
    const std::valarray<float> array = {1.0f, 2.0f, 3.0f,
        -std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00,
        0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write valarray of double")
{
    sofab::OStream ostream{64};
    const std::valarray<double> array = {1.0, 2.0, 3.0,
        -std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write vector of unsigned")
{
    sofab::OStream ostream{16};
    const std::vector<uint32_t> array = {1, 2, 3, 0x80000000, UINT32_MAX};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x80, 0x80, 0x80, 0x80, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write vector of signed")
{
    sofab::OStream ostream{16};
    const std::vector<int32_t> array = {-1, -2, -3, INT32_MIN, INT32_MAX};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write vector of float")
{
    sofab::OStream ostream{32};
    const std::vector<float> array = {1.0f, 2.0f, 3.0f,
        -std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00,
        0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write vector of double")
{
    sofab::OStream ostream{64};
    const std::vector<double> array = {1.0, 2.0, 3.0,
        -std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};

    auto result = ostream.write(0, array);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write span of unsigned")
{
    sofab::OStream ostream{16};
    const std::vector<uint32_t> array = {1, 2, 3, 0x80000000, UINT32_MAX};
    const std::span<const uint32_t> span{array};

    auto result = ostream.write(0, span);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x80, 0x80, 0x80, 0x80, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write span of signed")
{
    sofab::OStream ostream{16};
    const std::vector<int32_t> array = {-1, -2, -3, INT32_MIN, INT32_MAX};
    const std::span<const int32_t> span{array};

    auto result = ostream.write(0, span);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write span of float")
{
    sofab::OStream ostream{32};
    const std::vector<float> array = {1.0f, 2.0f, 3.0f,
        -std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    const std::span<const float> span{array};

    auto result = ostream.write(0, span);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00,
        0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write span of double")
{
    sofab::OStream ostream{64};
    const std::vector<double> array = {1.0, 2.0, 3.0,
        -std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    const std::span<const double> span{array};

    auto result = ostream.write(0, span);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x05, 0x05, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0x7F};

    REQUIRE(result.code() == sofab::Error::None);
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write nested sequence")
{
    sofab::OStream ostream{64};

    ostream.write(0, 42u);
    ostream.sequenceBeginLazy(1);
    {
        ostream.write(0, 42u);
        ostream.write(2, -42);
    }
    ostream.sequenceEnd();
    ostream.write(2, -42);
    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x07, 0x11, 0x53};
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write nested sequence fluent")
{
    sofab::OStream ostream{64};

    ostream.write(0, 42u)
        .sequenceBeginLazy(1)
            .write(0, 42u)
            .write(2, -42)
        .sequenceEnd()
        .write(2, -42)
        .writeIf(3, 4711, false); // should be skipped

    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x07, 0x11, 0x53};
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

// The wrapper inherits the C core's documented hold-back bound
// (SOFAB_LAZY_SEQ_DEPTH, CORELIB_PLAN §6): canonical up to it, eagerly framed --
// well-formed but non-canonical -- beyond it. Pinned here too, because the C++
// surface is where generated code meets it. See test_ostream.c for the byte-level
// window tests and why a heap-free profile takes that allowance.
TEST_CASE("OStream: contentless nesting up to the hold-back bound emits nothing")
{
    for (unsigned depth = 1; depth <= SOFAB_LAZY_SEQ_DEPTH; depth++)
    {
        sofab::OStream ostream{256};

        for (unsigned i = 0; i < depth; i++) ostream.sequenceBeginLazy(1);
        for (unsigned i = 0; i < depth; i++) ostream.sequenceEnd();

        INFO("depth " << depth);
        REQUIRE(ostream.bytesUsed() == 0);
    }
}

TEST_CASE("OStream: contentless nesting past the hold-back bound frames eagerly")
{
    sofab::OStream ostream{256};
    constexpr unsigned depth = 40;

    for (unsigned i = 0; i < depth; i++) ostream.sequenceBeginLazy(1);
    for (unsigned i = 0; i < depth; i++) ostream.sequenceEnd();

    auto used = ostream.bytesUsed();
    REQUIRE(used > 0);   // a bounded window cannot stay canonical at depth 40

    // Only begin(1) headers and end markers, in equal numbers: the empty frames
    // §2 would have omitted, which a decoder normalizes away.
    const uint8_t *bytes = ostream.data();
    size_t begins = 0, ends = 0;
    for (size_t i = 0; i < used; i++)
    {
        if (bytes[i] == 0x0E) begins++;
        else if (bytes[i] == 0x07) ends++;
        else FAIL("unexpected byte in a contentless deep nesting");
    }
    REQUIRE(begins == ends);
}

TEST_CASE("OStream: write nested sequence with array fluent")
{
    sofab::OStream ostream{64};

    ostream.write(0, 42u)
        .sequenceBeginLazy(3)
            .write(0, 42u)
            .write(3, std::array<int32_t, 3>{-42, -43, -44})
        .sequenceEnd()
        .write(2, -42);

    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {0x00, 0x2A, 0x1E, 0x00, 0x2A, 0x1C, 0x03, 0x53, 0x55, 0x57, 0x07, 0x11, 0x53};
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: write nested sequence multilevel")
{
    sofab::OStream ostream{128};

    ostream.write(0, 42u);

    for (int i = 0; i < 10; i++)
    {
        ostream.sequenceBeginLazy(1)
            .write(0, 42u)
            .write(2, -42);
    }

    for (int i = 0; i < 10; i++)
    {
        ostream.sequenceEnd();
    }

    ostream.write(2, -42);

    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x11, 0x53};
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

//
// ---------------------------------------------------------------------------
// MESSAGE_SPEC §2 vs §5.1 in the C++ wrapper: a FIELD is omitted, an ELEMENT
// keeps its frame.
//
// This is the one place in the §2 port where getting it wrong corrupts a value
// instead of costing bytes: a wrapper array's length is "highest present id + 1"
// (§5.1), so an element that loses its frame shortens the decoded array.
//
// The two rules are two different calls on OStreamImpl, differing only in the
// closer they hand to the hold-back trio in ostream.c:
//   - writeLazy(id, msg) -> sequenceEnd()     : the nested-message FIELD form.
//     The nested serialize() writes only children that differ from their default,
//     so "not one child was written" is exactly "the value equals its declared
//     default", and the held-back header is dropped (§2).
//   - write(id, msg)     -> sequenceEndKeep() : the wrapper-array ELEMENT form.
//     The held-back header is forced out even when the element wrote nothing,
//     because element presence carries the array's length (§5.1).
//
// The pure-C path reaches the same bytes by a different mechanism and must stay
// separate: sofab_object_encode() decides omission from the descriptor *before*
// it opens anything (no hold-back window at all, canonical at every depth), and
// its role check -- info->fixed_seq in object.c, not the field type -- is what
// keeps an interior element framed. The expectations below are deliberately
// byte-identical to their C descriptor-path counterparts in test/c/test_object.c
// (test_object_struct_wrapper_all_default_empty / _interior_default_kept), which
// pins that the two message layers agree on the wire while deciding it
// independently. Do not "simplify" them into one path.
// ---------------------------------------------------------------------------
//

// A nested message that writes exactly the children differing from their
// declared default -- the shape generated code has.  All-default => writes
// nothing.
class KeyValue final : public sofab::OStreamMessage
{
public:
    uint32_t k = 0;
    uint32_t v = 0;

    KeyValue() noexcept = default;
    KeyValue(uint32_t key, uint32_t value) noexcept : k{key}, v{value} { }

    sofab::OStreamImpl::Result
    serialize(sofab::OStreamImpl &_ostream) const noexcept override
    {
        return _ostream
            .writeIf(0, k, k != 0)
            .writeIf(1, v, v != 0)
        ;
    }
};

// The wrapper sequence of an array of KeyValue (§5): its children are the
// elements, id == array index. Every element goes through the ELEMENT form
// (plain write), whatever its value.
class KeyValueArray final : public sofab::OStreamMessage
{
public:
    std::vector<KeyValue> elements;

    sofab::OStreamImpl::Result
    serialize(sofab::OStreamImpl &_ostream) const noexcept override
    {
        // Result is only constructible by the stream, so seed the chain with a
        // success no-op; Result::write updates it in place and is sticky.
        auto result = _ostream.writeIf(0, 0u, false);
        for (size_t i = 0; i < elements.size(); i++)
        {
            result.write(static_cast<sofab_id_t>(i), elements[i]);
        }

        return result;
    }
};

TEST_CASE("OStream: an all-default nested message FIELD is omitted")
{
    sofab::OStream ostream{64};
    const KeyValue kv;  // every child at its declared default

    ostream.write(0, 42u)
        .writeLazy(10, kv)
        .write(3, 7u);

    auto used = ostream.bytesUsed();

    // No 0x56 (sequence start, id 10) / 0x07 pair: the field is gone, and the
    // fields around it are untouched.
    const uint8_t expected[] = {0x00, 0x2A, 0x18, 0x07};
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: a nested message FIELD with content is framed")
{
    sofab::OStream ostream{64};
    const KeyValue kv{1, 2};

    ostream.write(0, 42u)
        .writeLazy(10, kv)
        .write(3, 7u);

    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x00, 0x2A,                         // id 0 = 42
        0x56, 0x00, 0x01, 0x08, 0x02, 0x07, // id 10 = {k=1, v=2}
        0x18, 0x07,                         // id 3 = 7
    };
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: an all-default wrapper ELEMENT keeps its frame")
{
    sofab::OStream ostream{64};
    KeyValueArray arr;
    arr.elements.resize(2);  // two elements, both all-default

    ostream.writeLazy(200, arr);

    auto used = ostream.bytesUsed();

    // Both elements are on the wire as empty frames. If the element closer
    // dropped them the wrapper would write nothing, the field would then be
    // omitted too, and a two-element array would decode as absent.
    const uint8_t expected[] = {
        0xC6, 0x0C,   // wrapper open (id 200)
        0x06, 0x07,   // element 0: empty frame
        0x0E, 0x07,   // element 1: empty frame
        0x07,         // wrapper close
    };
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: a trailing all-default ELEMENT keeps the array's length")
{
    sofab::OStream ostream{64};
    KeyValueArray arr;
    arr.elements.push_back(KeyValue{1, 2});
    arr.elements.push_back(KeyValue{});   // all-default, and it is the last one

    ostream.writeLazy(200, arr);

    auto used = ostream.bytesUsed();

    // Length is "highest present id + 1" (§5.1): dropping element 1 would decode
    // as a one-element array. Only a *fixed-count* array elides its trailing
    // default run, and that trim is the message layer's decision, not the
    // stream's -- the ELEMENT form always frames what it is handed.
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x06, 0x00, 0x01, 0x08, 0x02, 0x07, // element 0 = {1, 2}
        0x0E, 0x07,                         // element 1 = empty frame
        0x07,
    };
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: an interior all-default ELEMENT stays framed")
{
    sofab::OStream ostream{64};
    KeyValueArray arr;
    arr.elements.push_back(KeyValue{1, 2});
    arr.elements.push_back(KeyValue{});
    arr.elements.push_back(KeyValue{3, 4});

    ostream.writeLazy(200, arr);

    auto used = ostream.bytesUsed();

    // Byte-identical to the C descriptor path's
    // test_object_struct_wrapper_interior_default_kept (test/c/test_object.c):
    // two message layers, two mechanisms, one wire form.
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x06, 0x00, 0x01, 0x08, 0x02, 0x07, // element 0 = {1, 2}
        0x0E, 0x07,                         // element 1 = empty frame (interior)
        0x16, 0x00, 0x03, 0x08, 0x04, 0x07, // element 2 = {3, 4}
        0x07,
    };
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: an array FIELD with no element is omitted")
{
    sofab::OStream ostream{64};
    const KeyValueArray arr;   // no elements at all

    ostream.writeLazy(200, arr);

    // The wrapper wrote nothing, so the field equals its declared (empty)
    // default and vanishes -- not an empty wrapper (0xC6 0x0C 0x07). Same
    // outcome as the C path's test_object_struct_wrapper_all_default_empty.
    REQUIRE(ostream.bytesUsed() == 0);
}

TEST_CASE("OStream: an all-default nested FIELD inside a framed ELEMENT")
{
    sofab::OStream ostream{64};

    // element 0 of a wrapper array; the element itself holds one nested message
    // FIELD (id 4) that is all-default. The element keeps its frame (§5.1), the
    // field inside it disappears (§2) -- both rules, one encode.
    ostream.sequenceBeginLazy(200);
    {
        const KeyValue inner;
        ostream.sequenceBeginLazy(0);
        ostream.writeLazy(4, inner);
        ostream.sequenceEndKeep();
    }
    ostream.sequenceEnd();

    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0xC6, 0x0C,   // wrapper open (id 200)
        0x06, 0x07,   // element 0: framed, and empty -- its only field went away
        0x07,         // wrapper close
    };
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

//
// ---------------------------------------------------------------------------
// A nested serialize() that fails must not leave its sequence open.
//
// Reachable on a perfectly healthy stream: a write can be refused before it
// emits a single byte -- sofab_ostream_write_fixlen returns E_ARGUMENT for
// invalid UTF-8 under SOFAB_ENABLE_STRICT_UTF8, and any writer refuses an id
// above SOFAB_ID_MAX. If write()/writeLazy() then skipped their closer, the
// sequence they opened would stay open: the following field would be encoded
// *inside* it and the message would decode as INCOMPLETE, with the leaked entry
// also occupying a slot of the bounded hold-back window for the rest of the
// encode. The failure is still reported -- the closer never masks it.
// ---------------------------------------------------------------------------
//

// Writes one valid child, then fails on an out-of-range id (E_ARGUMENT, emitted
// before any byte of that field). No opt-in build flag needed.
class FailingMessage : public sofab::OStreamMessage
{
public:
    sofab::OStreamImpl::Result
    serialize(sofab::OStreamImpl &_ostream) const noexcept override
    {
        return _ostream
            .write(0, 5u)
            .write(static_cast<sofab::id>(SOFAB_ID_MAX) + 1u, 1u)
        ;
    }
};

TEST_CASE("OStream: a failing nested FIELD serialize still closes its sequence")
{
    sofab::OStream ostream{64};
    const FailingMessage bad;

    auto res = ostream.writeLazy(10, bad);
    REQUIRE(res == sofab::Error::InvalidArgument);   // the failure is reported

    ostream.write(3, 7u);

    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x56, 0x00, 0x05, 0x07, // id 10 = { id 0 = 5 }, closed despite the failure
        0x18, 0x07,             // id 3 = 7 -- a sibling, NOT nested in id 10
    };
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

TEST_CASE("OStream: a failing nested ELEMENT serialize still closes its sequence")
{
    sofab::OStream ostream{64};
    const FailingMessage bad;

    auto res = ostream.write(1, bad);                // the ELEMENT form
    REQUIRE(res == sofab::Error::InvalidArgument);

    ostream.write(2, 9u);

    auto used = ostream.bytesUsed();

    const uint8_t expected[] = {
        0x0E, 0x00, 0x05, 0x07, // element 1 = { id 0 = 5 }, frame closed
        0x10, 0x09,             // id 2 = 9 -- a sibling
    };
    REQUIRE(used == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, used) == 0);
}

/* A string value carries its own length; the encoder must use it.
 *
 * Routing the string-like branch of `write()` through the C convenience
 * wrapper `sofab_ostream_write_string()` derived the payload length with
 * strlen() instead, which is wrong in both directions: a std::string_view need
 * not be NUL-terminated, so strlen() read past the caller's buffer; and a value
 * holding an embedded U+0000 was truncated there, although CORELIB_PLAN §4.6
 * frames a string by length with no terminator and §6.4.3 makes embedded
 * U+0000 valid UTF-8 that MUST NOT be rejected.
 */
TEST_CASE("OStream: a string_view's length is the payload length, not strlen")
{
    /* Deliberately unterminated: the six bytes are the whole allocation, so a
     * strlen() over them runs off the end. Heap-allocated rather than a local
     * array so a sanitizer build has a redzone to catch it. */
    char *raw = static_cast<char *>(std::malloc(6));
    REQUIRE(raw != nullptr);
    std::memcpy(raw, "abcdef", 6);

    sofab::OStreamInline<64> ostream;
    auto res = ostream.write(1, std::string_view(raw, 6));
    REQUIRE(res == sofab::Error::None);
    ostream.flush();

    const uint8_t expected[] = {
        0x0A,                               // id 1, FIXLEN
        0x32,                               // fixlen_word: (6 << 3) | STRING
        'a', 'b', 'c', 'd', 'e', 'f',
    };
    REQUIRE(ostream.bytesUsed() == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, sizeof(expected)) == 0);

    std::free(raw);
}

TEST_CASE("OStream: an embedded U+0000 survives; the string is not cut at it")
{
    const std::string value("a\0b", 3);
    REQUIRE(value.size() == 3);

    sofab::OStreamInline<64> ostream;
    auto res = ostream.write(1, value);
    REQUIRE(res == sofab::Error::None);
    ostream.flush();

    const uint8_t expected[] = {
        0x0A,                               // id 1, FIXLEN
        0x1A,                               // fixlen_word: (3 << 3) | STRING
        'a', 0x00, 'b',
    };
    REQUIRE(ostream.bytesUsed() == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, sizeof(expected)) == 0);
}

TEST_CASE("OStream: a NUL-terminated const char* is unchanged by that")
{
    sofab::OStreamInline<64> ostream;
    auto res = ostream.write(1, "abc");
    REQUIRE(res == sofab::Error::None);
    ostream.flush();

    const uint8_t expected[] = { 0x0A, 0x1A, 'a', 'b', 'c' };
    REQUIRE(ostream.bytesUsed() == sizeof(expected));
    REQUIRE(std::memcmp(ostream.data(), expected, sizeof(expected)) == 0);
}
