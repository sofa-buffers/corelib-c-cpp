/*!
 * @file test_ostream.h
 * @brief SofaBuffers test for output stream C API
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/ostream.h"
#include "sofab/istream.h"   /* the hold-back tests decode their own output */

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
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
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
#endif
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

static void test_write_array_of_unsigned_empty (void)
{
    // §4.7: a zero-count unsigned array encodes as exactly [hdr][count=0].
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const uint32_t array[1] = {0};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_unsigned(&ctx, 0, array, 0, sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x03, 0x00};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_signed_empty (void)
{
    // §4.7: a zero-count signed array encodes as exactly [hdr][count=0].
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const int32_t array[1] = {0};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_signed(&ctx, 0, array, 0, sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

static void test_write_array_of_fp32_empty (void)
{
    // §4.8: a zero-count fixlen array still carries its fixlen_word (so an empty
    // fp32 and fp64 array stay distinguishable) but no payload —
    // exactly [hdr][count=0][fixlen_word].
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[16];
    memset(buffer, 0x55, sizeof(buffer));

    const float array[1] = {0};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_fp32(&ctx, 0, array, 0);
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {0x05, 0x00, 0x20}; // fixlen_word 0x20 = (4<<3)|fp32
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

static void test_write_array_of_fp32_specials (void)
{
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[32];
    memset(buffer, 0x55, sizeof(buffer));

    const float array[] = {+0.0, -0.0, +INFINITY, -INFINITY, NAN};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_fp32(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {
        0x05, 0x05, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
        0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0xFF
        /*, 0x00, 0x00, 0xC0, 0x7F => test NaN separately, as there are multiple binary representations */};

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used - sizeof(array[0]), "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used - sizeof(array[0]), "buffer != expected");

    float nan_value;
    memcpy(&nan_value, &buffer[used - sizeof(array[0])], sizeof(nan_value));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint32_t tmp;
    memcpy(&tmp, &nan_value, sizeof(tmp));
    tmp = __builtin_bswap32(tmp);
    memcpy(&nan_value, &tmp, sizeof(nan_value));
#endif

    TEST_ASSERT_TRUE_MESSAGE(isnan(nan_value), "last value is not NAN");
}

static void test_write_array_of_fp64 (void)
{
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
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
#endif
}

static void test_write_array_of_fp64_specials (void)
{
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
    sofab_ostream_t ctx;
    sofab_ret_t ret;
    uint8_t buffer[64];
    memset(buffer, 0x55, sizeof(buffer));

    const double array[] = {+0.0, -0.0, +INFINITY, -INFINITY, NAN};

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    ret = sofab_ostream_write_array_of_fp64(&ctx, 0, array, sizeof(array) / sizeof(array[0]));
    size_t used = sofab_ostream_bytes_used(&ctx);

    const uint8_t expected[] = {
        0x05, 0x05, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xF0, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFF,
        /* 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x7F => test NaN separately, as there are multiple binary representations */};

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used - sizeof(array[0]), "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used - sizeof(array[0]), "buffer != expected");

    double nan_value;
    memcpy(&nan_value, &buffer[used - sizeof(array[0])], sizeof(nan_value));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint64_t tmp;
    memcpy(&tmp, &nan_value, sizeof(tmp));
    tmp = __builtin_bswap64(tmp);
    memcpy(&nan_value, &tmp, sizeof(nan_value));
#endif

    TEST_ASSERT_TRUE_MESSAGE(isnan(nan_value), "last value is not NAN");
#endif
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

/* --- sequence framing (MESSAGE_SPEC §2) ------------------------------------ */

/*!
 * @brief Encode with a fresh 64-byte stream and compare the produced bytes.
 */
#define LAZY_CHECK(what, expected_arr, body)                                       \
    do {                                                                           \
        sofab_ostream_t ctx;                                                       \
        uint8_t buffer[64];                                                        \
        memset(buffer, 0x55, sizeof(buffer));                                      \
        sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);            \
        body;                                                                      \
        size_t used = sofab_ostream_flush(&ctx);                                   \
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected_arr), used, what);         \
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected_arr, buffer, used, what);    \
    } while (0)

/* The eager API keeps writing the §4.9 empty-sequence primitive, which is also the
 * explicit-empty wrapper of §2/§3. It is the only sequence API a
 * SOFAB_DISABLE_LAZY_SEQ_SUPPORT build has, so this test stays outside the guard
 * below. */
static void test_eager_sequence_without_content_still_frames (void)
{
    const uint8_t expected[] = {0x0E, 0x07};
    LAZY_CHECK("eager empty sequence must stay framed", expected, {
        sofab_ostream_write_sequence_begin(&ctx, 1);
        sofab_ostream_write_sequence_end(&ctx);
    });
}

/* Everything from here to the end of this section exercises the hold-back
 * openers, which SOFAB_DISABLE_LAZY_SEQ_SUPPORT compiles out (together with the
 * pending run in sofab_ostream_t and SOFAB_LAZY_SEQ_DEPTH). Guarded so the whole
 * hand-written C suite -- not just the flag-tolerant vector runner -- still
 * compiles and runs in that configuration; the no-lazy CI leg does exactly that. */
#if !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)

/* An all-default sequence carries no information, so nothing is emitted -- where
 * the eager API writes the two-byte empty frame. */
static void test_lazy_sequence_without_content_emits_nothing (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[64];
    memset(buffer, 0x55, sizeof(buffer));
    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
    sofab_ostream_write_sequence_end(&ctx);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, sofab_ostream_flush(&ctx),
        "an empty lazy sequence must emit nothing");
}

/* One child field commits the whole held-back run, outermost header first. */
static void test_lazy_sequence_commits_run_on_first_content (void)
{
    const uint8_t expected[] = {0x0E, 0x16, 0x00, 0x2A, 0x07, 0x07};
    LAZY_CHECK("held-back run must commit outermost-first", expected, {
        sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
        sofab_ostream_write_sequence_begin_lazy(&ctx, 2);
        sofab_ostream_write_unsigned(&ctx, 0, 42);
        sofab_ostream_write_sequence_end(&ctx);
        sofab_ostream_write_sequence_end(&ctx);
    });
}

/* Only the empty inner sequence drops; the outer one has content and is framed. */
static void test_lazy_sequence_drops_only_empty_inner (void)
{
    const uint8_t expected[] = {0x0E, 0x00, 0x2A, 0x07};
    LAZY_CHECK("only the empty inner sequence may drop", expected, {
        sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
        sofab_ostream_write_sequence_begin_lazy(&ctx, 2);
        sofab_ostream_write_sequence_end(&ctx);
        sofab_ostream_write_unsigned(&ctx, 0, 42);
        sofab_ostream_write_sequence_end(&ctx);
    });
}

/* Mixing the two APIs: an eager inner frame is content for the lazy outer one. */
static void test_eager_inner_frame_commits_lazy_outer (void)
{
    const uint8_t expected[] = {0x0E, 0x16, 0x07, 0x07};
    LAZY_CHECK("an eager inner frame must commit the lazy outer one", expected, {
        sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
        sofab_ostream_write_sequence_begin(&ctx, 2);
        sofab_ostream_write_sequence_end(&ctx);
        sofab_ostream_write_sequence_end(&ctx);
    });
}

/* The other closer, and the only thing that can put an empty frame on the wire
 * via the lazy path: sofab_ostream_write_sequence_end_keep(). An array ELEMENT
 * closes with it -- element presence carries the array's length (§5.1) -- while a
 * FIELD closes with sofab_ostream_write_sequence_end() and vanishes (§2).
 * Nothing in the C *object* path calls it: sofab_object_encode() decides omission
 * from the descriptor before it opens anything, and its role test is
 * info->fixed_seq (object.c), not this trio. The primitive exists for a message
 * layer that only discovers content as it writes -- the C++ wrapper, and
 * generated C -- so it is pinned here directly. */
static void test_lazy_sequence_end_keep_forces_empty_frame (void)
{
    const uint8_t expected[] = {0x0E, 0x07};
    LAZY_CHECK("end_keep must force out a contentless frame", expected, {
        sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
        sofab_ostream_write_sequence_end_keep(&ctx);
    });
}

/* end_keep behaves like a write: it commits the whole held-back run, outermost
 * header first, so the enclosing sequence is framed too. Without that, a kept
 * element would sit inside a wrapper that was never opened. */
static void test_lazy_sequence_end_keep_commits_outer_run (void)
{
    const uint8_t expected[] = {0x0E, 0x16, 0x07, 0x07};
    LAZY_CHECK("end_keep must commit the enclosing run too", expected, {
        sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
        sofab_ostream_write_sequence_begin_lazy(&ctx, 2);
        sofab_ostream_write_sequence_end_keep(&ctx);   /* element: framed */
        sofab_ostream_write_sequence_end(&ctx);        /* field: has content now */
    });
}

/* The array shape where the two closers meet: a wrapper field (id 200) holding
 * two all-default elements (ids 0 and 1). Byte-identical to the C++ wrapper's
 * "an all-default wrapper ELEMENT keeps its frame" (test/cpp/test_ostream.cpp).
 * Close the elements with the field closer instead and everything collapses --
 * elements, wrapper and field -- so a two-element array decodes as absent. */
static void test_lazy_wrapper_keeps_every_default_element (void)
{
    const uint8_t expected[] = {0xC6, 0x0C, 0x06, 0x07, 0x0E, 0x07, 0x07};
    LAZY_CHECK("every wrapper element must keep its frame", expected, {
        sofab_ostream_write_sequence_begin_lazy(&ctx, 200);
        sofab_ostream_write_sequence_begin_lazy(&ctx, 0);
        sofab_ostream_write_sequence_end_keep(&ctx);
        sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
        sofab_ostream_write_sequence_end_keep(&ctx);
        sofab_ostream_write_sequence_end(&ctx);
    });
}

static uint8_t _lazy_sink[256];
static size_t _lazy_sink_len;

static void _lazy_flush_cb (sofab_ostream_t *ctx, const uint8_t *data, size_t len, void *usrptr)
{
    (void)ctx; (void)usrptr;
    TEST_ASSERT_TRUE_MESSAGE(_lazy_sink_len + len <= sizeof(_lazy_sink), "sink overflow");
    memcpy(_lazy_sink + _lazy_sink_len, data, len);
    _lazy_sink_len += len;
}

/*! @brief The message every buffer size below must reproduce byte for byte.
 *
 * Ids 100/101/102 give two-byte sequence headers, so the committed run is six
 * bytes -- wider than most of the buffers it is pushed through. */
static void _lazy_flush_msg (sofab_ostream_t *ctx)
{
    sofab_ostream_write_sequence_begin_lazy(ctx, 100);
    sofab_ostream_write_sequence_begin_lazy(ctx, 101);
    sofab_ostream_write_sequence_begin_lazy(ctx, 102);
    sofab_ostream_write_unsigned(ctx, 0, 42);   /* commits all three headers */
    sofab_ostream_write_signed(ctx, 1, -7);
    sofab_ostream_write_sequence_end(ctx);
    sofab_ostream_write_sequence_begin_lazy(ctx, 103);
    sofab_ostream_write_sequence_end(ctx);      /* contentless: drops */
    sofab_ostream_write_sequence_end(ctx);
    sofab_ostream_write_sequence_end(ctx);
}

/*
 * A committed run that straddles a flush boundary yields byte-identical output
 * to the one-shot encode -- at every buffer size from one byte up.
 *
 * What this does NOT show, and no test can, is a flush landing *while* a header
 * is still held back. That is unreachable by construction: a held-back header is
 * stream state (an id in ctx->pending), not buffer content, so holding one back
 * cannot bring the buffer any closer to full; and the buffer only ever fills
 * through a write, which commits the whole pending run before its own first byte
 * is pushed. A pending run therefore can never straddle a flush. What can, and
 * what the loop below drives through every split point, is the *committed* run:
 * six bytes of headers emitted back-to-back through a buffer as small as one.
 */
static void test_lazy_committed_run_across_flush_matches_one_shot (void)
{
    sofab_ostream_t ctx;
    uint8_t oneshot[64];

    sofab_ostream_init(&ctx, oneshot, sizeof(oneshot), 0, NULL, NULL);
    _lazy_flush_msg(&ctx);
    size_t oneshot_len = sofab_ostream_flush(&ctx);
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(6, oneshot_len, "expected a multi-byte message");

    for (size_t buflen = 1; buflen <= 12; buflen++)
    {
        uint8_t small[12];
        char msg[64];
        snprintf(msg, sizeof(msg), "chunked encode differs at buflen %zu", buflen);

        _lazy_sink_len = 0;
        sofab_ostream_init(&ctx, small, buflen, 0, _lazy_flush_cb, NULL);
        _lazy_flush_msg(&ctx);
        sofab_ostream_flush(&ctx);

        TEST_ASSERT_EQUAL_size_t_MESSAGE(oneshot_len, _lazy_sink_len, msg);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(oneshot, _lazy_sink, oneshot_len, msg);
    }
}

/* --- the hold-back window (CORELIB_PLAN §6, "How deep the hold-back reaches") -
 *
 * This is a heap-free profile, so it takes the allowance §6 grants exactly one
 * kind of implementation: the pending run is bounded by SOFAB_LAZY_SEQ_DEPTH
 * instead of reaching SOFAB_MAX_DEPTH. The tests below pin both halves of that
 * documented bargain -- canonical up to the bound, well-formed-but-not-canonical
 * beyond it -- so the bound cannot drift away from what ostream.h and the README
 * promise. A port that CAN allocate must be canonical at every depth and would
 * fail the second test by design; that is the point of documenting the bound.
 */

/*! @brief Nest @p depth lazy sequences of id 1, close them all contentless. */
static size_t _lazy_nest_contentless (uint8_t *buf, size_t buflen, unsigned depth)
{
    sofab_ostream_t ctx;
    sofab_ostream_init(&ctx, buf, buflen, 0, NULL, NULL);
    for (unsigned i = 0; i < depth; i++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
            sofab_ostream_write_sequence_begin_lazy(&ctx, 1), "begin_lazy failed");
    }
    for (unsigned i = 0; i < depth; i++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
            sofab_ostream_write_sequence_end(&ctx), "sequence_end failed");
    }
    return sofab_ostream_flush(&ctx);
}

