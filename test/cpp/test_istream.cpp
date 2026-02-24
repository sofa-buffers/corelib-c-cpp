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