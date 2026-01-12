/*!
 * @file test_ostream.h
 * @brief SofaBuffers test for output stream C API
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/ostream.h"

#include "unity.h"

#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

void hexdump(const void *data, size_t size)
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

void hexdump2array(const void *data, size_t len)
{
    const unsigned char *bytes = (const unsigned char *)data;
    printf("[%zu] = { ", len);
    for (size_t i = 0; i < len; ++i)
    {
        printf("0x%02X", bytes[i]);
        if (i < len - 1)
        {
            printf(", ");
        }

        if ((i + 1) % 12 == 0 && i < len - 1)
        {
            printf("\n  ");
        }
    }
    printf("\n};\n");
}


/*****************************************************************************/
/* tests */
/*****************************************************************************/

static void _flush_callback(sofab_ostream_t *ctx, const uint8_t *data, size_t len, void *usrptr)
{
    (void)ctx;
    (void)data;
    (void)len;
    (void)usrptr;
}

static void test_init (void)
{
    int usrdata = 0;

    sofab_ostream_t ctx;
    uint8_t buffer[16];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 8, _flush_callback, &usrdata);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer, ctx.buffer, "ctx.buffer != buffer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer + sizeof(buffer), ctx.bufend, "ctx.bufend != buffer + sizeof(buffer)");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer + 8, ctx.offset, "ctx.offset != buffer + 8");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(_flush_callback, ctx.flush, "ctx.flush != _flush_callback");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&usrdata, ctx.usrptr, "ctx.usrptr != &usrdata");
}

static void test_buffer_set (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[1];
    uint8_t buffer2[16];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ostream_buffer_set(&ctx, buffer2, sizeof(buffer2), 8);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer2, ctx.buffer, "ctx.buffer != buffer2");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer2 + sizeof(buffer2), ctx.bufend, "ctx.bufend != buffer2 + sizeof(buffer2)");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer2 + 8, ctx.offset, "ctx.offset != buffer2 + 8");
    TEST_ASSERT_NULL_MESSAGE(ctx.flush, "ctx.flush != NULL");
    TEST_ASSERT_NULL_MESSAGE(ctx.usrptr, "ctx.usrptr != NULL");
}

static void test_buffer_flush (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[1];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, _flush_callback, NULL);
    ret = sofab_ostream_write_unsigned(&ctx, 47, 11);
    size_t used = sofab_ostream_flush(&ctx);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1, used, "used != 1");
}