/* Up to the bound the encoder is canonical: nothing at all reaches the wire. */
static void test_lazy_window_at_bound_emits_nothing (void)
{
    uint8_t buffer[256];

    for (unsigned depth = 1; depth <= SOFAB_LAZY_SEQ_DEPTH; depth++)
    {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "%u contentless levels (<= SOFAB_LAZY_SEQ_DEPTH) must emit nothing", depth);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _lazy_nest_contentless(buffer, sizeof(buffer), depth), msg);
    }
}

/*
 * One level past the bound the run is committed and framed eagerly, so the empty
 * frames §2 would have omitted stay on the wire: SOFAB_LAZY_SEQ_DEPTH + 1 begins
 * followed by as many ends (only the innermost sequence, held back again after
 * the commit, still drops). Non-canonical by construction -- and this is the
 * exact consequence SOFAB_LAZY_SEQ_DEPTH is documented to have.
 */
static void test_lazy_window_beyond_bound_frames_eagerly (void)
{
    uint8_t buffer[256];
    const unsigned framed = SOFAB_LAZY_SEQ_DEPTH + 1;

    for (unsigned depth = framed; depth <= framed + 1; depth++)
    {
        size_t used = _lazy_nest_contentless(buffer, sizeof(buffer), depth);

        TEST_ASSERT_EQUAL_size_t_MESSAGE(2 * framed, used,
            "beyond the window the bounded profile frames eagerly");
        for (unsigned i = 0; i < framed; i++)
        {
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0E, buffer[i], "expected a sequence-begin(1) header");
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x07, buffer[framed + i], "expected a sequence-end marker");
        }
    }
}

