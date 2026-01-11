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

class SimpleObject : public sofab::IStreamMessage
{
public:
    uint32_t id = 0;
    float value = 0;

    void _onFieldCallback(sofab::IStream &_istream, sofab::id _id, size_t _size) noexcept override
    {
        (void)_size;

        switch (_id)
        {
            case 1:
                _istream.read(id);
                break;
            case 2:
                _istream.read(value);
                break;
        }
    }
};

class DynArrayOfUnsigned : public sofab::IStreamMessage
{
public:
    std::vector<uint32_t> values;

    void _onFieldCallback(sofab::IStream &_istream, sofab::id _id, size_t _size) noexcept override
    {
        (void)_id;
        (void)_size;

        values.emplace_back(0);
        _istream.read(values.back());
    }
};

class DynArrayOfStrings : public sofab::IStreamMessage
{
public:
    std::vector<std::string> values;

    void _onFieldCallback(sofab::IStream &_istream, sofab::id _id, size_t _size) noexcept override
    {
        (void)_id;
        (void)_size;

        values.emplace_back(_size, '\0');
        _istream.read(values.back());
    }
};

class FullObject : public sofab::IStreamMessage
{
public:
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

    std::array<int8_t, 5> i8Array{};
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

    void _onFieldCallback(sofab::IStream &_istream, sofab::id _id, size_t _size) noexcept override
    {
        (void)_size;

        switch (_id)
        {
            case 1: _istream.read(i8); break;
            case 2: _istream.read(u8); break;
            case 3: _istream.read(i16); break;
            case 4: _istream.read(u16); break;
            case 5: _istream.read(i32); break;
            case 6: _istream.read(u32); break;
            case 7: _istream.read(i64); break;
            case 8: _istream.read(u64); break;
            case 9: _istream.read(boolean); break;
            case 10: _istream.read(fp32); break;
            case 11: _istream.read(fp64); break;
            case 12:
                str.resize(_size);
                _istream.read(str);
                break;

            case 13: _istream.read(i8Array); break;
            case 14: _istream.read(u8Array); break;
            case 15: _istream.read(i16Array); break;
            case 16: _istream.read(u16Array); break;
            case 17: _istream.read(i32Array); break;
            case 18: _istream.read(u32Array); break;
            case 19: _istream.read(i64Array); break;
            case 20: _istream.read(u64Array); break;
            // case 21: _istream.read(boolArray); break;
            case 22: _istream.read(fp32Array); break;
            case 23: _istream.read(fp64Array); break;
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
        [&](sofab::IStream& _istream, sofab::id _id, size_t _size) noexcept
        {
            (void)_size;

            if (_id == 0)
            {
                _istream.read(value);
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
        [&](sofab::IStream& _istream, sofab::id _id, size_t _size) noexcept
        {
            if (_id == 0)
            {
                value.resize(_size);
                _istream.read(value);
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