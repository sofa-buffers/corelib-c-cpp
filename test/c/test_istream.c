/*!
 * @file test_istream.h
 * @brief SofaBuffers test for input stream C API
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/istream.h"

#include "unity.h"

#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>


/*****************************************************************************/
/* tests */
/*****************************************************************************/

typedef enum
{
    FIELD_TYPE_INT8,
    FIELD_TYPE_INT8U,
    FIELD_TYPE_INT16,
    FIELD_TYPE_INT16U,
    FIELD_TYPE_INT32,
    FIELD_TYPE_INT32U,
    FIELD_TYPE_INT64,
    FIELD_TYPE_INT64U,
    FIELD_TYPE_FP32,
    FIELD_TYPE_FP64,
    FIELD_TYPE_STRING,
    FIELD_TYPE_BLOB,
    FIELD_TYPE_BOOLEAN,
    FIELD_TYPE_ARRAY_INT8,
    FIELD_TYPE_ARRAY_INT8U,
    FIELD_TYPE_ARRAY_INT16,
    FIELD_TYPE_ARRAY_INT16U,
    FIELD_TYPE_ARRAY_INT32,
    FIELD_TYPE_ARRAY_INT32U,
    FIELD_TYPE_ARRAY_INT64,
    FIELD_TYPE_ARRAY_INT64U,
    FIELD_TYPE_ARRAY_FP32,
    FIELD_TYPE_ARRAY_FP64,
    FIELD_TYPE_UNSIGNED_ERROR,
    FIELD_TYPE_SIGNED_ERROR,
    FIELD_TYPE_FP32_ERROR,
} field_type_t;

typedef struct
{
    sofab_id_t expected_id;
    field_type_t target_type;
    void *target_ptr;
    size_t target_size;
    size_t filed_size;
    size_t filed_count;
    int calls;
} test_single_field_t;