/*! @brief Field callback that counts the (unbound, hence skipped) top-level fields. */
static void _count_fields_cb (sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)ctx; (void)size; (void)count;
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, id, "the only top-level field is the outer sequence");
    (*(unsigned *)usrptr)++;
}

/*
 * The non-canonical output stays interoperable: 40 levels -- far past the window,
 * and the depth an allocating port would hold back in full -- decode as a
 * COMPLETE message. Every emitted byte is either a begin(1) header or an end
 * marker, in equal numbers, and the whole thing carries not one value-bearing
 * field: the decoder sees a single empty top-level sequence, which is exactly the
 * non-canonical form MESSAGE_SPEC §2 requires every decoder to accept and
 * normalize back to the all-default value the canonical zero bytes denote. That
 * both forms rebuild the same object is asserted one layer up, in
 * test_object_default_sequence_roundtrips_both_forms (test_object.c).
 */
static void test_lazy_deep_nesting_is_wellformed_and_value_identical (void)
{
    uint8_t buffer[256];
    size_t used = _lazy_nest_contentless(buffer, sizeof(buffer), 40);
    size_t begins = 0, ends = 0;

    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(0, used,
        "a bounded window cannot stay canonical at depth 40 -- it frames eagerly");
    for (size_t i = 0; i < used; i++)
    {
        if (buffer[i] == 0x0E) begins++;
        else if (buffer[i] == 0x07) ends++;
        else TEST_FAIL_MESSAGE("unexpected byte in a contentless deep nesting");
    }
    TEST_ASSERT_EQUAL_size_t_MESSAGE(begins, ends, "unbalanced sequence framing");

    unsigned fields = 0;
    sofab_istream_t is;
    sofab_istream_init(&is, _count_fields_cb, &fields);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_istream_feed(&is, buffer, used),
        "the eagerly framed form must decode as a complete message");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, fields,
        "only the outer empty frame is visible; it carries no value");

    /* And the canonical form of the same value is the empty byte string, which
     * decodes to COMPLETE with nothing reported at all. */
    fields = 0;
    sofab_istream_init(&is, _count_fields_cb, &fields);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_istream_feed(&is, NULL, 0),
        "the canonical zero-byte message must decode as complete");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, fields, "zero bytes carry no fields");
}