static void test_buffer_overflow_by_id_via_unsigned (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_unsigned(&ctx, SOFAB_ID_MAX, 0);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_id_via_signed (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_signed(&ctx, SOFAB_ID_MAX, 0);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_id_via_fixlen (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_fp32(&ctx, SOFAB_ID_MAX, 0);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_id_via_array_of_unsigned (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    uint8_t array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_unsigned(&ctx, SOFAB_ID_MAX, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_id_via_array_of_signed (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    int8_t array[] = {-1, -2, -3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_signed(&ctx, SOFAB_ID_MAX, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_id_via_array_of_fixlen (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    float array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_fp32(&ctx, SOFAB_ID_MAX, array, sizeof(array) / sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_id_via_sequence_begin (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_sequence_begin(&ctx, SOFAB_ID_MAX);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_id_via_sequence_end (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[1];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ostream_write_sequence_begin(&ctx, 0);
    ret =  sofab_ostream_write_sequence_end(&ctx);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_unsigned_value (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_unsigned(&ctx, 0, SOFAB_UNSIGNED_MAX);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_signed_value (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_signed(&ctx, 0, SOFAB_SIGNED_MAX);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_fixlen_length (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[1];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_fp32(&ctx, 0, 3.14f);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_fixlen_value (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_fp32(&ctx, 0, 3.14f);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_array_count_via_array_of_unsigned (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[1];

    uint8_t array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_unsigned(&ctx, 0, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_array_count_via_array_of_signed (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[1];

    int8_t array[] = {-1, -2, -3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_signed(&ctx, 0, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_array_count_via_array_of_fixlen (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[1];

    float array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_fp32(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_array_fixlen_length (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];

    float array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_fp32(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_array_fixlen_value (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[4];

    float array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_fp32(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_array_value_via_array_of_unsigned (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[4];

    uint8_t array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_unsigned(&ctx, 0, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_buffer_overflow_by_array_value_via_array_of_signed (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[4];

    int8_t array[] = {-1, -2, -3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_signed(&ctx, 0, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_BUFFER_FULL, "ret != SOFAB_RET_E_BUFFER_FULL");
}

static void test_invalid_arg_via_array_of_unsigned_element_size (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];

    uint8_t array[] = {1, 2, 3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_unsigned(&ctx, 0, array, sizeof(array) / sizeof(array[0]), 3);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_ARGUMENT, "ret != SOFAB_RET_E_ARGUMENT");
}

static void test_invalid_arg_via_array_of_signed_element_size (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];

    int8_t array[] = {-1, -2, -3};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret =  sofab_ostream_write_array_of_signed(&ctx, 0, array, sizeof(array) / sizeof(array[0]), 3);
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_ARGUMENT, "ret != SOFAB_RET_E_ARGUMENT");
}

static void test_id_min (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[2];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_unsigned(&ctx, 0, 0);
    size_t used = sofab_ostream_flush(&ctx);

    const uint8_t expected[] = {0x00, 0x00};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_id_max (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_unsigned(&ctx, SOFAB_ID_MAX, 0);
    size_t used = sofab_ostream_flush(&ctx);

    const uint8_t expected[] = {0xF8, 0xFF, 0xFF, 0xFF, 0x3F, 0x00};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_id_overflow (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_unsigned(&ctx, (uint32_t)(SOFAB_ID_MAX) + 1, 0);

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_ARGUMENT, "ret != SOFAB_RET_E_ARGUMENT");
}

static void _write_unsigned (uint64_t value, const uint8_t *expected, size_t expected_len)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_unsigned(&ctx, 0, value);
    size_t used = sofab_ostream_flush(&ctx);

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(expected_len, used, "used != expected_len");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_unsigned_h0 (void)
{
    _write_unsigned(0x0, (const uint8_t[]){0x00, 0x00}, 2);
}

static void test_write_unsigned_h7F (void)
{
    _write_unsigned(0x7F, (const uint8_t[]){0x00, 0x7F}, 2);
}

static void test_write_unsigned_h80 (void)
{
    _write_unsigned(0x80, (const uint8_t[]){0x00, 0x80, 0x01}, 3);
}

static void test_write_unsigned_h3FFF (void)
{
    _write_unsigned(0x3FFF, (const uint8_t[]){0x00, 0xFF, 0x7F}, 3);
}

static void test_write_unsigned_h4000 (void)
{
    _write_unsigned(0x4000, (const uint8_t[]){0x00, 0x80, 0x80, 0x01}, 4);
}

static void test_write_unsigned_h1FFFFF (void)
{
    _write_unsigned(0x1FFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0x7F}, 4);
}

static void test_write_unsigned_h200000 (void)
{
    _write_unsigned(0x200000, (const uint8_t[]){0x00, 0x80, 0x80, 0x80, 0x01}, 5);
}

static void test_write_unsigned_hFFFFFFF (void)
{
    _write_unsigned(0xFFFFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0xFF, 0x7F}, 5);
}

static void test_write_unsigned_h10000000 (void)
{
    _write_unsigned(0x10000000, (const uint8_t[]){0x00, 0x80, 0x80, 0x80, 0x80, 0x01}, 6);
}

static void test_write_unsigned_h7FFFFFFFF (void)
{
    _write_unsigned(0x7FFFFFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, 6);
}

static void test_write_unsigned_h800000000 (void)
{
    _write_unsigned(0x800000000, (const uint8_t[]){0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 7);
}

static void test_write_unsigned_h3FFFFFFFFFF (void)
{
    _write_unsigned(0x3FFFFFFFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, 7);
}

static void test_write_unsigned_h40000000000 (void)
{
    _write_unsigned(0x40000000000, (const uint8_t[]){0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 8);
}

static void test_write_unsigned_h1FFFFFFFFFFFF (void)
{
    _write_unsigned(0x1FFFFFFFFFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, 8);
}

static void test_write_unsigned_h2000000000000 (void)
{
    _write_unsigned(0x2000000000000, (const uint8_t[]){0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 9);
}

static void test_write_unsigned_hFFFFFFFFFFFFFF (void)
{
    _write_unsigned(0xFFFFFFFFFFFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, 9);
}

static void test_write_unsigned_h100000000000000 (void)
{
    _write_unsigned(0x100000000000000, (const uint8_t[]){0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 10);
}

static void test_write_unsigned_h7FFFFFFFFFFFFFFF (void)
{
    _write_unsigned(0x7FFFFFFFFFFFFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, 10);
}

static void test_write_unsigned_h8000000000000000 (void)
{
    _write_unsigned(0x8000000000000000, (const uint8_t[]){0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 11);
}

static void test_write_unsigned_hFFFFFFFFFFFFFFFF (void)
{
    _write_unsigned(0xFFFFFFFFFFFFFFFF, (const uint8_t[]){0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}, 11);
}

static void test_write_signed_min (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_signed(&ctx, 0, SOFAB_SIGNED_MIN);
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_signed_max (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_signed(&ctx, 0, SOFAB_SIGNED_MAX);
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x01, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_boolean (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_boolean(&ctx, 0, true);
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x00, 0x01};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_fp32 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_fp32(&ctx, 0, 3.1415f);
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x02, 0x20, 0x56, 0x0E, 0x49, 0x40};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_fp64 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    // using float to double conversion to ensure payload test
    ret = sofab_ostream_write_fp64(&ctx, 0, 3.14159265f);
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x02, 0x41, 0x00, 0x00, 0x00, 0x60, 0xFB, 0x21, 0x09, 0x40};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_string (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_string(&ctx, 0, "Hello Couch!");
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_string_empty (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_string(&ctx, 0, "");
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x02, 0x02};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_blob (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    uint8_t blob[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_blob(&ctx, 0, blob, sizeof(blob));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x02, 0x2B, 0x01, 0x02, 0x03, 0x04, 0x05};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_blob_empty (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_blob(&ctx, 0, NULL, 0);
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x02, 0x03};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_unsigned (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    uint32_t array[] = {1, 2, 3, 0x80000000, UINT32_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_unsigned(&ctx, 0, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x80, 0x80, 0x80, 0x80, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_signed (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const int32_t array[] = {-1, -2, -3, INT32_MIN, INT32_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_signed(&ctx, 0, array, sizeof(array) / sizeof(array[0]), sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_i8 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const int8_t array[] = {-1, -2, -3, INT8_MIN, INT8_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_i8(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0x01, 0xFE, 0x01};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_u8 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const uint8_t array[] = {1, 2, 3, 0, UINT8_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_u8(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0x01};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_i16 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const int16_t array[] = {-1, -2, -3, INT16_MIN, INT16_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_i16(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0x03, 0xFE, 0xFF, 0x03};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_u16 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const uint16_t array[] = {1, 2, 3, 0, UINT16_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_u16(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0xFF, 0x03};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_i32 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const int32_t array[] = {-1, -2, -3, INT32_MIN, INT32_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_i32(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_u32 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const uint32_t array[] = {1, 2, 3, 0, UINT32_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_u32(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_i64 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[32];
    memset(buffer, 0x55, sizeof(buffer));

    const int64_t array[] = {-1, -2, -3, INT64_MIN, INT64_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_i64(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {
        0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
          0xFF, 0xFF, 0x01, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
          0x01};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_u64 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[32];
    memset(buffer, 0x55, sizeof(buffer));

    const uint64_t array[] = {1, 2, 3, 0, UINT64_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_u64(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {
        0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x01};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_fp32 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[32];
    memset(buffer, 0x55, sizeof(buffer));

    const float array[] = {1.0f, 2.0f, 3.0f, -FLT_MAX, FLT_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_fp32(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {
        0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00,
        0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_fp64 (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[64];
    memset(buffer, 0x55, sizeof(buffer));

    const double array[] = {1.0, 2.0, 3.0, -DBL_MAX, DBL_MAX};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_fp64(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {
        0x05, 0x05, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0x7F};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_nested_sequence (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[64];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ostream_write_unsigned(&ctx, 0, 42);
    sofab_ostream_write_sequence_begin(&ctx, 1);
    {
        sofab_ostream_write_unsigned(&ctx, 0, 42);
        sofab_ostream_write_signed(&ctx, 2, -42);
    }
    sofab_ostream_write_sequence_end(&ctx);
    sofab_ostream_write_signed(&ctx, 2, -42);
    size_t used = sofab_ostream_flush(&ctx);

    const uint8_t expected[] = {0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x07, 0x11, 0x53};
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_nested_sequence_with_array (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[64];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ostream_write_unsigned(&ctx, 0, 42);
    sofab_ostream_write_sequence_begin(&ctx, 3);
    {
        sofab_ostream_write_unsigned(&ctx, 0, 42);
        sofab_ostream_write_array_of_signed(&ctx, 3, (int32_t[]){-42, -43, -44}, 3, sizeof(int32_t));
    }
    sofab_ostream_write_sequence_end(&ctx);
    sofab_ostream_write_signed(&ctx, 2, -42);
    size_t used = sofab_ostream_flush(&ctx);

    const uint8_t expected[] = {0x00, 0x2A, 0x1E, 0x00, 0x2A, 0x1C, 0x03, 0x53, 0x55, 0x57, 0x07, 0x11, 0x53};
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_nested_sequence_multilevel (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[128];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ostream_write_unsigned(&ctx, 0, 42);

    for (int i = 0; i < 10; i++)
    {
        sofab_ostream_write_sequence_begin(&ctx, 1);
        sofab_ostream_write_unsigned(&ctx, 0, 42);
        sofab_ostream_write_signed(&ctx, 2, -42);
    }

    for (int i = 0; i < 10; i++)
    {
        sofab_ostream_write_sequence_end(&ctx);
    }

    sofab_ostream_write_signed(&ctx, 2, -42);
    size_t used = sofab_ostream_flush(&ctx);

    const uint8_t expected[] = {
        0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x11, 0x53};
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_full_scale_example (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[512];
    memset(buffer, 0x55, sizeof(buffer));

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ostream_write_unsigned(&ctx, 0, 200);
    sofab_ostream_write_signed(&ctx, 1, -100);
    sofab_ostream_write_unsigned(&ctx, 2, 50000);
    sofab_ostream_write_signed(&ctx, 3, -20000);
    sofab_ostream_write_unsigned(&ctx, 4, 3000000000);
    sofab_ostream_write_signed(&ctx, 5, -1000000000);
    sofab_ostream_write_unsigned(&ctx, 6, 10000000000000);
    sofab_ostream_write_signed(&ctx, 7, -5000000000000);

    sofab_ostream_write_sequence_begin(&ctx, 10);
    {
        sofab_ostream_write_fp32(&ctx, 0, 3.14f);
        sofab_ostream_write_fp64(&ctx, 1, 3.14159265);
        sofab_ostream_write_string(&ctx, 2, "Hello, World!");
        sofab_ostream_write_blob(&ctx, 3, (const uint8_t[]){0xDE, 0xAD, 0xBE, 0xEF}, 4);
    }
    sofab_ostream_write_sequence_end(&ctx);

    sofab_ostream_write_sequence_begin(&ctx, 100);
    {
        sofab_ostream_write_array_of_u8(&ctx, 0, (const uint8_t[]){0, 64, 128, 191, 255}, 5);
        sofab_ostream_write_array_of_i8(&ctx, 1, (const int8_t[]){-128, -64, 0, 63, 127}, 5);
        sofab_ostream_write_array_of_u16(&ctx, 2, (const uint16_t[]){0, 16384, 32768, 49151, 65535}, 5);
        sofab_ostream_write_array_of_i16(&ctx, 3, (const int16_t[]){-32768, -16384, 0, 16383, 32767}, 5);
        sofab_ostream_write_array_of_u32(&ctx, 4, (const uint32_t[]){0, 1073741824, 2147483648, 3221225471, 4294967295}, 5);
        sofab_ostream_write_array_of_i32(&ctx, 5, (const int32_t[]){-2147483648, -1073741824, 0, 1073741823, 2147483647}, 5);
        sofab_ostream_write_array_of_u64(&ctx, 6, (const uint64_t[]){0, 4611686018427387904ULL, 9223372036854775808ULL, 13835058055282163711ULL, 18446744073709551615ULL}, 5);
        sofab_ostream_write_array_of_i64(&ctx, 7, (const int64_t[]){-9223372036854775807LL, -4611686018427387904LL, 0LL, 4611686018427387903LL, 9223372036854775807LL}, 5);

        sofab_ostream_write_sequence_begin(&ctx, 10);
        {
            sofab_ostream_write_array_of_fp32(&ctx, 0, (const float[]){+0.0, -0.0, +INFINITY, -INFINITY, NAN}, 5);
            sofab_ostream_write_array_of_fp64(&ctx, 1, (const double[]){+0.0, -0.0, +INFINITY, -INFINITY, NAN}, 5);
        }
        sofab_ostream_write_sequence_end(&ctx);
    }
    sofab_ostream_write_sequence_end(&ctx);

    sofab_ostream_write_sequence_begin(&ctx, 200);
    {
        sofab_ostream_write_string(&ctx, 0, "Hello, Sofab!");
        sofab_ostream_write_string(&ctx, 1, "");
        sofab_ostream_write_string(&ctx, 2, "1234567890");
        sofab_ostream_write_string(&ctx, 3, "äöüÄÖÜß");
        sofab_ostream_write_string(&ctx, 4, "This_is_a_very_long_test_string_with_!@#$%^&*()_+-=[]{}");
    }
    sofab_ostream_write_sequence_end(&ctx);

    size_t used = sofab_ostream_flush(&ctx);

    const uint8_t expected[] = {
        0x00, 0xC8, 0x01, 0x09, 0xC7, 0x01, 0x10, 0xD0, 0x86, 0x03, 0x19, 0xBF,
        0xB8, 0x02, 0x20, 0x80, 0xBC, 0xC1, 0x96, 0x0B, 0x29, 0xFF, 0xA7, 0xD6,
        0xB9, 0x07, 0x30, 0x80, 0xC0, 0xCA, 0xF3, 0x84, 0xA3, 0x02, 0x39, 0xFF,
        0xBF, 0xCA, 0xF3, 0x84, 0xA3, 0x02, 0x56, 0x02, 0x20, 0xC3, 0xF5, 0x48,
        0x40, 0x0A, 0x41, 0xF1, 0xD4, 0xC8, 0x53, 0xFB, 0x21, 0x09, 0x40, 0x12,
        0x6A, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x57, 0x6F, 0x72, 0x6C,
        0x64, 0x21, 0x1A, 0x23, 0xDE, 0xAD, 0xBE, 0xEF, 0x07, 0xA6, 0x06, 0x03,
        0x05, 0x00, 0x40, 0x80, 0x01, 0xBF, 0x01, 0xFF, 0x01, 0x0C, 0x05, 0xFF,
        0x01, 0x7F, 0x00, 0x7E, 0xFE, 0x01, 0x13, 0x05, 0x00, 0x80, 0x80, 0x01,
        0x80, 0x80, 0x02, 0xFF, 0xFF, 0x02, 0xFF, 0xFF, 0x03, 0x1C, 0x05, 0xFF,
        0xFF, 0x03, 0xFF, 0xFF, 0x01, 0x00, 0xFE, 0xFF, 0x01, 0xFE, 0xFF, 0x03,
        0x23, 0x05, 0x00, 0x80, 0x80, 0x80, 0x80, 0x04, 0x80, 0x80, 0x80, 0x80,
        0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x2C,
        0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00,
        0xFE, 0xFF, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F, 0x33, 0x05,
        0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x40, 0x80, 0x80,
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x01, 0x3C, 0x05, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x7F, 0x00, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFE,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x56, 0x05, 0x05,
        0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
        0x7F, 0x00, 0x00, 0x80, 0xFF, 0x00, 0x00, 0xC0, 0x7F, 0x0D, 0x05, 0x41,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x7F,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xF8, 0x7F, 0x07, 0x07, 0xC6, 0x0C, 0x02, 0x6A, 0x48, 0x65,
        0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x53, 0x6F, 0x66, 0x61, 0x62, 0x21, 0x0A,
        0x02, 0x12, 0x52, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x30, 0x1A, 0x72, 0xC3, 0xA4, 0xC3, 0xB6, 0xC3, 0xBC, 0xC3, 0x84, 0xC3,
        0x96, 0xC3, 0x9C, 0xC3, 0x9F, 0x22, 0xBA, 0x03, 0x54, 0x68, 0x69, 0x73,
        0x5F, 0x69, 0x73, 0x5F, 0x61, 0x5F, 0x76, 0x65, 0x72, 0x79, 0x5F, 0x6C,
        0x6F, 0x6E, 0x67, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x5F, 0x73, 0x74, 0x72,
        0x69, 0x6E, 0x67, 0x5F, 0x77, 0x69, 0x74, 0x68, 0x5F, 0x21, 0x40, 0x23,
        0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F, 0x2B, 0x2D, 0x3D, 0x5B,
        0x5D, 0x7B, 0x7D, 0x07};
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

int test_ostream_main (void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_buffer_set);
    RUN_TEST(test_buffer_flush);

    RUN_TEST(test_buffer_overflow_by_id_via_unsigned);
    RUN_TEST(test_buffer_overflow_by_id_via_signed);
    RUN_TEST(test_buffer_overflow_by_id_via_fixlen);
    RUN_TEST(test_buffer_overflow_by_id_via_array_of_unsigned);
    RUN_TEST(test_buffer_overflow_by_id_via_array_of_signed);
    RUN_TEST(test_buffer_overflow_by_id_via_array_of_fixlen);
    RUN_TEST(test_buffer_overflow_by_id_via_sequence_begin);
    RUN_TEST(test_buffer_overflow_by_id_via_sequence_end);

    RUN_TEST(test_buffer_overflow_by_unsigned_value);
    RUN_TEST(test_buffer_overflow_by_signed_value);
    RUN_TEST(test_buffer_overflow_by_fixlen_length);
    RUN_TEST(test_buffer_overflow_by_fixlen_value);

    RUN_TEST(test_buffer_overflow_by_array_count_via_array_of_unsigned);
    RUN_TEST(test_buffer_overflow_by_array_count_via_array_of_signed);
    RUN_TEST(test_buffer_overflow_by_array_count_via_array_of_fixlen);
    RUN_TEST(test_buffer_overflow_by_array_fixlen_length);
    RUN_TEST(test_buffer_overflow_by_array_fixlen_value);
    RUN_TEST(test_buffer_overflow_by_array_value_via_array_of_unsigned);
    RUN_TEST(test_buffer_overflow_by_array_value_via_array_of_signed);

    RUN_TEST(test_invalid_arg_via_array_of_unsigned_element_size);
    RUN_TEST(test_invalid_arg_via_array_of_signed_element_size);

    RUN_TEST(test_id_min);
    RUN_TEST(test_id_max);
    RUN_TEST(test_id_overflow);

    RUN_TEST(test_write_unsigned_h0);
    RUN_TEST(test_write_unsigned_h7F);
    RUN_TEST(test_write_unsigned_h80);
    RUN_TEST(test_write_unsigned_h3FFF);
    RUN_TEST(test_write_unsigned_h4000);
    RUN_TEST(test_write_unsigned_h1FFFFF);
    RUN_TEST(test_write_unsigned_h200000);
    RUN_TEST(test_write_unsigned_hFFFFFFF);
    RUN_TEST(test_write_unsigned_h10000000);
    RUN_TEST(test_write_unsigned_h7FFFFFFFF);
    RUN_TEST(test_write_unsigned_h800000000);
    RUN_TEST(test_write_unsigned_h3FFFFFFFFFF);
    RUN_TEST(test_write_unsigned_h40000000000);
    RUN_TEST(test_write_unsigned_h1FFFFFFFFFFFF);
    RUN_TEST(test_write_unsigned_h2000000000000);
    RUN_TEST(test_write_unsigned_hFFFFFFFFFFFFFF);
    RUN_TEST(test_write_unsigned_h100000000000000);
    RUN_TEST(test_write_unsigned_h7FFFFFFFFFFFFFFF);
    RUN_TEST(test_write_unsigned_h8000000000000000);
    RUN_TEST(test_write_unsigned_hFFFFFFFFFFFFFFFF);

    RUN_TEST(test_write_signed_min);
    RUN_TEST(test_write_signed_max);
    RUN_TEST(test_write_boolean);
    RUN_TEST(test_write_fp32);
    RUN_TEST(test_write_fp64);
    RUN_TEST(test_write_string);
    RUN_TEST(test_write_string_empty);
    RUN_TEST(test_write_blob);
    RUN_TEST(test_write_blob_empty);

    RUN_TEST(test_write_array_of_unsigned);
    RUN_TEST(test_write_array_of_signed);
    RUN_TEST(test_write_array_of_i8);
    RUN_TEST(test_write_array_of_u8);
    RUN_TEST(test_write_array_of_i16);
    RUN_TEST(test_write_array_of_u16);
    RUN_TEST(test_write_array_of_i32);
    RUN_TEST(test_write_array_of_u32);
    RUN_TEST(test_write_array_of_i64);
    RUN_TEST(test_write_array_of_u64);
    RUN_TEST(test_write_array_of_fp32);
    RUN_TEST(test_write_array_of_fp64);

    RUN_TEST(test_write_nested_sequence);
    RUN_TEST(test_write_nested_sequence_with_array);
    RUN_TEST(test_write_nested_sequence_multilevel);

    RUN_TEST(test_write_full_scale_example);

    return UNITY_END();
}