static void _single_field_callback(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    test_single_field_t *test = (test_single_field_t *)usrptr;
    if (test == NULL)
    {
        return;
    }

    test->filed_size = size;
    test->filed_count = count;
    test->calls++;

    if (id != test->expected_id)
    {
        return;
    }

    switch (test->target_type)
    {
        case FIELD_TYPE_INT8:
            sofab_istream_read_i8(ctx, (int8_t *)test->target_ptr);
            break;
        case FIELD_TYPE_INT8U:
            sofab_istream_read_u8(ctx, (uint8_t *)test->target_ptr);
            break;
        case FIELD_TYPE_INT16:
            sofab_istream_read_i16(ctx, (int16_t *)test->target_ptr);
            break;
        case FIELD_TYPE_INT16U:
            sofab_istream_read_u16(ctx, (uint16_t *)test->target_ptr);
            break;
        case FIELD_TYPE_INT32:
            sofab_istream_read_i32(ctx, (int32_t *)test->target_ptr);
            break;
        case FIELD_TYPE_INT32U:
            sofab_istream_read_u32(ctx, (uint32_t *)test->target_ptr);
            break;
        case FIELD_TYPE_INT64:
            sofab_istream_read_i64(ctx, (int64_t *)test->target_ptr);
            break;
        case FIELD_TYPE_INT64U:
            sofab_istream_read_u64(ctx, (uint64_t *)test->target_ptr);
            break;
        case FIELD_TYPE_FP32:
            sofab_istream_read_fp32(ctx, (float *)test->target_ptr);
            break;
        case FIELD_TYPE_FP64:
            sofab_istream_read_fp64(ctx, (double *)test->target_ptr);
            break;
        case FIELD_TYPE_STRING:
            sofab_istream_read_string(ctx, (char *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_BLOB:
            sofab_istream_read_blob(ctx, (uint8_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_BOOLEAN:
            sofab_istream_read_bool(ctx, (bool *)test->target_ptr);
            break;
        case FIELD_TYPE_ARRAY_INT8:
            sofab_istream_read_array_of_i8(ctx, (int8_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_INT8U:
            sofab_istream_read_array_of_u8(ctx, (uint8_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_INT16:
            sofab_istream_read_array_of_i16(ctx, (int16_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_INT16U:
            sofab_istream_read_array_of_u16(ctx, (uint16_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_INT32:
            sofab_istream_read_array_of_i32(ctx, (int32_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_INT32U:
            sofab_istream_read_array_of_u32(ctx, (uint32_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_INT64:
            sofab_istream_read_array_of_i64(ctx, (int64_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_INT64U:
            sofab_istream_read_array_of_u64(ctx, (uint64_t *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_FP32:
            sofab_istream_read_array_of_fp32(ctx, (float *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_ARRAY_FP64:
            sofab_istream_read_array_of_fp64(ctx, (double *)test->target_ptr, test->target_size);
            break;
        case FIELD_TYPE_UNSIGNED_ERROR:
            sofab_istream_read_field(ctx, test->target_ptr, 5 /* invalid size */,
                SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
            break;
        case FIELD_TYPE_SIGNED_ERROR:
            sofab_istream_read_field(ctx, test->target_ptr, 5 /* invalid size */,
                SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED));
            break;
        case FIELD_TYPE_FP32_ERROR:
            sofab_istream_read_field(ctx, test->target_ptr, 3 /* invalid size */,
                SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
                SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32));
            break;
    }
}

static void test_init (void)
{
    int usrdata = 0;

    sofab_istream_t ctx;

    sofab_istream_init(&ctx, _single_field_callback, &usrdata);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(_single_field_callback, ctx.decoder->field_callback, "decoder->field_callback != _single_field_callback");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&usrdata, ctx.decoder->usrptr, "decoder->usrptr != &usrdata");
}

static void test_feed_buffer (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0x7F};

    uint8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT8U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(127, value);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_feed_buffer_stream (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};

    char value[16] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_STRING,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);

    for (size_t i = 0; i < sizeof(buffer); i++)
    {
        ret = sofab_istream_feed(&ctx, &buffer[i], 1);
        TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    }
    TEST_ASSERT_EQUAL_STRING("Hello Couch!", value);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_usage_invalid_field_type (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

    uint64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_FP32, // invalid type test (fp32 != u64)
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_USAGE, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_usage_invalid_field_type_fixlen (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};

    uint8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT8U, // invalid field type (u8 != string)
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_USAGE, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_usage_invalid_target_len_varint_unsigned (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

    uint64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_UNSIGNED_ERROR, // invalid size test
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_USAGE, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_usage_invalid_target_len_varint_signed (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

    int64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_SIGNED_ERROR, // invalid size test
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_USAGE, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_read_nothing (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};

    sofab_istream_init(&ctx, _single_field_callback, NULL);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
}

static void test_id_max (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0xF8, 0xFF, 0xFF, 0xFF, 0x3F, 0x00};

    uint8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = SOFAB_ID_MAX,
        .target_type = FIELD_TYPE_INT8U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0, value);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_msg_invalid_id_overflow (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0xF8, 0xFF, 0xFF, 0xFF, 0x7F, 0x00};

    uint8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = SOFAB_ID_MAX,
        .target_type = FIELD_TYPE_INT8U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(0, test.calls); // due to id overflow, callback is not called
}

static void test_msg_invalid_varint_unsigned_overflow (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF /* error */, 0x01};

    uint64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT64U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_msg_invalid_varint_signed_overflow (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF /* error */, 0x01};

    int64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT64,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_msg_invalid_fixlen_length_overflow (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x02,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF /* error */, 0x01,
        0x56, 0x0E, 0x49, 0x40};

    float value = 0.0f;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_FP32,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(0, test.calls); // due to fixlen lenght overflow, callback is not called
}

static void test_msg_invalid_array_count_overflow (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x04,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF /* error */, 0x01,
        0x53};

    int8_t value[128] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT8,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(0, test.calls); // due to array count overflow, callback is not called
}

static void test_msg_invalid_array_count_zero (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x04,
        0x00,
        0x53};

    int8_t value[128] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT8,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(0, test.calls); // due to zero array count, callback is not called
}

static void test_msg_invalid_array_fixlen_type (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x05, 0x05, 0x27 /* invalid fixlen type 0x7 */, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00,
        0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F};

    float value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_FP32,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(0, test.calls); // due to invalid fixlen type, callback is not called
}

static void test_msg_invalid_target_len_fixlen (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x20, 0x56, 0x0E, 0x49, 0x40};

    float value = 0.0f;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_FP32_ERROR, // invalid size test
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_msg_invalid_target_len_fixlen_string (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};

    char value[12] = {0}; // "Hello Couch!\0" needs 13 bytes
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_STRING,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_msg_invalid_target_array_count_too_small (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0x01, 0xFE, 0x01};

    int8_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT8,
        .target_ptr = &value,
        .target_size = 2, // smaller than the 5 elements in the message
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_msg_invalid_target_array_count_too_big (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0x01, 0xFE, 0x01};

    int8_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT8,
        .target_ptr = &value,
        .target_size = 10, // larger than the 5 elements in the message
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_read_unsigned_min (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0x00};

    uint8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT8U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_unsigned_max (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

    uint64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT64U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_signed_min (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

    int64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT64,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_INT64(INT64_MIN, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_signed_max (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};

    int64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT64,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT64(INT64_MAX, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_i8 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFE, 0x01};

    int8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT8,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_INT8(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_u8 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0x7F};

    uint8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT8U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_i16 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFE, 0x01};

    int16_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT16,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_INT16(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_u16 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0x7F};

    uint16_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT16U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_i32 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFE, 0x01};

    int32_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT32,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_INT32(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_u32 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0x7F};

    uint32_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT32U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_i64 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x01, 0xFE, 0x01};

    int64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT64,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_INT64(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_u64 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0x7F};

    uint64_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_INT64U,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT64(0x7F, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_boolean (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x00, 0x01};

    bool value = false;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_BOOLEAN,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(true, value);

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_fp32 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x20, 0x56, 0x0E, 0x49, 0x40};

    float value = 0.0f;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_FP32,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_FLOAT(3.1415f, value);

    TEST_ASSERT_EQUAL(sizeof(value), test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_fp64 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x41, 0x00, 0x00, 0x00, 0x60, 0xFB, 0x21, 0x09, 0x40};

    double value = 0.0;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_FP64,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_DOUBLE(3.141592653589793f, value);

    TEST_ASSERT_EQUAL(sizeof(value), test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_string (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x62, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x43, 0x6F, 0x75, 0x63, 0x68, 0x21};

    char value[13] = {0}; // "Hello Couch!\0"
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_STRING,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_STRING("Hello Couch!", value);

    TEST_ASSERT_EQUAL(strlen(value), test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_blob (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x2B, 0x01, 0x02, 0x03, 0x04, 0x05};

    uint8_t value[16] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_BLOB,
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const uint8_t expected[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(sizeof(expected), test.filed_size);
    TEST_ASSERT_EQUAL(0, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_i8 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0x01, 0xFE, 0x01};

    int8_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT8,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const int8_t expected[] = {-1, -2, -3, INT8_MIN, INT8_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_i8_varint_count (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x04, 0x80, 0x01, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
        0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53};

    int8_t value[128] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT8,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    int8_t expected[128];
    memset(expected, -42, sizeof(expected));
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_u8 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0x01};

    uint8_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT8U,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const uint8_t expected[] = {1, 2, 3, 0, UINT8_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_i16 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0x03, 0xFE, 0xFF, 0x03};

    int16_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT16,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const int16_t expected[] = {-1, -2, -3, INT16_MIN, INT16_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_u16 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0xFF, 0x03};

    uint16_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT16U,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const uint16_t expected[] = {1, 2, 3, 0, UINT16_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_i32 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F};

    int32_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT32,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const int32_t expected[] = {-1, -2, -3, INT32_MIN, INT32_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_u32 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};

    uint32_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT32U,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const uint32_t expected[] = {1, 2, 3, 0, UINT32_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_i64 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
          0xFF, 0xFF, 0x01, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
          0x01};

    int64_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT64,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const int64_t expected[] = {-1, -2, -3, INT64_MIN, INT64_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_u64 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x03, 0x05, 0x01, 0x02, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x01};

    uint64_t value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_INT64U,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const uint64_t expected[] = {1, 2, 3, 0, UINT64_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_fp32 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40, 0x00,
        0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F};

    float value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_FP32,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const float expected[] = {1.0f, 2.0f, 3.0f, -FLT_MAX, FLT_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(sizeof(value[0]), test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_array_of_fp64 (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x05, 0x05, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0x7F};

    double value[5] = {0};
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_ARRAY_FP64,
        .target_ptr = &value,
        .target_size = sizeof(value) / sizeof(value[0]),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    const double expected[] = {1.0, 2.0, 3.0, -DBL_MAX, DBL_MAX};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(sizeof(value[0]), test.filed_size);
    TEST_ASSERT_EQUAL(test.target_size, test.filed_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

typedef struct
{
    uint8_t u8;
    int8_t i8;
} test_nested_t;

typedef struct
{
    uint8_t u8;
    int8_t i8;
    test_nested_t nested;
} test_sequence_t;

static sofab_decoder_t _nested_sequence_decoder;

static void _fields(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    test_nested_t *seq = (test_nested_t *)usrptr;

    switch (id)
    {
        case 0:
            sofab_istream_read_u8(ctx, &seq->u8);
            break;
        case 2:
            sofab_istream_read_i8(ctx, &seq->i8);
            break;
    }
}

static void _fields_with_nested_sequence(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    test_sequence_t *seq = (test_sequence_t *)usrptr;

    switch (id)
    {
        case 0:
            sofab_istream_read_u8(ctx, &seq->u8);
            break;
        case 1:
            sofab_istream_read_sequence(ctx, &_nested_sequence_decoder, _fields, &seq->nested);
            break;
        case 2:
            sofab_istream_read_i8(ctx, &seq->i8);
            break;
    }
}

static void test_read_nested_sequence (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    /*
     * 0: u8 = 42
     * 1: nested sequence
     *    0: u8 = 42
     *    2: i8 = -42
     * 2: i8 = -42
     */
    const uint8_t buffer[] = {0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x07, 0x11, 0x53};

    test_sequence_t value = {0};

    sofab_istream_init(&ctx, _fields_with_nested_sequence, &value);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(42, value.u8);
    TEST_ASSERT_EQUAL_UINT8(42, value.nested.u8);
    TEST_ASSERT_EQUAL_INT8(-42, value.nested.i8);
    TEST_ASSERT_EQUAL_INT8(-42, value.i8);
}

static void test_read_nested_sequence_skip (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    /*
     * 0: u8 = 42
     * 1: nested sequence (1 is ignored)
     *    0: u8 = 42
     *    2: i8 = -42
     * 2: i8 = -42
     */
    const uint8_t buffer[] = {0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x07, 0x11, 0x53};

    test_sequence_t value =
    {
        .u8 = 0x55,
        .i8 = 0x55,
        .nested = {
            .u8 = 0x55,
            .i8 = 0x55
        }
    };

    sofab_istream_init(&ctx, _fields, &value);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(42, value.u8);
    TEST_ASSERT_EQUAL_UINT8(0x55, value.nested.u8);
    TEST_ASSERT_EQUAL_INT8(0x55, value.nested.i8);
    TEST_ASSERT_EQUAL_INT8(-42, value.i8);
}

static void test_read_nested_sequence_skip_with_array (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    /*
     * 0: u8 = 42
     * 3: nested sequence (3 is ignored)
     *    0: u8 = 42
     *    3: i8[] = -42, -43, -44
     * 2: i8 = -42
     */
    const uint8_t buffer[] = {0x00, 0x2A, 0x1E, 0x00, 0x2A, 0x1C, 0x03, 0x53, 0x55, 0x57, 0x07, 0x11, 0x53};

    test_sequence_t value =
    {
        .u8 = 0x55,
        .i8 = 0x55,
        .nested = {
            .u8 = 0x55,
            .i8 = 0x55
        }
    };

    sofab_istream_init(&ctx, _fields, &value);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(42, value.u8);
    TEST_ASSERT_EQUAL_UINT8(0x55, value.nested.u8);
    TEST_ASSERT_EQUAL_INT8(0x55, value.nested.i8);
    TEST_ASSERT_EQUAL_INT8(-42, value.i8);
}

static void test_read_nested_sequence_skip_multilevel (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    /*
     * 0: u8 = 42
     * 1: nested sequence
     *    0: u8 = 42
     *    2: i8 = -42
     *    1: nested sequence
     * 	      0: u8 = 42
     *        2: i8 = -42
     *        1: nested sequence ...
     * 2: i8 = -42
     */
    const uint8_t buffer[] = {
        0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x11, 0x53};

    test_sequence_t value =
    {
        .u8 = 0x55,
        .i8 = 0x55,
        .nested = {
            .u8 = 0x55,
            .i8 = 0x55
        }
    };

    sofab_istream_init(&ctx, _fields, &value);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(42, value.u8);
    TEST_ASSERT_EQUAL_UINT8(0x55, value.nested.u8);
    TEST_ASSERT_EQUAL_INT8(0x55, value.nested.i8);
    TEST_ASSERT_EQUAL_INT8(-42, value.i8);
}

static void test_msg_invalid_nested_sequence_extra_end (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    /*
     * 0: u8 = 42
     * 1: nested sequence
     *    0: u8 = 42
     *    2: i8 = -42
     *    1: nested sequence
     * 	      0: u8 = 42
     *        2: i8 = -42
     *        1: nested sequence ...
     * + one SEQUENCE_END extra!
     * 2: i8 = -42
     */
    const uint8_t buffer[] = {
        0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07 /* invalid */, 0x11, 0x53};

    test_sequence_t value =
    {
        .u8 = 0x55,
        .i8 = 0x55,
        .nested = {
            .u8 = 0x55,
            .i8 = 0x55
        }
    };

    sofab_istream_init(&ctx, _fields, &value);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
}

static void test_msg_invalid_nested_sequence_depth (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    /*
     * 0: u8 = 42
     * 1: nested sequence - invalid 256 levels! (depth limit is 255)
     * 2: i8 = -42
     */
    const uint8_t buffer[] = {
        0x00, 0x2A, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11,
        0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E,
        0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A,
        0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53,
        0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00, 0x2A, 0x11, 0x53, 0x0E, 0x00,
        0x2A, 0x11, 0x53, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x11, 0x53};

    test_sequence_t value =
    {
        .u8 = 0x55,
        .i8 = 0x55,
        .nested = {
            .u8 = 0x55,
            .i8 = 0x55
        }
    };

    sofab_istream_init(&ctx, _fields, &value);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, ret);
}

int test_istream_main (void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_feed_buffer);
    RUN_TEST(test_feed_buffer_stream);
    RUN_TEST(test_usage_invalid_field_type);
    RUN_TEST(test_usage_invalid_field_type_fixlen);
    RUN_TEST(test_usage_invalid_target_len_varint_unsigned);
    RUN_TEST(test_usage_invalid_target_len_varint_signed);
    RUN_TEST(test_read_nothing);

    RUN_TEST(test_id_max);

    RUN_TEST(test_msg_invalid_id_overflow);
    RUN_TEST(test_msg_invalid_varint_unsigned_overflow);
    RUN_TEST(test_msg_invalid_varint_signed_overflow);
    RUN_TEST(test_msg_invalid_fixlen_length_overflow);
    RUN_TEST(test_msg_invalid_array_count_overflow);
    RUN_TEST(test_msg_invalid_array_count_zero);
    RUN_TEST(test_msg_invalid_array_fixlen_type);
    RUN_TEST(test_msg_invalid_nested_sequence_depth);
    RUN_TEST(test_msg_invalid_nested_sequence_extra_end);
    RUN_TEST(test_msg_invalid_target_len_fixlen);
    RUN_TEST(test_msg_invalid_target_len_fixlen_string);
    RUN_TEST(test_msg_invalid_target_array_count_too_small);
    RUN_TEST(test_msg_invalid_target_array_count_too_big);

    RUN_TEST(test_read_unsigned_min);
    RUN_TEST(test_read_unsigned_max);
    RUN_TEST(test_read_signed_min);
    RUN_TEST(test_read_signed_max);
    RUN_TEST(test_read_i8);
    RUN_TEST(test_read_u8);
    RUN_TEST(test_read_i16);
    RUN_TEST(test_read_u16);
    RUN_TEST(test_read_i32);
    RUN_TEST(test_read_u32);
    RUN_TEST(test_read_i64);
    RUN_TEST(test_read_u64);
    RUN_TEST(test_read_fp32);
    RUN_TEST(test_read_fp64);
    RUN_TEST(test_read_boolean);
    RUN_TEST(test_read_fp32);
    RUN_TEST(test_read_fp64);
    RUN_TEST(test_read_string);
    RUN_TEST(test_read_blob);

    RUN_TEST(test_read_array_of_i8);
    RUN_TEST(test_read_array_of_i8_varint_count);
    RUN_TEST(test_read_array_of_u8);
    RUN_TEST(test_read_array_of_i16);
    RUN_TEST(test_read_array_of_u16);
    RUN_TEST(test_read_array_of_i32);
    RUN_TEST(test_read_array_of_u32);
    RUN_TEST(test_read_array_of_i64);
    RUN_TEST(test_read_array_of_u64);
    RUN_TEST(test_read_array_of_fp32);
    RUN_TEST(test_read_array_of_fp64);

    RUN_TEST(test_read_nested_sequence);
    RUN_TEST(test_read_nested_sequence_skip);
    RUN_TEST(test_read_nested_sequence_skip_with_array);
    RUN_TEST(test_read_nested_sequence_skip_multilevel);

    return UNITY_END();
}