/*
 * Depth bookkeeping: after a run that overflowed the window and was committed
 * eagerly, the pending state is back to empty, so the next sequence is held back
 * again and a contentless one still drops. (If an overflow ever left npending
 * stale, ctx->pending would stop being a suffix of the open sequences and this
 * would emit a stray frame -- or drop a real one.)
 */
static void test_lazy_window_overflow_leaves_state_clean (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[256];

    /* No flush callback, so sofab_ostream_flush() only reports the cumulative
     * byte count; the cursor keeps advancing and the deltas below are what each
     * step added. */
    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    for (unsigned i = 0; i < SOFAB_LAZY_SEQ_DEPTH + 4; i++)
        sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
    for (unsigned i = 0; i < SOFAB_LAZY_SEQ_DEPTH + 4; i++)
        sofab_ostream_write_sequence_end(&ctx);
    size_t after_overflow = sofab_ostream_flush(&ctx);
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(0, after_overflow, "expected eager frames past the window");

    /* Same stream, now a shallow contentless sequence: it must vanish again. */
    sofab_ostream_write_sequence_begin_lazy(&ctx, 2);
    sofab_ostream_write_sequence_end(&ctx);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(after_overflow, sofab_ostream_flush(&ctx),
        "the window must be empty again after an overflow");

    /* ...and a shallow one WITH content must still frame exactly once. */
    sofab_ostream_write_sequence_begin_lazy(&ctx, 2);
    sofab_ostream_write_unsigned(&ctx, 0, 42);
    sofab_ostream_write_sequence_end(&ctx);
    const uint8_t expected[] = {0x16, 0x00, 0x2A, 0x07};
    TEST_ASSERT_EQUAL_size_t_MESSAGE(after_overflow + sizeof(expected), sofab_ostream_flush(&ctx),
        "a post-overflow sequence must frame exactly once");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer + after_overflow, sizeof(expected),
        "post-overflow framing bytes mismatch");
}

