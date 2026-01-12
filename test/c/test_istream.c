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
    size_t field_size;
    size_t field_count;
    int calls;
} test_single_field_t;

static void _single_field_callback(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    test_single_field_t *test = (test_single_field_t *)usrptr;
    if (test == NULL)
    {
        return;
    }

    test->field_size = size;
    test->field_count = count;
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
    const uint8_t buffer[] = {0x04, 0x05, 0x01, 0x03, 0x05, 0xFF, 0x01, 0xFE, 0x01};

    uint8_t value = 0x55;
    test_single_field_t test =
    {
        .expected_id = 0,
        .target_type = FIELD_TYPE_STRING, // invalid field type (string != i8_array)
        .target_ptr = &value,
        .target_size = sizeof(value),
        .calls = 0
    };

    sofab_istream_init(&ctx, _single_field_callback, &test);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_USAGE, ret);
    TEST_ASSERT_EQUAL_UINT8(1, test.calls);
}

static void test_usage_invalid_field_type_array (void)
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

static void test_msg_invalid_varint_unsigned_varint_overflow (void)
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

static void test_msg_invalid_varint_signed_varint_overflow (void)
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

static void test_msg_invalid_fixlen_length_varint_overflow (void)
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

static void test_msg_invalid_fixlen_length_limit_overflow (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x02,
        0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03,
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
    TEST_ASSERT_EQUAL_UINT8(0, test.calls); // due to fixlen length > SOFAB_FIXLEN_MAX, callback is not called
}

static void test_msg_invalid_array_count_varint_overflow (void)
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

