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

	void _onFieldCallback(sofab::IStream &_istream, sofab_id_t _id, size_t size) noexcept override
	{
		(void)size;

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

//

static_assert(sofab::API_VERSION == 1, "API version mismatch");

//

TEST_CASE("IStream: inline feed buffer")
{
	uint8_t value = 0x55;

	sofab::IStreamInline istream{
		[&](sofab::IStream& _istream, sofab_id_t id, size_t size) noexcept
		{
			(void)size;

			if (id == 0)
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
		[&](sofab::IStream& _istream, sofab_id_t id, size_t size) noexcept
		{
			if (id == 0)
			{
				value.resize(size);
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