/*
 * The other way a commit can end early: the buffer runs out in the middle of the
 * held-back run. The ids that did not make it must stay pending, because their
 * sofab_ostream_write_sequence_end() calls are still coming -- a run dropped on
 * the way out would leave end markers for headers that were never written, i.e.
 * an unbalanced stream, even after the caller recovers.
 *
 * Reproduced with a one-byte buffer and no flush callback: two held-back headers,
 * one byte of room. The recovery path is the documented one -- hand the stream a
 * fresh buffer with sofab_ostream_buffer_set() and carry on -- and the two
 * buffers concatenated must equal the one-shot encode byte for byte.
 */
static void test_lazy_commit_buffer_full_keeps_pending_run (void)
{
    sofab_ostream_t ctx;
    uint8_t oneshot[16];
    uint8_t first[1];
    uint8_t rest[16];

    /* Reference: the same message through a buffer that never fills. */
    sofab_ostream_init(&ctx, oneshot, sizeof(oneshot), 0, NULL, NULL);
    sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
    sofab_ostream_write_sequence_begin_lazy(&ctx, 2);
    sofab_ostream_write_unsigned(&ctx, 0, 42);
    sofab_ostream_write_sequence_end(&ctx);
    sofab_ostream_write_sequence_end(&ctx);
    size_t oneshot_len = sofab_ostream_flush(&ctx);
    const uint8_t reference[] = {0x0E, 0x16, 0x00, 0x2A, 0x07, 0x07};
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(reference), oneshot_len, "reference length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(reference, oneshot, oneshot_len, "reference bytes");

    /* Now the same calls through a one-byte buffer with no flush callback. */
    memset(first, 0x55, sizeof(first));
    sofab_ostream_init(&ctx, first, sizeof(first), 0, NULL, NULL);
    sofab_ostream_write_sequence_begin_lazy(&ctx, 1);
    sofab_ostream_write_sequence_begin_lazy(&ctx, 2);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_BUFFER_FULL,
        sofab_ostream_write_unsigned(&ctx, 0, 42),
        "the commit must report the full buffer");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1, sofab_ostream_bytes_used(&ctx),
        "exactly the one header that fits may be written");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0E, first[0],
        "the outermost held-back header is the one that fits");

    /* Recover: a fresh buffer, then finish the message unchanged. The header that
     * did not fit is still pending, so it comes out here. */
    sofab_ostream_buffer_set(&ctx, rest, sizeof(rest), 0);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_ostream_write_unsigned(&ctx, 0, 42),
        "the retried write must succeed on the fresh buffer");
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_ostream_write_sequence_end(&ctx), "inner end");
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_ostream_write_sequence_end(&ctx), "outer end");
    size_t rest_len = sofab_ostream_flush(&ctx);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(oneshot_len, 1 + rest_len,
        "recovered stream length differs from the one-shot encode");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(oneshot + 1, rest, rest_len,
        "recovered stream differs from the one-shot encode");
}