static void test_msg_invalid_array_count_limit_overflow (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
        0x04,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
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
    TEST_ASSERT_EQUAL_UINT8(0, test.calls); // due to array count > SOFAB_ARRAY_MAX, callback is not called
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(sizeof(value), test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(sizeof(value), test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(strlen(value), test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_string_empty (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x02};

    char value[] = {0x55, 0x55, 0x55, 0x55};
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
    TEST_ASSERT_EQUAL_STRING("", value);

    TEST_ASSERT_EQUAL(strlen(value), test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(sizeof(expected), test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
    TEST_ASSERT_EQUAL(1, test.calls);
}

static void test_read_blob_empty (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {0x02, 0x03};

    uint8_t value[4] = {0x55, 0x55, 0x55, 0x55};
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

    // empty blob, value should remain unchanged
    const uint8_t expected[] = {0x55, 0x55, 0x55, 0x55};
    TEST_ASSERT_EQUAL_MEMORY(expected, value, sizeof(expected));

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(0, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(0, test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(sizeof(value[0]), test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

    TEST_ASSERT_EQUAL(sizeof(value[0]), test.field_size);
    TEST_ASSERT_EQUAL(test.target_size, test.field_count);
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

typedef struct
{
    float f32;
    double f64;
    char str[32];
    uint8_t bytes[4];
} full_scale_seq_struct_t;

typedef struct
{
    float fp32[5];
    double fp64[5];
} full_scale_seq_struct_of_fp_arrays_t;

typedef struct
{
    uint8_t u8[5];
    int8_t i8[5];
    uint16_t u16[5];
    int16_t i16[5];
    uint32_t u32[5];
    int32_t i32[5];
    uint64_t u64[5];
    int64_t i64[5];
    full_scale_seq_struct_of_fp_arrays_t nested;
} full_scale_struct_of_arrays_t;

typedef struct
{
    uint8_t u8[5];
    int8_t i8[5];
    uint16_t u16[5];
    int16_t i16[5];
    uint32_t u32[5];
    int32_t i32[5];
    uint64_t u64[5];
    int64_t i64[5];
    full_scale_seq_struct_of_fp_arrays_t nested;
} full_scale_seq_struct_of_arrays_t;

typedef struct
{
    char strings[5][64];
} full_scale_seq_array_of_strings_t;

typedef struct
{
    uint8_t u8;
    int8_t i8;
    uint16_t u16;
    int16_t i16;
    uint32_t u32;
    int32_t i32;
    uint64_t u64;
    int64_t i64;
    full_scale_seq_struct_t nested;
    full_scale_seq_struct_of_arrays_t arrays;
    full_scale_seq_array_of_strings_t string_array;
} full_scale_example_t;

static sofab_decoder_t _full_scale_decoder[2];

static void _full_scale_example_struct(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    full_scale_seq_struct_t *seq = (full_scale_seq_struct_t *)usrptr;

    switch (id)
    {
        case 0: sofab_istream_read_fp32(ctx, &seq->f32); break;
        case 1: sofab_istream_read_fp64(ctx, &seq->f64); break;
        case 2: sofab_istream_read_string(ctx, seq->str, sizeof(seq->str)); break;
        case 3: sofab_istream_read_blob(ctx, seq->bytes, sizeof(seq->bytes)); break;
    }
}

static void _full_scale_example_struct_of_fp_arrays(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    full_scale_seq_struct_of_fp_arrays_t *seq = (full_scale_seq_struct_of_fp_arrays_t *)usrptr;

    switch (id)
    {
        case 0: sofab_istream_read_array_of_fp32(ctx, seq->fp32, sizeof(seq->fp32) / sizeof(seq->fp32[0])); break;
        case 1: sofab_istream_read_array_of_fp64(ctx, seq->fp64, sizeof(seq->fp64) / sizeof(seq->fp64[0])); break;
    }
}

static void _full_scale_example_struct_of_arrays(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    full_scale_seq_struct_of_arrays_t *seq = (full_scale_seq_struct_of_arrays_t *)usrptr;

    switch (id)
    {
        case 0: sofab_istream_read_array_of_u8(ctx, seq->u8, sizeof(seq->u8) / sizeof(seq->u8[0])); break;
        case 1: sofab_istream_read_array_of_i8(ctx, seq->i8, sizeof(seq->i8) / sizeof(seq->i8[0])); break;
        case 2: sofab_istream_read_array_of_u16(ctx, seq->u16, sizeof(seq->u16) / sizeof(seq->u16[0])); break;
        case 3: sofab_istream_read_array_of_i16(ctx, seq->i16, sizeof(seq->i16) / sizeof(seq->i16[0])); break;
        case 4: sofab_istream_read_array_of_u32(ctx, seq->u32, sizeof(seq->u32) / sizeof(seq->u32[0])); break;
        case 5: sofab_istream_read_array_of_i32(ctx, seq->i32, sizeof(seq->i32) / sizeof(seq->i32[0])); break;
        case 6: sofab_istream_read_array_of_u64(ctx, seq->u64, sizeof(seq->u64) / sizeof(seq->u64[0])); break;
        case 7: sofab_istream_read_array_of_i64(ctx, seq->i64, sizeof(seq->i64) / sizeof(seq->i64[0])); break;
        case 10: sofab_istream_read_sequence(ctx, &_full_scale_decoder[1], _full_scale_example_struct_of_fp_arrays, &seq->nested); break;
    }
}

static void _full_scale_example_arrays_of_strings(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    full_scale_seq_array_of_strings_t *seq = (full_scale_seq_array_of_strings_t *)usrptr;

    if (id < (sizeof(seq->strings) / sizeof(seq->strings[0])))
    {
        sofab_istream_read_string(ctx, seq->strings[id], sizeof(seq->strings[id]));
    }
}

static void _full_scale_example(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    full_scale_example_t *seq = (full_scale_example_t *)usrptr;

    switch (id)
    {
        case 0: sofab_istream_read_u8(ctx, &seq->u8); break;
        case 1: sofab_istream_read_i8(ctx, &seq->i8); break;
        case 2: sofab_istream_read_u16(ctx, &seq->u16); break;
        case 3: sofab_istream_read_i16(ctx, &seq->i16); break;
        case 4: sofab_istream_read_u32(ctx, &seq->u32); break;
        case 5: sofab_istream_read_i32(ctx, &seq->i32); break;
        case 6: sofab_istream_read_u64(ctx, &seq->u64); break;
        case 7: sofab_istream_read_i64(ctx, &seq->i64); break;
        case 10: sofab_istream_read_sequence(ctx, &_full_scale_decoder[0], _full_scale_example_struct, &seq->nested); break;
        case 100: sofab_istream_read_sequence(ctx, &_full_scale_decoder[0], _full_scale_example_struct_of_arrays, &seq->arrays); break;
        case 200: sofab_istream_read_sequence(ctx, &_full_scale_decoder[0], _full_scale_example_arrays_of_strings, &seq->string_array); break;
    }
}

static void test_read_full_scale_example (void)
{
    sofab_istream_t ctx;
    sofab_ret_t ret;
    const uint8_t buffer[] = {
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

    full_scale_example_t value;

    sofab_istream_init(&ctx, _full_scale_example, &value);
    ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);

    TEST_ASSERT_EQUAL_UINT8(200, value.u8);
    TEST_ASSERT_EQUAL_INT8(-100, value.i8);
    TEST_ASSERT_EQUAL_UINT16(50000, value.u16);
    TEST_ASSERT_EQUAL_INT16(-20000, value.i16);
    TEST_ASSERT_EQUAL_UINT32(3000000000, value.u32);
    TEST_ASSERT_EQUAL_INT32(-1000000000, value.i32);
    TEST_ASSERT_EQUAL_UINT64(10000000000000ULL, value.u64);
    TEST_ASSERT_EQUAL_INT64(-5000000000000LL, value.i64);

    {
        const uint8_t expected_bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14f, value.nested.f32);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14159265, value.nested.f64);
        TEST_ASSERT_EQUAL_STRING("Hello, World!", value.nested.str);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_bytes, value.nested.bytes, sizeof(expected_bytes));
    }

    {
        const uint8_t expected_u8[] = {0, 64, 128, 191, 255};
        const int8_t expected_i8[] = {-128, -64, 0, 63, 127};
        const uint16_t expected_u16[] = {0, 16384, 32768, 49151, 65535};
        const int16_t expected_i16[] = {-32768, -16384, 0, 16383, 32767};
        const uint32_t expected_u32[] = {0, 1073741824, 2147483648, 3221225471, 4294967295};
        const int32_t expected_i32[] = {-2147483648, -1073741824, 0, 1073741823, 2147483647};
        const uint64_t expected_u64[] = {0, 4611686018427387904ULL, 9223372036854775808ULL, 13835058055282163711ULL, 18446744073709551615ULL};
        const int64_t expected_i64[] = {-9223372036854775807LL, -4611686018427387904LL, 0LL, 4611686018427387903LL, 9223372036854775807LL};

        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_u8, value.arrays.u8, sizeof(expected_u8));
        TEST_ASSERT_EQUAL_INT8_ARRAY(expected_i8, value.arrays.i8, sizeof(expected_i8));
        TEST_ASSERT_EQUAL_UINT16_ARRAY(expected_u16, value.arrays.u16, sizeof(expected_u16) / sizeof(expected_u16[0]));
        TEST_ASSERT_EQUAL_INT16_ARRAY(expected_i16, value.arrays.i16, sizeof(expected_i16) / sizeof(expected_i16[0]));
        TEST_ASSERT_EQUAL_UINT32_ARRAY(expected_u32, value.arrays.u32, sizeof(expected_u32) / sizeof(expected_u32[0]));
        TEST_ASSERT_EQUAL_INT32_ARRAY(expected_i32, value.arrays.i32, sizeof(expected_i32) / sizeof(expected_i32[0]));
        TEST_ASSERT_EQUAL_UINT64_ARRAY(expected_u64, value.arrays.u64, sizeof(expected_u64) / sizeof(expected_u64[0]));
        TEST_ASSERT_EQUAL_INT64_ARRAY(expected_i64, value.arrays.i64, sizeof(expected_i64) / sizeof(expected_i64[0]));

        {
            const float expected_fp32[] = {+0.0, -0.0, +INFINITY, -INFINITY, NAN};
            const double expected_fp64[] = {+0.0, -0.0, +INFINITY, -INFINITY, NAN};

            TEST_ASSERT_FLOAT_ARRAY_WITHIN(0.001f, expected_fp32, value.arrays.nested.fp32, sizeof(expected_fp32) / sizeof(expected_fp32[0]));
            TEST_ASSERT_DOUBLE_ARRAY_WITHIN(0.001, expected_fp64, value.arrays.nested.fp64, sizeof(expected_fp64) / sizeof(expected_fp64[0]));
        }
    }

    {
        const char *expected_strings[] = {
            "Hello, Sofab!",
            "",
            "1234567890",
            "äöüÄÖÜß",
            "This_is_a_very_long_test_string_with_!@#$%^&*()_+-=[]{}"
        };

        for (size_t i = 0; i < (sizeof(expected_strings) / sizeof(expected_strings[0])); i++)
        {
            TEST_ASSERT_EQUAL_STRING(expected_strings[i], value.string_array.strings[i]);
        }
    }
}

int test_istream_main (void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_feed_buffer);
    RUN_TEST(test_feed_buffer_stream);
    RUN_TEST(test_usage_invalid_field_type);
    RUN_TEST(test_usage_invalid_field_type_fixlen);
    RUN_TEST(test_usage_invalid_field_type_array);
    RUN_TEST(test_usage_invalid_target_len_varint_unsigned);
    RUN_TEST(test_usage_invalid_target_len_varint_signed);
    RUN_TEST(test_read_nothing);

    RUN_TEST(test_id_max);

    RUN_TEST(test_msg_invalid_id_overflow);
    RUN_TEST(test_msg_invalid_varint_unsigned_varint_overflow);
    RUN_TEST(test_msg_invalid_varint_signed_varint_overflow);
    RUN_TEST(test_msg_invalid_fixlen_length_varint_overflow);
    RUN_TEST(test_msg_invalid_fixlen_length_limit_overflow);
    RUN_TEST(test_msg_invalid_array_count_varint_overflow);
    RUN_TEST(test_msg_invalid_array_count_limit_overflow);
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
    RUN_TEST(test_read_string_empty);
    RUN_TEST(test_read_blob);
    RUN_TEST(test_read_blob_empty);

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

    RUN_TEST(test_read_full_scale_example);

    return UNITY_END();
}
