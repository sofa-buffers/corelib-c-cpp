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
    uint32_t id = 0;
    float value = 0;

    static constexpr size_t _maxSize = 12;

    sofab::OStream::Result
    _serialize(sofab::OStream &_ostream) const noexcept override
    {
        return _ostream
            .writeIf(1, id, id != 0)
            .writeIf(2, value, value != 0.0f)
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

    auto result = ostream.sequenceBegin(SOFAB_ID_MAX);
    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by id via sequence end")
{
    sofab::OStream ostream{1};

    ostream.sequenceBegin(SOFAB_ID_MAX);
    auto result = ostream.sequenceEnd();
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

    auto result = ostream
        .write(0, 4711u)
        .sequenceBegin(1)
        .sequenceBegin(2);

    REQUIRE(result.code() == sofab::Error::BufferFull);
}

TEST_CASE("OStream: overflow by fluent sequence end")
{
    sofab::OStream ostream{4};

    auto result = ostream
        .write(0, 4711u)
        .sequenceBegin(1)
        .sequenceEnd()
        .sequenceEnd();

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
    ostream.sequenceBegin(1);
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
        .sequenceBegin(1)
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

TEST_CASE("OStream: write nested sequence with array fluent")
{
    sofab::OStream ostream{64};

    ostream.write(0, 42u)
        .sequenceBegin(3)
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
        ostream.sequenceBegin(1)
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