#endif /* !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT) */

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
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
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
            sofab_ostream_write_array_of_fp32(&ctx, 0, (const float[]){1.0, 2.0, 3.0, -FLT_MAX, FLT_MAX}, 5);
            sofab_ostream_write_array_of_fp64(&ctx, 1, (const double[]){1.0, 2.0, 3.0, -DBL_MAX, DBL_MAX}, 5);
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
        0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40,
        0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x0D, 0x05, 0x41,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xEF, 0x7F, 0x07, 0x07, 0xC6, 0x0C, 0x02, 0x6A, 0x48, 0x65,
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
#endif
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
    RUN_TEST(test_write_array_of_unsigned_empty);
    RUN_TEST(test_write_array_of_signed_empty);
    RUN_TEST(test_write_array_of_fp32_empty);
    RUN_TEST(test_write_array_of_i8);
    RUN_TEST(test_write_array_of_u8);
    RUN_TEST(test_write_array_of_i16);
    RUN_TEST(test_write_array_of_u16);
    RUN_TEST(test_write_array_of_i32);
    RUN_TEST(test_write_array_of_u32);
    RUN_TEST(test_write_array_of_i64);
    RUN_TEST(test_write_array_of_u64);
    RUN_TEST(test_write_array_of_fp32);
    RUN_TEST(test_write_array_of_fp32_specials);
    RUN_TEST(test_write_array_of_fp64);
    RUN_TEST(test_write_array_of_fp64_specials);

    RUN_TEST(test_write_nested_sequence);
    RUN_TEST(test_eager_sequence_without_content_still_frames);
#if !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
    RUN_TEST(test_lazy_sequence_without_content_emits_nothing);
    RUN_TEST(test_lazy_sequence_commits_run_on_first_content);
    RUN_TEST(test_lazy_sequence_drops_only_empty_inner);
    RUN_TEST(test_eager_inner_frame_commits_lazy_outer);
    RUN_TEST(test_lazy_sequence_end_keep_forces_empty_frame);
    RUN_TEST(test_lazy_sequence_end_keep_commits_outer_run);
    RUN_TEST(test_lazy_wrapper_keeps_every_default_element);
    RUN_TEST(test_lazy_committed_run_across_flush_matches_one_shot);
    RUN_TEST(test_lazy_window_at_bound_emits_nothing);
    RUN_TEST(test_lazy_window_beyond_bound_frames_eagerly);
    RUN_TEST(test_lazy_deep_nesting_is_wellformed_and_value_identical);
    RUN_TEST(test_lazy_window_overflow_leaves_state_clean);
    RUN_TEST(test_lazy_commit_buffer_full_keeps_pending_run);
#endif /* !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT) */

    RUN_TEST(test_write_nested_sequence_with_array);
    RUN_TEST(test_write_nested_sequence_multilevel);

    RUN_TEST(test_write_full_scale_example);

    return UNITY_END();
}
