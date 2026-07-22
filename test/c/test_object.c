/*!
 * @file test_object.c
 * @brief SofaBuffers test for object C API
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/object.h"

#include "unity.h"

#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

extern void hexdump2array(const void *data, size_t len);

/*****************************************************************************/
/* tests */
/*****************************************************************************/
typedef struct
{
    double fp64;
    float fp32;
    char str[32];
    uint8_t bytes[4];
    uint32_t unused;
} fullscale_message_seq_struct_t;

const sofab_object_descr_field_t _info_fields_fullscale_message_seq_struct[] =
{
    SOFAB_OBJECT_FIELD(1, fullscale_message_seq_struct_t, fp64, SOFAB_OBJECT_FIELDTYPE_FP64),
    SOFAB_OBJECT_FIELD(0, fullscale_message_seq_struct_t, fp32, SOFAB_OBJECT_FIELDTYPE_FP32),
    SOFAB_OBJECT_FIELD(2, fullscale_message_seq_struct_t, str, SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(3, fullscale_message_seq_struct_t, bytes, SOFAB_OBJECT_FIELDTYPE_BLOB),
    SOFAB_OBJECT_FIELD(4, fullscale_message_seq_struct_t, unused, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};

const fullscale_message_seq_struct_t _info_defaults_fullscale_message_seq_struct =
{
    .fp64 = 0.0,
    .fp32 = 0.0f,
    .str = {0},
    .bytes = {0},
    .unused = 1234,
};

const sofab_object_descr_t _info_fullscale_message_seq_struct =
    SOFAB_OBJECT_DESCR_WITH_DEFAULTS(
        _info_fields_fullscale_message_seq_struct,
        5,
        NULL,
        0,
        &_info_defaults_fullscale_message_seq_struct
    );

//

typedef struct
{
    double fp64[5];
    float fp32[5];
} fullscale_message_seq_struct_of_fp_arrays_t;

const sofab_object_descr_field_t _info_fields_fullscale_message_seq_struct_of_fp_arrays[] =
{
    SOFAB_OBJECT_FIELD_ARRAY(1, fullscale_message_seq_struct_of_fp_arrays_t, fp64, SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64),
    SOFAB_OBJECT_FIELD_ARRAY(0, fullscale_message_seq_struct_of_fp_arrays_t, fp32, SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32),
};

const sofab_object_descr_t _info_fullscale_message_seq_struct_of_fp_arrays =
    SOFAB_OBJECT_DESCR(
        _info_fields_fullscale_message_seq_struct_of_fp_arrays,
        2,
        NULL,
        0
    );

//

typedef struct
{
    fullscale_message_seq_struct_of_fp_arrays_t nested;
    uint64_t u64[5];
    int64_t i64[5];
    uint32_t u32[5];
    int32_t i32[5];
    uint16_t u16[5];
    int16_t i16[5];
    uint8_t u8[5];
    int8_t i8[5];
} fullscale_message_struct_of_arrays_t;

const sofab_object_descr_field_t _info_fields_fullscale_message_struct_of_arrays[] =
{
    SOFAB_OBJECT_FIELD_SEQUENCE(8, fullscale_message_struct_of_arrays_t, nested, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_ARRAY(6, fullscale_message_struct_of_arrays_t, u64, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY(7, fullscale_message_struct_of_arrays_t, i64, SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED),
    SOFAB_OBJECT_FIELD_ARRAY(4, fullscale_message_struct_of_arrays_t, u32, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY(5, fullscale_message_struct_of_arrays_t, i32, SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED),
    SOFAB_OBJECT_FIELD_ARRAY(2, fullscale_message_struct_of_arrays_t, u16, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY(3, fullscale_message_struct_of_arrays_t, i16, SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED),
    SOFAB_OBJECT_FIELD_ARRAY(0, fullscale_message_struct_of_arrays_t, u8, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY(1, fullscale_message_struct_of_arrays_t, i8, SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED),
};

const sofab_object_descr_t *const _info_nested_fullscale_message_struct_of_arrays[] =
{
    &_info_fullscale_message_seq_struct_of_fp_arrays,
};

const sofab_object_descr_t _info_struct_of_arrays =
    SOFAB_OBJECT_DESCR(
        _info_fields_fullscale_message_struct_of_arrays,
        9,
        _info_nested_fullscale_message_struct_of_arrays,
        1
    );

//

typedef struct
{
    char strings[5][64];
} fullscale_message_seq_array_of_strings_t;

const sofab_object_descr_field_t _info_fields_fullscale_message_seq_array_of_strings[] =
{
    SOFAB_OBJECT_FIELD(0, fullscale_message_seq_array_of_strings_t, strings[0], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(1, fullscale_message_seq_array_of_strings_t, strings[1], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(2, fullscale_message_seq_array_of_strings_t, strings[2], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(3, fullscale_message_seq_array_of_strings_t, strings[3], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(4, fullscale_message_seq_array_of_strings_t, strings[4], SOFAB_OBJECT_FIELDTYPE_STRING),
};

const sofab_object_descr_t _info_fullscale_message_seq_array_of_strings =
    SOFAB_OBJECT_DESCR(
        _info_fields_fullscale_message_seq_array_of_strings,
        5,
        NULL,
        0
    );

//

typedef struct
{
    fullscale_message_seq_struct_t nested;
    fullscale_message_struct_of_arrays_t arrays;
    fullscale_message_seq_array_of_strings_t string_array;
    uint64_t u64;
    int64_t i64;
    uint32_t u32;
    int32_t i32;
    uint16_t u16;
    int16_t i16;
    uint8_t u8;
    int8_t i8;
} fullscale_message_t;

const sofab_object_descr_field_t _info_fields_fullscale_message[] =
{
    SOFAB_OBJECT_FIELD_SEQUENCE(10, fullscale_message_t, nested, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(100, fullscale_message_t, arrays, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 1),
    SOFAB_OBJECT_FIELD_SEQUENCE(200, fullscale_message_t, string_array, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 2),
    SOFAB_OBJECT_FIELD(6, fullscale_message_t, u64, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(7, fullscale_message_t, i64, SOFAB_OBJECT_FIELDTYPE_SIGNED),
    SOFAB_OBJECT_FIELD(4, fullscale_message_t, u32, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(5, fullscale_message_t, i32, SOFAB_OBJECT_FIELDTYPE_SIGNED),
    SOFAB_OBJECT_FIELD(2, fullscale_message_t, u16, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(3, fullscale_message_t, i16, SOFAB_OBJECT_FIELDTYPE_SIGNED),
    SOFAB_OBJECT_FIELD(0, fullscale_message_t, u8, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(11, fullscale_message_t, i8, SOFAB_OBJECT_FIELDTYPE_SIGNED),
};

const sofab_object_descr_t *const _info_nested_fullscale_message[] =
{
    &_info_fullscale_message_seq_struct,
    &_info_struct_of_arrays,
    &_info_fullscale_message_seq_array_of_strings
};

const sofab_object_descr_t _info_fullscale_message =
    SOFAB_OBJECT_DESCR(
        _info_fields_fullscale_message,
        11,
        _info_nested_fullscale_message,
        3
    );

//

static void test_object_serialize (void)
{
    fullscale_message_t data;
    memset(&data, 0, sizeof(data));

    // Fill scalar fields
    data.u8 = 200;
    data.i8 = -100;
    data.u16 = 50000;
    data.i16 = -20000;
    data.u32 = 3000000000U;
    data.i32 = -1000000000;
    data.u64 = 10000000000000ULL;
    data.i64 = -5000000000000LL;

    // Fill nested struct (sequence)
    data.nested.fp32 = 3.14f;
    data.nested.fp64 = 3.14159265;
    strncpy(data.nested.str, "Hello, World!", sizeof(data.nested.str));
    memcpy(data.nested.bytes, (const uint8_t[]){0xDE, 0xAD, 0xBE, 0xEF}, 4);
    data.nested.unused = 1234; // This field should be serialized due to default value

    // Fill arrays struct
    const uint8_t u8_vals[5] = {0, 64, 128, 191, 255};
    memcpy(data.arrays.u8, u8_vals, sizeof(u8_vals));

    const int8_t i8_vals[5] = {-128, -64, 0, 63, 127};
    memcpy(data.arrays.i8, i8_vals, sizeof(i8_vals));

    const uint16_t u16_vals[5] = {0, 16384, 32768, 49151, 65535};
    memcpy(data.arrays.u16, u16_vals, sizeof(u16_vals));

    const int16_t i16_vals[5] = {-32768, -16384, 0, 16383, 32767};
    memcpy(data.arrays.i16, i16_vals, sizeof(i16_vals));

    const uint32_t u32_vals[5] = {0U, 1073741824U, 2147483648U, 3221225471U, 4294967295U};
    memcpy(data.arrays.u32, u32_vals, sizeof(u32_vals));

    const int32_t i32_vals[5] = {-2147483648, -1073741824, 0, 1073741823, 2147483647};
    memcpy(data.arrays.i32, i32_vals, sizeof(i32_vals));

    const uint64_t u64_vals[5] = {
        0ULL,
        4611686018427387904ULL,
        9223372036854775808ULL,
        13835058055282163711ULL,
        18446744073709551615ULL
    };
    memcpy(data.arrays.u64, u64_vals, sizeof(u64_vals));

    const int64_t i64_vals[5] = {
        -9223372036854775807LL,
        -4611686018427387904LL,
        0LL,
        4611686018427387903LL,
        9223372036854775807LL
    };
    memcpy(data.arrays.i64, i64_vals, sizeof(i64_vals));

    // Fill nested struct of arrays (sequence)
    const float fp32_vals[5] = {1.0f, 2.0f, 3.0f, -FLT_MAX, FLT_MAX};
    memcpy(data.arrays.nested.fp32, fp32_vals, sizeof(fp32_vals));

    const double fp64_vals[5] = {1.0, 2.0, 3.0, -DBL_MAX, DBL_MAX};
    memcpy(data.arrays.nested.fp64, fp64_vals, sizeof(fp64_vals));

    // Fill array of strings
    strncpy(data.string_array.strings[0], "Hello, Sofab!", sizeof(data.string_array.strings[0]));
    data.string_array.strings[0][sizeof(data.string_array.strings[0])-1] = '\0';

    data.string_array.strings[1][0] = '\0';

    strncpy(data.string_array.strings[2], "1234567890", sizeof(data.string_array.strings[2]));
    data.string_array.strings[2][sizeof(data.string_array.strings[2])-1] = '\0';

    strncpy(data.string_array.strings[3], "äöüÄÖÜß", sizeof(data.string_array.strings[3]));
    data.string_array.strings[3][sizeof(data.string_array.strings[3])-1] = '\0';

    strncpy(data.string_array.strings[4], "This_is_a_very_long_test_string_with_!@#$%^&*()_+-=[]{}", sizeof(data.string_array.strings[4]));
    data.string_array.strings[4][sizeof(data.string_array.strings[4])-1] = '\0';

    sofab_ostream_t ctx;
    uint8_t buffer[512];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ret_t ret = sofab_object_encode(&ctx, &_info_fullscale_message, &data);
    size_t used = sofab_ostream_flush(&ctx);

    const uint8_t expected[] = {
        0x56, 0x0A, 0x41, 0xF1, 0xD4, 0xC8, 0x53, 0xFB, 0x21, 0x09, 0x40, 0x02,
        0x20, 0xC3, 0xF5, 0x48, 0x40, 0x12, 0x6A, 0x48, 0x65, 0x6C, 0x6C, 0x6F,
        0x2C, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x21, 0x1A, 0x23, 0xDE, 0xAD,
        0xBE, 0xEF, 0x07, 0xA6, 0x06, 0x46, 0x0D, 0x05, 0x41, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF,
        0x7F, 0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F,
        0x07, 0x33, 0x05, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        0x40, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0x01, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x3C, 0x05, 0xFD, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
        0x23, 0x05, 0x00, 0x80, 0x80, 0x80, 0x80, 0x04, 0x80, 0x80, 0x80, 0x80,
        0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x2C,
        0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00,
        0xFE, 0xFF, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F, 0x13, 0x05,
        0x00, 0x80, 0x80, 0x01, 0x80, 0x80, 0x02, 0xFF, 0xFF, 0x02, 0xFF, 0xFF,
        0x03, 0x1C, 0x05, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0x01, 0x00, 0xFE, 0xFF,
        0x01, 0xFE, 0xFF, 0x03, 0x03, 0x05, 0x00, 0x40, 0x80, 0x01, 0xBF, 0x01,
        0xFF, 0x01, 0x0C, 0x05, 0xFF, 0x01, 0x7F, 0x00, 0x7E, 0xFE, 0x01, 0x07,
        0xC6, 0x0C, 0x02, 0x6A, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x53,
        0x6F, 0x66, 0x61, 0x62, 0x21, 0x12, 0x52, 0x31, 0x32, 0x33, 0x34, 0x35,
        0x36, 0x37, 0x38, 0x39, 0x30, 0x1A, 0x72, 0xC3, 0xA4, 0xC3, 0xB6, 0xC3,
        0xBC, 0xC3, 0x84, 0xC3, 0x96, 0xC3, 0x9C, 0xC3, 0x9F, 0x22, 0xBA, 0x03,
        0x54, 0x68, 0x69, 0x73, 0x5F, 0x69, 0x73, 0x5F, 0x61, 0x5F, 0x76, 0x65,
        0x72, 0x79, 0x5F, 0x6C, 0x6F, 0x6E, 0x67, 0x5F, 0x74, 0x65, 0x73, 0x74,
        0x5F, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x77, 0x69, 0x74, 0x68,
        0x5F, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F,
        0x2B, 0x2D, 0x3D, 0x5B, 0x5D, 0x7B, 0x7D, 0x07, 0x30, 0x80, 0xC0, 0xCA,
        0xF3, 0x84, 0xA3, 0x02, 0x39, 0xFF, 0xBF, 0xCA, 0xF3, 0x84, 0xA3, 0x02,
        0x20, 0x80, 0xBC, 0xC1, 0x96, 0x0B, 0x29, 0xFF, 0xA7, 0xD6, 0xB9, 0x07,
        0x10, 0xD0, 0x86, 0x03, 0x19, 0xBF, 0xB8, 0x02, 0x00, 0xC8, 0x01, 0x59,
        0xC7, 0x01
    };

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "used != sizeof(expected)");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used, "buffer != expected");
}

//

typedef struct
{
    sofab_istream_t ctx;
    sofab_object_decoder_t decoder[3];
} fullscale_message_decoder_t;

void fullscale_message_decoder_init (
    fullscale_message_decoder_t *decoder,
    fullscale_message_t *msg)
{
    sofab_object_decoder_t *dec = &decoder->decoder[0];
    memset(decoder, 0, sizeof(*decoder));

    dec->info = &_info_fullscale_message;
    dec->dst = (uint8_t *)msg;
    dec->depth = sizeof(decoder->decoder) / sizeof(decoder->decoder[0]) - 1;

    sofab_istream_init(&decoder->ctx, sofab_object_field_cb, (void*)dec);
}

static void test_object_deserialize (void)
{
    const uint8_t buffer[] = {
        0x56, 0x0A, 0x41, 0xF1, 0xD4, 0xC8, 0x53, 0xFB, 0x21, 0x09, 0x40, 0x02,
        0x20, 0xC3, 0xF5, 0x48, 0x40, 0x12, 0x6A, 0x48, 0x65, 0x6C, 0x6C, 0x6F,
        0x2C, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x21, 0x1A, 0x23, 0xDE, 0xAD,
        0xBE, 0xEF, 0x07, 0xA6, 0x06, 0x46, 0x0D, 0x05, 0x41, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF,
        0x7F, 0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F,
        0x07, 0x33, 0x05, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        0x40, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0x01, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x3C, 0x05, 0xFD, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
        0x23, 0x05, 0x00, 0x80, 0x80, 0x80, 0x80, 0x04, 0x80, 0x80, 0x80, 0x80,
        0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x2C,
        0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00,
        0xFE, 0xFF, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F, 0x13, 0x05,
        0x00, 0x80, 0x80, 0x01, 0x80, 0x80, 0x02, 0xFF, 0xFF, 0x02, 0xFF, 0xFF,
        0x03, 0x1C, 0x05, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0x01, 0x00, 0xFE, 0xFF,
        0x01, 0xFE, 0xFF, 0x03, 0x03, 0x05, 0x00, 0x40, 0x80, 0x01, 0xBF, 0x01,
        0xFF, 0x01, 0x0C, 0x05, 0xFF, 0x01, 0x7F, 0x00, 0x7E, 0xFE, 0x01, 0x07,
        0xC6, 0x0C, 0x02, 0x6A, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x53,
        0x6F, 0x66, 0x61, 0x62, 0x21, 0x12, 0x52, 0x31, 0x32, 0x33, 0x34, 0x35,
        0x36, 0x37, 0x38, 0x39, 0x30, 0x1A, 0x72, 0xC3, 0xA4, 0xC3, 0xB6, 0xC3,
        0xBC, 0xC3, 0x84, 0xC3, 0x96, 0xC3, 0x9C, 0xC3, 0x9F, 0x22, 0xBA, 0x03,
        0x54, 0x68, 0x69, 0x73, 0x5F, 0x69, 0x73, 0x5F, 0x61, 0x5F, 0x76, 0x65,
        0x72, 0x79, 0x5F, 0x6C, 0x6F, 0x6E, 0x67, 0x5F, 0x74, 0x65, 0x73, 0x74,
        0x5F, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x77, 0x69, 0x74, 0x68,
        0x5F, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F,
        0x2B, 0x2D, 0x3D, 0x5B, 0x5D, 0x7B, 0x7D, 0x07, 0x30, 0x80, 0xC0, 0xCA,
        0xF3, 0x84, 0xA3, 0x02, 0x39, 0xFF, 0xBF, 0xCA, 0xF3, 0x84, 0xA3, 0x02,
        0x20, 0x80, 0xBC, 0xC1, 0x96, 0x0B, 0x29, 0xFF, 0xA7, 0xD6, 0xB9, 0x07,
        0x10, 0xD0, 0x86, 0x03, 0x19, 0xBF, 0xB8, 0x02, 0x00, 0xC8, 0x01, 0x59,
        0xC7, 0x01
    };

    fullscale_message_t data;
    memset(&data, 0x55, sizeof(data));
    sofab_object_init(&_info_fullscale_message, &data);

    fullscale_message_decoder_t decoder;
    fullscale_message_decoder_init(&decoder, &data);
    sofab_ret_t ret = sofab_istream_feed(&decoder.ctx, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
    TEST_ASSERT_EQUAL_UINT8(200, data.u8);
    TEST_ASSERT_EQUAL_INT8(-100, data.i8);
    TEST_ASSERT_EQUAL_UINT16(50000, data.u16);
    TEST_ASSERT_EQUAL_INT16(-20000, data.i16);
    TEST_ASSERT_EQUAL_UINT32(3000000000U, data.u32);
    TEST_ASSERT_EQUAL_INT32(-1000000000, data.i32);
    TEST_ASSERT_EQUAL_UINT64(10000000000000ULL, data.u64);
    TEST_ASSERT_EQUAL_INT64(-5000000000000LL, data.i64);

    TEST_ASSERT_EQUAL_FLOAT(3.14f, data.nested.fp32);
    TEST_ASSERT_EQUAL_DOUBLE(3.14159265, data.nested.fp64);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", data.nested.str);
    const uint8_t expected_bytes[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_bytes, data.nested.bytes, 4);
    TEST_ASSERT_EQUAL_UINT32(1234, data.nested.unused); // Should be default value

    const uint8_t expected_u8[5] = {0, 64, 128, 191, 255};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_u8, data.arrays.u8, 5);

    const int8_t expected_i8[5] = {-128, -64, 0, 63, 127};
    TEST_ASSERT_EQUAL_INT8_ARRAY(expected_i8, data.arrays.i8, 5);

    const uint16_t expected_u16[5] = {0, 16384, 32768, 49151, 65535};
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected_u16, data.arrays.u16, 5);

    const int16_t expected_i16[5] = {-32768, -16384, 0, 16383, 32767};
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected_i16, data.arrays.i16, 5);

    const uint32_t expected_u32[5] = {0U, 1073741824U, 2147483648U, 3221225471U, 4294967295U};
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expected_u32, data.arrays.u32, 5);

    const int32_t expected_i32[5] = {-2147483648, -1073741824, 0, 1073741823, 2147483647};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected_i32, data.arrays.i32, 5);

    const uint64_t expected_u64[5] = {
        0ULL,
        4611686018427387904ULL,
        9223372036854775808ULL,
        13835058055282163711ULL,
        18446744073709551615ULL
    };
    TEST_ASSERT_EQUAL_UINT64_ARRAY(expected_u64, data.arrays.u64, 5);

    const int64_t expected_i64[5] = {
        -9223372036854775807LL,
        -4611686018427387904LL,
        0LL,
        4611686018427387903LL,
        9223372036854775807LL
    };
    TEST_ASSERT_EQUAL_INT64_ARRAY(expected_i64, data.arrays.i64, 5);

    const float expected_fp32[5] = {1.0f, 2.0f, 3.0f, -FLT_MAX, FLT_MAX};
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected_fp32, data.arrays.nested.fp32, 5);

    const double expected_fp64[5] = {1.0, 2.0, 3.0, -DBL_MAX, DBL_MAX};
    TEST_ASSERT_EQUAL_DOUBLE_ARRAY(expected_fp64, data.arrays.nested.fp64, 5);

    TEST_ASSERT_EQUAL_STRING("Hello, Sofab!", data.string_array.strings[0]);
    TEST_ASSERT_EQUAL_STRING("", data.string_array.strings[1]);
    TEST_ASSERT_EQUAL_STRING("1234567890", data.string_array.strings[2]);
    TEST_ASSERT_EQUAL_STRING("äöüÄÖÜß", data.string_array.strings[3]);
    TEST_ASSERT_EQUAL_STRING("This_is_a_very_long_test_string_with_!@#$%^&*()_+-=[]{}", data.string_array.strings[4]);
}

//

typedef struct
{
    uint32_t u32;
} invalid_unsigned_t;

const sofab_object_descr_field_t _info_fields_invalid_unsigned[] =
{
    {1, 0, 3 /* invalid */, 0, SOFAB_OBJECT_FIELDTYPE_UNSIGNED, 3 /* invalid */},
};

const sofab_object_descr_t _info_invalid_unsigned =
    SOFAB_OBJECT_DESCR(
        _info_fields_invalid_unsigned,
        1,
        NULL,
        0
    );

static void test_object_serialize_invalid_unsigned_size (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[16];

    invalid_unsigned_t data;
    data.u32 = 0x55AA55AA;

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ret_t ret = sofab_object_encode(&ctx, &_info_invalid_unsigned, &data);

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_USAGE, "ret != SOFAB_RET_E_USAGE");
}

//

typedef struct
{
    int32_t i32;
} invalid_signed_t;

const sofab_object_descr_field_t _info_fields_invalid_signed[] =
{
    {1, 0, 3 /* invalid */, 0, SOFAB_OBJECT_FIELDTYPE_SIGNED, 3 /* invalid */},
};

const sofab_object_descr_t _info_invalid_signed =
    SOFAB_OBJECT_DESCR(
        _info_fields_invalid_signed,
        1,
        NULL,
        0
    );


static void test_object_serialize_invalid_signed_size (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[16];

    invalid_signed_t data;
    data.i32 = 0x55AA55AA;

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ret_t ret = sofab_object_encode(&ctx, &_info_invalid_signed, &data);

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_USAGE, "ret != SOFAB_RET_E_USAGE");
}

//

typedef struct
{
    int32_t i32;
} invalid_field_type_t;

const sofab_object_descr_field_t _info_fields_invalid_field_type[] =
{
    {1, 0, 4, 0, 0xF /* invalid */, 4},
};

const sofab_object_descr_t _info_invalid_field_type =
    SOFAB_OBJECT_DESCR(
        _info_fields_invalid_field_type,
        1,
        NULL,
        0
    );

static void test_object_serialize_invalid_field_type (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[16];

    invalid_field_type_t data;
    data.i32 = 0x55AA55AA;

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ret_t ret = sofab_object_encode(&ctx, &_info_invalid_field_type, &data);

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_USAGE, "ret != SOFAB_RET_E_USAGE");
}

//

void fullscale_message_decoder_init_invalid_nested_depth (
    fullscale_message_decoder_t *decoder,
    fullscale_message_t *msg)
{
    sofab_object_decoder_t *dec = &decoder->decoder[0];
    memset(decoder, 0, sizeof(*decoder));

    dec->info = &_info_fullscale_message;
    dec->dst = (uint8_t *)msg;
    dec->depth = 1; /* decoder depth is too small for the fullscale message */

    sofab_istream_init(&decoder->ctx, sofab_object_field_cb, (void*)dec);
}

static void test_object_deserialize_invalid_nested_depth (void)
{
    const uint8_t buffer[] = {
        0x56, 0x0A, 0x41, 0xF1, 0xD4, 0xC8, 0x53, 0xFB, 0x21, 0x09, 0x40, 0x02,
        0x20, 0xC3, 0xF5, 0x48, 0x40, 0x12, 0x6A, 0x48, 0x65, 0x6C, 0x6C, 0x6F,
        0x2C, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x21, 0x1A, 0x23, 0xDE, 0xAD,
        0xBE, 0xEF, 0x07, 0xA6, 0x06, 0x46, 0x0D, 0x05, 0x41, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF,
        0x7F, 0x05, 0x05, 0x20, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x40, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F,
        0x07, 0x33, 0x05, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        0x40, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0x01, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x3C, 0x05, 0xFD, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
        0x23, 0x05, 0x00, 0x80, 0x80, 0x80, 0x80, 0x04, 0x80, 0x80, 0x80, 0x80,
        0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x2C,
        0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00,
        0xFE, 0xFF, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F, 0x13, 0x05,
        0x00, 0x80, 0x80, 0x01, 0x80, 0x80, 0x02, 0xFF, 0xFF, 0x02, 0xFF, 0xFF,
        0x03, 0x1C, 0x05, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0x01, 0x00, 0xFE, 0xFF,
        0x01, 0xFE, 0xFF, 0x03, 0x03, 0x05, 0x00, 0x40, 0x80, 0x01, 0xBF, 0x01,
        0xFF, 0x01, 0x0C, 0x05, 0xFF, 0x01, 0x7F, 0x00, 0x7E, 0xFE, 0x01, 0x07,
        0xC6, 0x0C, 0x02, 0x6A, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x53,
        0x6F, 0x66, 0x61, 0x62, 0x21, 0x0A, 0x02, 0x12, 0x52, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x1A, 0x72, 0xC3, 0xA4, 0xC3,
        0xB6, 0xC3, 0xBC, 0xC3, 0x84, 0xC3, 0x96, 0xC3, 0x9C, 0xC3, 0x9F, 0x22,
        0xBA, 0x03, 0x54, 0x68, 0x69, 0x73, 0x5F, 0x69, 0x73, 0x5F, 0x61, 0x5F,
        0x76, 0x65, 0x72, 0x79, 0x5F, 0x6C, 0x6F, 0x6E, 0x67, 0x5F, 0x74, 0x65,
        0x73, 0x74, 0x5F, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x77, 0x69,
        0x74, 0x68, 0x5F, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28,
        0x29, 0x5F, 0x2B, 0x2D, 0x3D, 0x5B, 0x5D, 0x7B, 0x7D, 0x07, 0x30, 0x80,
        0xC0, 0xCA, 0xF3, 0x84, 0xA3, 0x02, 0x39, 0xFF, 0xBF, 0xCA, 0xF3, 0x84,
        0xA3, 0x02, 0x20, 0x80, 0xBC, 0xC1, 0x96, 0x0B, 0x29, 0xFF, 0xA7, 0xD6,
        0xB9, 0x07, 0x10, 0xD0, 0x86, 0x03, 0x19, 0xBF, 0xB8, 0x02, 0x00, 0xC8,
        0x01, 0x59, 0xC7, 0x01
    };

    fullscale_message_t data;
    memset(&data, 0x55, sizeof(data));

    fullscale_message_decoder_t decoder;
    fullscale_message_decoder_init_invalid_nested_depth(&decoder, &data);
    sofab_ret_t ret = sofab_istream_feed(&decoder.ctx, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
}

//

static void test_object_deserialize_invalid_field_type (void)
{
    const uint8_t buffer[] = {0x09, 0xFE, 0x01};

    invalid_field_type_t data;
    memset(&data, 0x55, sizeof(data));

    sofab_istream_t ctx;
    sofab_object_decoder_t decoder = {
        .info = &_info_invalid_field_type,
        .dst = (uint8_t *)&data,
        .depth = 0,
    };

    sofab_istream_init(&ctx, sofab_object_field_cb, (void*)&decoder);
    sofab_ret_t ret = sofab_istream_feed(&ctx, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_OK, "ret != SOFAB_RET_OK");
}

//

/*
 * Regression test: consecutive SEQUENCE fields must each select their own nested
 * descriptor (keyed off the static field->nested_idx), even when a preceding
 * sequence is empty.
 *
 * A SEQUENCE is always framed and recursed into -- only its leaf fields are
 * skipped when default, the wrapper itself is not -- so seq_a below is emitted as
 * an EMPTY wrapper. For each SEQUENCE, encoder and decoder must use that field's
 * own nested_idx; a naive running counter over emitted sequences would pick the
 * wrong nested_list entry and corrupt the stream.
 *
 * Layout: seq_a (nested_idx 0) is all-default -> emitted empty; seq_b
 * (nested_idx 1) carries data. seq_a and seq_b have deliberately different field
 * layouts so a wrong-descriptor encode is detectable. The round-trip must
 * preserve seq_b.
 */
typedef struct
{
    uint32_t x;
} regr_seq_a_t;

static const sofab_object_descr_field_t _info_fields_regr_seq_a[] =
{
    SOFAB_OBJECT_FIELD(0, regr_seq_a_t, x, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};

static const sofab_object_descr_t _info_regr_seq_a =
    SOFAB_OBJECT_DESCR(_info_fields_regr_seq_a, 1, NULL, 0);

typedef struct
{
    char s[16];
    uint32_t y;
} regr_seq_b_t;

static const sofab_object_descr_field_t _info_fields_regr_seq_b[] =
{
    SOFAB_OBJECT_FIELD(0, regr_seq_b_t, s, SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(1, regr_seq_b_t, y, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};

static const sofab_object_descr_t _info_regr_seq_b =
    SOFAB_OBJECT_DESCR(_info_fields_regr_seq_b, 2, NULL, 0);

typedef struct
{
    regr_seq_a_t a; /* sequence, all-default -> emitted as an empty wrapper */
    regr_seq_b_t b; /* sequence, carries data */
} regr_msg_t;

static const sofab_object_descr_field_t _info_fields_regr_msg[] =
{
    SOFAB_OBJECT_FIELD_SEQUENCE(0, regr_msg_t, a, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(1, regr_msg_t, b, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 1),
};

static const sofab_object_descr_t *const _info_nested_regr_msg[] =
{
    &_info_regr_seq_a,
    &_info_regr_seq_b,
};

static const sofab_object_descr_t _info_regr_msg =
    SOFAB_OBJECT_DESCR(_info_fields_regr_msg, 2, _info_nested_regr_msg, 2);

typedef struct
{
    sofab_istream_t ctx;
    sofab_object_decoder_t decoder[2];
} regr_msg_decoder_t;

static void test_object_roundtrip_empty_sequence_before_sequence (void)
{
    regr_msg_t in;
    memset(&in, 0, sizeof(in));
    /* leave `a` all-default -> emitted as an empty wrapper; only `b` carries data */
    strncpy(in.b.s, "hello", sizeof(in.b.s));
    in.b.y = 0xABCD;

    sofab_ostream_t octx;
    uint8_t buffer[128];
    sofab_ostream_init(&octx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ret_t enc = sofab_object_encode(&octx, &_info_regr_msg, &in);
    size_t used = sofab_ostream_flush(&octx);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, enc, "encode failed");

    regr_msg_t out;
    memset(&out, 0x55, sizeof(out));
    sofab_object_init(&_info_regr_msg, &out);

    regr_msg_decoder_t dec;
    memset(&dec, 0, sizeof(dec));
    dec.decoder[0].info = &_info_regr_msg;
    dec.decoder[0].dst = (uint8_t *)&out;
    dec.decoder[0].depth = sizeof(dec.decoder) / sizeof(dec.decoder[0]) - 1;
    sofab_istream_init(&dec.ctx, sofab_object_field_cb, (void *)&dec.decoder[0]);

    sofab_ret_t r = sofab_istream_feed(&dec.ctx, buffer, used);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, r, "decode of empty-sequence-before-sequence failed");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("hello", out.b.s, "seq_b.s not preserved");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0xABCD, out.b.y, "seq_b.y not preserved");
}

//

/*
 * A nested object (SEQUENCE) whose fields are all at their default is emitted as
 * an EMPTY wrapper sequence, never dropped: presence is decided per inner field,
 * so the framing is always written and the wrapper is simply empty. Previously a
 * whole-object memcmp/_iszero could drop the sequence entirely, which diverges
 * from the per-field model and is sensitive to struct padding.
 */
typedef struct
{
    uint32_t x;
    uint32_t y;
} defseq_inner_t;

static const sofab_object_descr_field_t _info_fields_defseq_inner[] =
{
    SOFAB_OBJECT_FIELD(0, defseq_inner_t, x, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, defseq_inner_t, y, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};

static const sofab_object_descr_t _info_defseq_inner =
    SOFAB_OBJECT_DESCR(_info_fields_defseq_inner, 2, NULL, 0);

typedef struct
{
    defseq_inner_t inner;
} defseq_msg_t;

static const sofab_object_descr_field_t _info_fields_defseq_msg[] =
{
    SOFAB_OBJECT_FIELD_SEQUENCE(7, defseq_msg_t, inner, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};

static const sofab_object_descr_t *const _info_nested_defseq_msg[] =
{
    &_info_defseq_inner,
};

static const sofab_object_descr_t _info_defseq_msg =
    SOFAB_OBJECT_DESCR(_info_fields_defseq_msg, 1, _info_nested_defseq_msg, 1);

static void test_object_default_sequence_emitted_empty (void)
{
    defseq_msg_t in;
    memset(&in, 0, sizeof(in)); /* inner all-default */

    sofab_ostream_t octx;
    uint8_t buffer[32];
    sofab_ostream_init(&octx, buffer, sizeof(buffer), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, &_info_defseq_msg, &in), "encode failed");
    size_t used = sofab_ostream_flush(&octx);

    /*
     * Emitted as an empty wrapper sequence for field id 7 -- NOT dropped:
     * sequence_begin(7) = (7<<3)|0b110 = 0x3e, sequence_end = 0b111 = 0x07.
     */
    static const uint8_t expected[] = { 0x3e, 0x07 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "default nested sequence must be emitted as an empty wrapper, not dropped");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used,
        "default nested sequence bytes mismatch");
}

//

/*
 * A STRING field is omitted when its logical, null-terminated value equals its
 * default -- compared by content, not by raw buffer bytes. The bytes past the
 * terminator are indeterminate (here a shorter string left behind the tail of a
 * longer one); a whole-buffer memcmp would wrongly serialise a logically-default
 * string. Covers both the default-image path (strncmp vs the default) and the
 * zero-baseline path (empty string == implicit default).
 */
typedef struct
{
    char label[8];
} strdef_t;

static const sofab_object_descr_field_t _info_fields_strdef[] =
{
    SOFAB_OBJECT_FIELD(0, strdef_t, label, SOFAB_OBJECT_FIELDTYPE_STRING),
};

static const strdef_t _info_defaults_strdef = { .label = "hi" };

static const sofab_object_descr_t _info_strdef_with_default =
    SOFAB_OBJECT_DESCR_WITH_DEFAULTS(_info_fields_strdef, 1, NULL, 0, &_info_defaults_strdef);

static const sofab_object_descr_t _info_strdef_zero =
    SOFAB_OBJECT_DESCR(_info_fields_strdef, 1, NULL, 0);

static size_t strdef_encode (const sofab_object_descr_t *info, const strdef_t *in)
{
    sofab_ostream_t octx;
    uint8_t buffer[32];
    sofab_ostream_init(&octx, buffer, sizeof(buffer), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, info, in), "encode failed");
    return sofab_ostream_flush(&octx);
}

static void test_object_string_default_omission (void)
{
    strdef_t in;

    /* default image: value equal to the default is omitted ... */
    memset(&in, 0, sizeof(in));
    strcpy(in.label, "hi");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, strdef_encode(&_info_strdef_with_default, &in),
        "string equal to default must be omitted");

    /* ... even when a longer prior value left indeterminate tail bytes ... */
    memset(&in, 0, sizeof(in));
    strcpy(in.label, "hello");
    strcpy(in.label, "hi"); /* "hi\0lo\0..": logical "hi" == default */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, strdef_encode(&_info_strdef_with_default, &in),
        "logically-default string with dirty tail must be omitted");

    /* ... but a genuinely different value is emitted. */
    memset(&in, 0, sizeof(in));
    strcpy(in.label, "yo");
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(0, strdef_encode(&_info_strdef_with_default, &in),
        "non-default string must be emitted");

    /* zero baseline (no default image): the implicit default is the empty string. */
    memset(&in, 0, sizeof(in));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, strdef_encode(&_info_strdef_zero, &in),
        "empty string must be omitted against the zero baseline");

    memset(&in, 0, sizeof(in));
    strcpy(in.label, "abcdef");
    strcpy(in.label, ""); /* "\0bcdef\0": logical "" */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, strdef_encode(&_info_strdef_zero, &in),
        "logically-empty string with dirty tail must be omitted");

    memset(&in, 0, sizeof(in));
    strcpy(in.label, "x");
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(0, strdef_encode(&_info_strdef_zero, &in),
        "non-empty string must be emitted against the zero baseline");
}

//

/*
 * Object-path array decode: the wire carries the ACTUAL element count (0..N per
 * MESSAGE_SPEC §3 / CORELIB_PLAN §4.7), not necessarily the descriptor capacity.
 * The C object encoder emits the canonical trimmed length (dropping the trailing
 * element-default run), and heap targets (Go/Python/TS/...) likewise encode the
 * real stored length, which may be < N. A C receiver must accept such a message:
 * decode the leading elements and clear the trailing slots to the element default
 * (zero) per MESSAGE_SPEC §3 - a present field never leaves a stale init/default
 * image in [M, N). Over-count (wire > N) stays rejected (generator#100 direction).
 */
typedef struct
{
    uint8_t vals[4];
} arr_count_msg_t;

static const sofab_object_descr_field_t _info_fields_arr_count[] =
{
    SOFAB_OBJECT_FIELD_ARRAY(0, arr_count_msg_t, vals, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
};

static const sofab_object_descr_t _info_arr_count =
    SOFAB_OBJECT_DESCR(_info_fields_arr_count, 1, NULL, 0);

static sofab_ret_t arr_count_decode (arr_count_msg_t *msg, const uint8_t *buf, size_t len)
{
    sofab_istream_t ctx;
    sofab_object_decoder_t dec[2];
    memset(dec, 0, sizeof(dec));
    dec[0].info = &_info_arr_count;
    dec[0].dst = (uint8_t *)msg;
    dec[0].depth = (uint8_t)(sizeof(dec) / sizeof(dec[0]) - 1);
    sofab_istream_init(&ctx, sofab_object_field_cb, (void *)&dec[0]);
    return sofab_istream_feed(&ctx, buf, len);
}

static void test_object_array_count_full_partial_empty (void)
{
    /* full: wire count == capacity -> all four slots filled */
    {
        const uint8_t full[] = {0x03, 0x04, 1, 2, 3, 4};
        arr_count_msg_t msg;
        memset(msg.vals, 0xAA, sizeof(msg.vals));
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, arr_count_decode(&msg, full, sizeof(full)),
            "full array must decode");
        const uint8_t expected[] = {1, 2, 3, 4};
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, msg.vals, 4);
    }

    /* partial: wire count 2 < capacity 4 -> leading two set, tail cleared to
     * the element default (zero), even over the 0xAA sentinel (spec §3). */
    {
        const uint8_t partial[] = {0x03, 0x02, 1, 2};
        arr_count_msg_t msg;
        memset(msg.vals, 0xAA, sizeof(msg.vals));
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, arr_count_decode(&msg, partial, sizeof(partial)),
            "partial array must decode (spec: 0..N)");
        const uint8_t expected[] = {1, 2, 0, 0};
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, msg.vals, 4);
    }

    /* empty: wire count 0 -> present with count 0, so every slot is cleared to
     * the element default (zero) - distinct from an absent field (spec §3). */
    {
        const uint8_t empty[] = {0x03, 0x00};
        arr_count_msg_t msg;
        memset(msg.vals, 0xAA, sizeof(msg.vals));
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, arr_count_decode(&msg, empty, sizeof(empty)),
            "explicit-empty array must decode (spec: 0..N)");
        const uint8_t expected[] = {0, 0, 0, 0};
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, msg.vals, 4);
    }

    /* over-count: wire count 5 > capacity 4 -> rejected */
    {
        const uint8_t over[] = {0x03, 0x05, 1, 2, 3, 4, 5};
        arr_count_msg_t msg;
        memset(msg.vals, 0xAA, sizeof(msg.vals));
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_INVALID_MSG, arr_count_decode(&msg, over, sizeof(over)),
            "over-count array must be rejected");
    }
}

//

/*
 * MESSAGE_SPEC §3 trailing-default-run rule (issue #86, Crucible F-0010), both
 * directions, on the C object-descriptor path.
 *
 * A count:N array is always exactly N logical elements, but the encoder MUST
 * write M = one past the last element that differs from the element default and
 * MUST NOT emit the trailing default (zero) run; a decoder MUST refill [M, N)
 * from the element default and MUST NOT let a schema default: image survive into
 * that tail once the field is present on the wire.
 */
typedef struct
{
    uint32_t u32s[5];
} trailrun_u32_t;

static const sofab_object_descr_field_t _info_fields_trailrun_u32[] =
{
    SOFAB_OBJECT_FIELD_ARRAY(0, trailrun_u32_t, u32s, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
};

/* No default image: the element default is zero. */
static const sofab_object_descr_t _info_trailrun_u32 =
    SOFAB_OBJECT_DESCR(_info_fields_trailrun_u32, 1, NULL, 0);

/* Default image seeds _init to {1,2,3,0,0}: the gap-2 leak vector. */
static const trailrun_u32_t _info_defaults_trailrun_u32 = { .u32s = {1, 2, 3, 0, 0} };
static const sofab_object_descr_t _info_trailrun_u32_def =
    SOFAB_OBJECT_DESCR_WITH_DEFAULTS(_info_fields_trailrun_u32, 1, NULL, 0,
        &_info_defaults_trailrun_u32);

typedef struct
{
    float fp32s[4];
} trailrun_fp32_t;

static const sofab_object_descr_field_t _info_fields_trailrun_fp32[] =
{
    SOFAB_OBJECT_FIELD_ARRAY(0, trailrun_fp32_t, fp32s, SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32),
};

static const sofab_object_descr_t _info_trailrun_fp32 =
    SOFAB_OBJECT_DESCR(_info_fields_trailrun_fp32, 1, NULL, 0);

static size_t trailrun_encode (const sofab_object_descr_t *info, const void *in,
                               uint8_t *buffer, size_t buflen)
{
    sofab_ostream_t octx;
    sofab_ostream_init(&octx, buffer, buflen, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, info, in), "encode failed");
    return sofab_ostream_flush(&octx);
}

static sofab_ret_t trailrun_u32_decode (const sofab_object_descr_t *info,
                                        trailrun_u32_t *msg,
                                        const uint8_t *buf, size_t len)
{
    sofab_istream_t ctx;
    sofab_object_decoder_t dec[2];
    memset(dec, 0, sizeof(dec));
    dec[0].info = info;
    dec[0].dst = (uint8_t *)msg;
    dec[0].depth = (uint8_t)(sizeof(dec) / sizeof(dec[0]) - 1);
    sofab_istream_init(&ctx, sofab_object_field_cb, (void *)&dec[0]);
    return sofab_istream_feed(&ctx, buf, len);
}

static void test_object_array_trailing_default_run (void)
{
    uint8_t buffer[32];

    /* gap 1 (varint): value [7,8,9,0,0] encodes to the canonical count 3 with no
     * trailing default run -- the issue's reproducer (id 0 -> tag 0x03). */
    {
        trailrun_u32_t in;
        sofab_object_init(&_info_trailrun_u32, &in);
        in.u32s[0] = 7; in.u32s[1] = 8; in.u32s[2] = 9; /* [7,8,9,0,0] */
        size_t used = trailrun_encode(&_info_trailrun_u32, &in, buffer, sizeof(buffer));
        const uint8_t expected[] = {0x03, 0x03, 0x07, 0x08, 0x09};
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
            "encode must trim the trailing default run (count 3, not 5)");
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);
    }

    /* gap 1 (varint): an all-default value that still differs from a non-zero
     * default image is present with count 0 -- distinct from being omitted. */
    {
        trailrun_u32_t in;
        sofab_object_init(&_info_trailrun_u32_def, &in); /* {1,2,3,0,0} */
        in.u32s[0] = 0; in.u32s[1] = 0; in.u32s[2] = 0;  /* [0,0,0,0,0] != default */
        size_t used = trailrun_encode(&_info_trailrun_u32_def, &in, buffer, sizeof(buffer));
        const uint8_t expected[] = {0x03, 0x00};
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
            "all-default array differing from its default image emits count 0");
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);
    }

    /* gap 1 (fixlen): fp32 [1.5, 2.5, 0, 0] encodes to the canonical count 2. */
    {
        trailrun_fp32_t in;
        sofab_object_init(&_info_trailrun_fp32, &in);
        in.fp32s[0] = 1.5f; in.fp32s[1] = 2.5f; /* [1.5, 2.5, 0, 0] */
        size_t used = trailrun_encode(&_info_trailrun_fp32, &in, buffer, sizeof(buffer));
        /* [tag 0x05][count 2][fixlen_word 0x20][1.5f LE][2.5f LE] */
        const uint8_t expected[] = {
            0x05, 0x02, 0x20,
            0x00, 0x00, 0xC0, 0x3F,  /* 1.5f */
            0x00, 0x00, 0x20, 0x40}; /* 2.5f */
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
            "fixlen encode must trim the trailing default run (count 2, not 4)");
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);
    }

    /* gap 1 (fixlen): a trailing -0.0 has its sign bit set, so its byte image is
     * NOT the default -- it (and every element before it) stays on the wire. */
    {
        trailrun_fp32_t in;
        sofab_object_init(&_info_trailrun_fp32, &in);
        in.fp32s[0] = 1.5f; in.fp32s[3] = -0.0f; /* [1.5, +0.0, +0.0, -0.0] */
        size_t used = trailrun_encode(&_info_trailrun_fp32, &in, buffer, sizeof(buffer));
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x04, buffer[1],
            "a trailing -0.0 must not be trimmed as a default (count stays 4)");
        TEST_ASSERT_EQUAL_size_t(3 + 4 * sizeof(float), used);
    }

    /* gap 2: decoding the canonical short wire must clear [M, N) to the element
     * default, not leave the schema default's u32s[2]==3 behind. */
    {
        trailrun_u32_t msg;
        /* value [1,2,0,0,0] trimmed to count 2: [tag 0x03][count 2][1][2] */
        const uint8_t wire[] = {0x03, 0x02, 0x01, 0x02};
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
            trailrun_u32_decode(&_info_trailrun_u32_def, &msg, wire, sizeof(wire)),
            "short array must decode");
        const uint32_t expected[] = {1, 2, 0, 0, 0};
        TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, msg.u32s, 5);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, msg.u32s[2],
            "schema default must not survive into [M, N)");
    }

    /* end-to-end: encode [1,2,0,0,0] against the default image and feed it back;
     * the tail must round-trip as the element default (zero), not {..,3,..}. */
    {
        trailrun_u32_t in;
        sofab_object_init(&_info_trailrun_u32_def, &in); /* {1,2,3,0,0} */
        in.u32s[0] = 1; in.u32s[1] = 2; in.u32s[2] = 0; /* [1,2,0,0,0] */
        size_t used = trailrun_encode(&_info_trailrun_u32_def, &in, buffer, sizeof(buffer));

        trailrun_u32_t out;
        TEST_ASSERT_EQUAL(SOFAB_RET_OK,
            trailrun_u32_decode(&_info_trailrun_u32_def, &out, buffer, used));
        const uint32_t expected[] = {1, 2, 0, 0, 0};
        TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, out.u32s, 5);
    }
}

//

/*
 * A sized blob (SOFAB_OBJECT_FIELD_BLOB_SIZED) is a fixed-capacity buffer paired
 * with an adjacent used-length member. Only used_len bytes are written to the
 * wire (byte-identical to a plain blob of that length); the decoded length is
 * stored back into used_len; and an empty blob (used_len == 0) is omitted by the
 * sparse rule -- exactly like the STRING content path above.
 */
typedef struct
{
    uint8_t used_len;   /* length precedes the buffer (alignment-robust) */
    uint8_t data[8];
} blobsized_t;

static const sofab_object_descr_field_t _info_fields_blobsized[] =
{
    SOFAB_OBJECT_FIELD_BLOB_SIZED(0, blobsized_t, data, used_len),
};

static const sofab_object_descr_t _info_blobsized =
    SOFAB_OBJECT_DESCR(_info_fields_blobsized, 1, NULL, 0);

/*
 * Alignment regression: a wide (uint16) length in front of an odd-sized buffer.
 * With the length placed before the buffer there is never padding between them,
 * so the descriptor locates used_len correctly for any width/size. (A length
 * placed *after* an odd buffer would be padded away and silently corrupted.)
 */
typedef struct
{
    uint16_t used_len;
    uint8_t  data[7];
} blobsized_wide_t;

static const sofab_object_descr_field_t _info_fields_blobsized_wide[] =
{
    SOFAB_OBJECT_FIELD_BLOB_SIZED(0, blobsized_wide_t, data, used_len),
};

static const sofab_object_descr_t _info_blobsized_wide =
    SOFAB_OBJECT_DESCR(_info_fields_blobsized_wide, 1, NULL, 0);

static size_t blobsized_encode (const blobsized_t *in, uint8_t *buf, size_t buflen)
{
    sofab_ostream_t octx;
    sofab_ostream_init(&octx, buf, buflen, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, &_info_blobsized, in), "encode failed");
    return sofab_ostream_flush(&octx);
}

static void test_object_blob_sized (void)
{
    uint8_t buf[32];

    /* 1. a blob shorter than its buffer serialises with its actual length; the
     *    dirty tail past used_len must not reach the wire. */
    blobsized_t in;
    memset(&in, 0, sizeof(in));
    in.data[0] = 0xAA; in.data[1] = 0xBB; in.data[2] = 0xCC;
    in.data[3] = 0xDD; in.data[4] = 0xEE; /* tail beyond used_len */
    in.used_len = 3;

    size_t used = blobsized_encode(&in, buf, sizeof(buf));
    /* id 0 | FIXLEN(2) = 0x02 ; (3 << 3) | BLOB(3) = 0x1B ; then 3 payload bytes */
    const uint8_t expected[] = { 0x02, 0x1B, 0xAA, 0xBB, 0xCC };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used, "sized blob wire length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buf, used, "sized blob wire bytes");

    /* 2. round-trips losslessly: both the bytes and used_len are restored. */
    blobsized_t out;
    memset(&out, 0, sizeof(out));

    sofab_istream_t ictx;
    sofab_object_decoder_t dec;
    memset(&dec, 0, sizeof(dec));
    dec.info = &_info_blobsized;
    dec.dst = (uint8_t *)&out;
    dec.depth = 0;
    sofab_istream_init(&ictx, sofab_object_field_cb, &dec);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_istream_feed(&ictx, buf, used), "decode failed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, out.used_len, "used_len not restored");
    const uint8_t expect_data[3] = { 0xAA, 0xBB, 0xCC };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_data, out.data, 3);

    /* 3. an empty sized blob (used_len == 0) is omitted, regardless of buffer
     *    content -- length-driven, not content-driven. */
    memset(&in, 0, sizeof(in));
    in.data[0] = 0x11; /* content present but logical length zero */
    in.used_len = 0;
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, blobsized_encode(&in, buf, sizeof(buf)),
        "empty sized blob must be omitted");

    /* 4. a full-capacity blob emits all N bytes. */
    memset(&in, 0, sizeof(in));
    for (uint8_t i = 0; i < 8; i++) in.data[i] = (uint8_t)(0xF0 + i);
    in.used_len = 8;
    used = blobsized_encode(&in, buf, sizeof(buf));
    /* (8 << 3) | BLOB(3) = 0x43 */
    const uint8_t expected_full[] = {
        0x02, 0x43, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected_full), used, "full sized blob length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected_full, buf, used, "full sized blob bytes");

    /* 5. alignment regression: uint16 used_len before an odd (7-byte) buffer
     *    round-trips losslessly -- the length is located correctly despite the
     *    struct's natural 2-byte alignment. */
    blobsized_wide_t win;
    memset(&win, 0, sizeof(win));
    win.data[0] = 0x10; win.data[1] = 0x20; win.data[2] = 0x30; win.data[3] = 0x40;
    win.used_len = 4;
    used = 0;
    {
        sofab_ostream_t wos;
        sofab_ostream_init(&wos, buf, sizeof(buf), 0, NULL, NULL);
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
            sofab_object_encode(&wos, &_info_blobsized_wide, &win), "wide encode failed");
        used = sofab_ostream_flush(&wos);
    }
    const uint8_t expected_wide[] = { 0x02, 0x23, 0x10, 0x20, 0x30, 0x40 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected_wide), used, "wide sized blob length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected_wide, buf, used, "wide sized blob bytes");

    blobsized_wide_t wout;
    memset(&wout, 0, sizeof(wout));
    sofab_istream_t wis;
    sofab_object_decoder_t wdec;
    memset(&wdec, 0, sizeof(wdec));
    wdec.info = &_info_blobsized_wide;
    wdec.dst = (uint8_t *)&wout;
    wdec.depth = 0;
    sofab_istream_init(&wis, sofab_object_field_cb, &wdec);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_istream_feed(&wis, buf, used), "wide decode failed");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(4, wout.used_len, "wide used_len not restored");
    const uint8_t expect_wide_data[4] = { 0x10, 0x20, 0x30, 0x40 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_wide_data, wout.data, 4);
}

//
// issue #94: the C object API must reject an over-index element of a fixed-count
// string/blob wrapper array (a SOFAB_OBJECT_DESCR_SEQ holder) as INVALID rather
// than silently drop it — the object-API counterpart of the #92/#93 abort
// channel. A message descriptor must still SKIP an unknown (forward-compat) id.
//

#define _OVERIDX_CAP 5

/* fixed-count string[5] holder + a 1-field message wrapping it at id 200 */
typedef struct { char strings[_OVERIDX_CAP][16]; } _overidx_str_holder_t;
static const sofab_object_descr_field_t _overidx_str_fields[] = {
    SOFAB_OBJECT_FIELD(0, _overidx_str_holder_t, strings[0], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(1, _overidx_str_holder_t, strings[1], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(2, _overidx_str_holder_t, strings[2], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(3, _overidx_str_holder_t, strings[3], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(4, _overidx_str_holder_t, strings[4], SOFAB_OBJECT_FIELDTYPE_STRING),
};
static const sofab_object_descr_t _overidx_str_holder =
    SOFAB_OBJECT_DESCR_SEQ(_overidx_str_fields, _OVERIDX_CAP, NULL, 0);

typedef struct { _overidx_str_holder_t arr; } _overidx_str_msg_t;
static const sofab_object_descr_field_t _overidx_str_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _overidx_str_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _overidx_str_nested[] = { &_overidx_str_holder };
static const sofab_object_descr_t _overidx_str_msg =
    SOFAB_OBJECT_DESCR(_overidx_str_msg_fields, 1, _overidx_str_nested, 1);

/* fixed-count blob[5] holder + its wrapper message */
typedef struct { uint8_t blobs[_OVERIDX_CAP][16]; } _overidx_blob_holder_t;
static const sofab_object_descr_field_t _overidx_blob_fields[] = {
    SOFAB_OBJECT_FIELD(0, _overidx_blob_holder_t, blobs[0], SOFAB_OBJECT_FIELDTYPE_BLOB),
    SOFAB_OBJECT_FIELD(1, _overidx_blob_holder_t, blobs[1], SOFAB_OBJECT_FIELDTYPE_BLOB),
    SOFAB_OBJECT_FIELD(2, _overidx_blob_holder_t, blobs[2], SOFAB_OBJECT_FIELDTYPE_BLOB),
    SOFAB_OBJECT_FIELD(3, _overidx_blob_holder_t, blobs[3], SOFAB_OBJECT_FIELDTYPE_BLOB),
    SOFAB_OBJECT_FIELD(4, _overidx_blob_holder_t, blobs[4], SOFAB_OBJECT_FIELDTYPE_BLOB),
};
static const sofab_object_descr_t _overidx_blob_holder =
    SOFAB_OBJECT_DESCR_SEQ(_overidx_blob_fields, _OVERIDX_CAP, NULL, 0);

typedef struct { _overidx_blob_holder_t arr; } _overidx_blob_msg_t;
static const sofab_object_descr_field_t _overidx_blob_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _overidx_blob_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _overidx_blob_nested[] = { &_overidx_blob_holder };
static const sofab_object_descr_t _overidx_blob_msg =
    SOFAB_OBJECT_DESCR(_overidx_blob_msg_fields, 1, _overidx_blob_nested, 1);

static sofab_ret_t _overidx_decode (
    const sofab_object_descr_t *info, void *dst, const uint8_t *buf, size_t len)
{
    struct { sofab_istream_t ctx; sofab_object_decoder_t decoder[2]; } d;
    memset(&d, 0, sizeof(d));
    d.decoder[0].info  = info;
    d.decoder[0].dst   = (uint8_t *)dst;
    d.decoder[0].depth = 1;   /* one nesting level (the holder sequence) */
    sofab_istream_init(&d.ctx, sofab_object_field_cb, &d.decoder[0]);
    return sofab_istream_feed(&d.ctx, buf, len);
}

/* id 200 SEQUENCE_START (0xC6 0x0C), then one string element ... , SEQUENCE_END */
static void test_object_overindex_string_rejected (void)
{
    _overidx_str_msg_t msg;
    /* element wire id 5 (>= capacity 5): string "x" -> INVALID */
    const uint8_t buf[] = {0xC6, 0x0C, 0x2A, 0x0A, 0x78, 0x07};
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG,
        _overidx_decode(&_overidx_str_msg, &msg, buf, sizeof(buf)));
}

static void test_object_overindex_string_in_range_ok (void)
{
    _overidx_str_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    /* element wire id 4 (< capacity 5): string "x" -> OK, stored in slot 4 */
    const uint8_t buf[] = {0xC6, 0x0C, 0x22, 0x0A, 0x78, 0x07};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("x", msg.arr.strings[4]);
}

static void test_object_overindex_blob_rejected (void)
{
    _overidx_blob_msg_t msg;
    /* element wire id 5 (>= capacity 5): blob 0x78 -> INVALID */
    const uint8_t buf[] = {0xC6, 0x0C, 0x2A, 0x0B, 0x78, 0x07};
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG,
        _overidx_decode(&_overidx_blob_msg, &msg, buf, sizeof(buf)));
}

static void test_object_overindex_blob_in_range_ok (void)
{
    _overidx_blob_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    /* element wire id 4 (< capacity 5): blob 0x78 -> OK, stored in slot 4 */
    const uint8_t buf[] = {0xC6, 0x0C, 0x22, 0x0B, 0x78, 0x07};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_overidx_blob_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0x78, msg.arr.blobs[4][0]);
}

static void test_object_message_unknown_id_still_skipped (void)
{
    /* Regression: a normal (non-holder) message must SKIP an unknown/forward-
     * compat id, not reject it. Top-level unknown id 99, u8 = 42. */
    _overidx_str_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {0x98, 0x06, 0x2A};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, buf, sizeof(buf)));
}

//
// issue #100: MESSAGE_SPEC §7.3 — a matched field whose header wire type (wire
// type + fixlen subtype) contradicts the declared type must be SKIPPED, exactly
// like an unknown id, not rejected with E_USAGE. The single-byte header packs
// (id << 3) | wire_type; a fixlen word packs (length << 3) | fixlen_subtype.
//

typedef struct { uint8_t x; } _wt_inner_t;      /* one u8 at id 0 */
typedef struct {
    uint8_t     u8;      /* id 0,  declared UNSIGNED */
    char        str[8];  /* id 1,  declared STRING   */
    _wt_inner_t nested;  /* id 10, declared SEQUENCE */
} _wt_msg_t;

static const sofab_object_descr_field_t _wt_inner_fields[] = {
    SOFAB_OBJECT_FIELD(0, _wt_inner_t, x, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};
static const sofab_object_descr_t _wt_inner_descr =
    SOFAB_OBJECT_DESCR(_wt_inner_fields, 1, NULL, 0);

static const sofab_object_descr_field_t _wt_msg_fields[] = {
    SOFAB_OBJECT_FIELD(0, _wt_msg_t, u8, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, _wt_msg_t, str, SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD_SEQUENCE(10, _wt_msg_t, nested, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _wt_nested[] = { &_wt_inner_descr };
static const sofab_object_descr_t _wt_msg =
    SOFAB_OBJECT_DESCR(_wt_msg_fields, 3, _wt_nested, 1);

static void test_object_wiretype_control_decodes (void)
{
    /* Control: id 0 carries UNSIGNED, matching the schema -> u8 = 5. */
    _wt_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {0x00, 0x05};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wt_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(5, msg.u8);
}

static void test_object_wiretype_signed_for_unsigned_skipped (void)
{
    /* id 0 declared UNSIGNED, header carries SIGNED -> skip, u8 stays default. */
    _wt_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {0x01, 0x06};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wt_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0, msg.u8);
}

static void test_object_wiretype_array_for_scalar_skipped (void)
{
    /* id 0 declared UNSIGNED, header carries ARRAY_UNSIGNED (count 1, elem 5)
     * -> skip, u8 stays default. */
    _wt_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {0x03, 0x01, 0x05};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wt_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0, msg.u8);
}

static void test_object_wiretype_scalar_for_sequence_skipped (void)
{
    /* id 10 declared SEQUENCE, header carries UNSIGNED -> skip, nested untouched. */
    _wt_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {0x50, 0x05};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wt_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0, msg.nested.x);
}

static void test_object_wiretype_sequence_for_scalar_skipped (void)
{
    /* id 0 declared UNSIGNED, header opens a (empty) SEQUENCE -> the whole
     * sequence is skipped via skip_depth, u8 stays default. */
    _wt_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {0x06, 0x07};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wt_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0, msg.u8);
}

static void test_object_wiretype_fixlen_subtype_skipped (void)
{
    /* id 1 declared STRING, header carries FIXLEN/BLOB (differs only in the
     * fixlen subtype, mask 0x3F) -> skip, str stays empty. The matching-subtype
     * control (STRING) still decodes to prove the check is subtype-precise. */
    _wt_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t blob_for_string[] = {0x0A, 0x0B, 0x78};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_wt_msg, &msg, blob_for_string, sizeof(blob_for_string)));
    TEST_ASSERT_EQUAL_STRING("", msg.str);

    memset(&msg, 0, sizeof(msg));
    const uint8_t string_ok[] = {0x0A, 0x0A, 0x78};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_wt_msg, &msg, string_ok, sizeof(string_ok)));
    TEST_ASSERT_EQUAL_STRING("x", msg.str);
}

//
// issue #99: MESSAGE_SPEC §7.4 — a re-opened array wrapper REPLACES the array
// value whole (each open resets slots to defaults), whereas a struct/union
// MERGES (last occurrence wins per field id). The wrapper is distinguished by
// the fixed_seq flag (SOFAB_OBJECT_DESCR_SEQ), reused from issue #94.
//

#define _WRAP_CAP 3
typedef struct { char strings[_WRAP_CAP][8]; } _wrap_holder_t;
static const sofab_object_descr_field_t _wrap_fields[] = {
    SOFAB_OBJECT_FIELD(0, _wrap_holder_t, strings[0], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(1, _wrap_holder_t, strings[1], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(2, _wrap_holder_t, strings[2], SOFAB_OBJECT_FIELDTYPE_STRING),
};
static const sofab_object_descr_t _wrap_holder =   /* fixed_seq = 1 (wrapper) */
    SOFAB_OBJECT_DESCR_SEQ(_wrap_fields, _WRAP_CAP, NULL, 0);

typedef struct { _wrap_holder_t arr; } _wrap_msg_t;
static const sofab_object_descr_field_t _wrap_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _wrap_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _wrap_nested[] = { &_wrap_holder };
static const sofab_object_descr_t _wrap_msg =
    SOFAB_OBJECT_DESCR(_wrap_msg_fields, 1, _wrap_nested, 1);

static void test_object_wrapper_reopen_replaces (void)
{
    /* string_array (id 200) opened twice: element 0 = "A" in the first opening,
     * element 1 = "B" in the second. §7.4 replaces -> ["", "B", ""]. */
    _wrap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {
        0xC6, 0x0C, 0x02, 0x0A, 0x41, 0x07,   /* open, strings[0]="A", close */
        0xC6, 0x0C, 0x0A, 0x0A, 0x42, 0x07,   /* re-open, strings[1]="B", close */
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wrap_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("",  msg.arr.strings[0]);
    TEST_ASSERT_EQUAL_STRING("B", msg.arr.strings[1]);
    TEST_ASSERT_EQUAL_STRING("",  msg.arr.strings[2]);
}

static void test_object_wrapper_single_open_unchanged (void)
{
    /* Control: both elements in ONE opening must still yield ["A", "B", ""]. */
    _wrap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {
        0xC6, 0x0C, 0x02, 0x0A, 0x41, 0x0A, 0x0A, 0x42, 0x07,
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wrap_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("A", msg.arr.strings[0]);
    TEST_ASSERT_EQUAL_STRING("B", msg.arr.strings[1]);
    TEST_ASSERT_EQUAL_STRING("",  msg.arr.strings[2]);
}

/* struct holder (fixed_seq = 0) must keep MERGING across re-opens */
typedef struct { uint8_t a; uint8_t b; } _mrg_inner_t;
static const sofab_object_descr_field_t _mrg_fields[] = {
    SOFAB_OBJECT_FIELD(0, _mrg_inner_t, a, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, _mrg_inner_t, b, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};
static const sofab_object_descr_t _mrg_inner =
    SOFAB_OBJECT_DESCR(_mrg_fields, 2, NULL, 0);

typedef struct { _mrg_inner_t s; } _mrg_msg_t;
static const sofab_object_descr_field_t _mrg_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _mrg_msg_t, s, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _mrg_nested[] = { &_mrg_inner };
static const sofab_object_descr_t _mrg_msg =
    SOFAB_OBJECT_DESCR(_mrg_msg_fields, 1, _mrg_nested, 1);

static void test_object_struct_reopen_merges (void)
{
    /* Struct (fixed_seq == 0): first opening sets a = 9, second sets b = 7.
     * §7.4 merges -> both retained (a reset-on-open would drop a). */
    _mrg_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {
        0xC6, 0x0C, 0x00, 0x09, 0x07,   /* open, a = 9, close */
        0xC6, 0x0C, 0x08, 0x07, 0x07,   /* re-open, b = 7, close */
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_mrg_msg, &msg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(9, msg.s.a);
    TEST_ASSERT_EQUAL_UINT8(7, msg.s.b);
}

//
// issue #106: the §7.4 wrapper-replace above must also reset a SIZED BLOB slot's
// companion used-length. That length sits before the buffer and is NOT covered by
// the descriptor's (offset, size), so sofab_object_init's generic clear missed it:
// on a re-open the dropped element survived as an all-zero blob. A string wrapper
// (no separate length) already reset correctly — this is the blob-specific residual.
//

#define _BWRAP_CAP 3
/* three sized-blob element slots; each length member immediately precedes its
 * buffer, as SOFAB_OBJECT_FIELD_BLOB_SIZED requires (all uint8 -> no padding). */
typedef struct {
    uint8_t l0; uint8_t b0[8];
    uint8_t l1; uint8_t b1[8];
    uint8_t l2; uint8_t b2[8];
} _bwrap_holder_t;
static const sofab_object_descr_field_t _bwrap_fields[] = {
    SOFAB_OBJECT_FIELD_BLOB_SIZED(0, _bwrap_holder_t, b0, l0),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(1, _bwrap_holder_t, b1, l1),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(2, _bwrap_holder_t, b2, l2),
};
static const sofab_object_descr_t _bwrap_holder =   /* fixed_seq = 1 (wrapper) */
    SOFAB_OBJECT_DESCR_SEQ(_bwrap_fields, _BWRAP_CAP, NULL, 0);

typedef struct { _bwrap_holder_t arr; } _bwrap_msg_t;
static const sofab_object_descr_field_t _bwrap_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _bwrap_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _bwrap_nested[] = { &_bwrap_holder };
static const sofab_object_descr_t _bwrap_msg =
    SOFAB_OBJECT_DESCR(_bwrap_msg_fields, 1, _bwrap_nested, 1);

static void test_object_wrapper_reopen_replaces_blob (void)
{
    /* blob_array (id 200) opened with element 0 = "de ad", then RE-OPENED empty.
     * §7.4 replaces the array whole -> it must be empty. */
    _bwrap_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    const uint8_t buf[] = {
        0xC6, 0x0C, 0x02, 0x13, 0xDE, 0xAD, 0x07,   /* open, blobs[0]="dead", close */
        0xC6, 0x0C, 0x07,                            /* re-open empty, close */
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_bwrap_msg, &msg, buf, sizeof(buf)));

    /* the re-open dropped element 0: its used-length is back to 0 (pre-fix it stayed
     * 2, so _field_is_default saw the slot as present). */
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, msg.arr.l0, "sized-blob length not reset on §7.4 re-open");
    TEST_ASSERT_EQUAL_UINT8(0, msg.arr.l1);
    TEST_ASSERT_EQUAL_UINT8(0, msg.arr.l2);

    /* and it re-encodes as an empty wrapper (begin C6 0C, end 07) — never as a
     * stale "00 00" blob for element 0. */
    uint8_t out[32];
    sofab_ostream_t octx;
    sofab_ostream_init(&octx, out, sizeof(out), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, &_bwrap_msg, &msg), "re-encode failed");
    size_t used = sofab_ostream_flush(&octx);
    const uint8_t expected_empty[] = { 0xC6, 0x0C, 0x07 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected_empty), used, "re-encoded wrapper not empty");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected_empty, out, used, "re-encoded wrapper bytes");
}

//

int test_object_main (void)
{
    UNITY_BEGIN();

    RUN_TEST(test_object_serialize);
    RUN_TEST(test_object_deserialize);
    RUN_TEST(test_object_array_count_full_partial_empty);
    RUN_TEST(test_object_array_trailing_default_run);

    RUN_TEST(test_object_serialize_invalid_unsigned_size);
    RUN_TEST(test_object_serialize_invalid_signed_size);
    RUN_TEST(test_object_serialize_invalid_field_type);

    RUN_TEST(test_object_deserialize_invalid_nested_depth);
    RUN_TEST(test_object_deserialize_invalid_field_type);

    RUN_TEST(test_object_default_sequence_emitted_empty);
    RUN_TEST(test_object_roundtrip_empty_sequence_before_sequence);
    RUN_TEST(test_object_string_default_omission);
    RUN_TEST(test_object_blob_sized);

    RUN_TEST(test_object_overindex_string_rejected);
    RUN_TEST(test_object_overindex_string_in_range_ok);
    RUN_TEST(test_object_overindex_blob_rejected);
    RUN_TEST(test_object_overindex_blob_in_range_ok);
    RUN_TEST(test_object_message_unknown_id_still_skipped);

    RUN_TEST(test_object_wiretype_control_decodes);
    RUN_TEST(test_object_wiretype_signed_for_unsigned_skipped);
    RUN_TEST(test_object_wiretype_array_for_scalar_skipped);
    RUN_TEST(test_object_wiretype_scalar_for_sequence_skipped);
    RUN_TEST(test_object_wiretype_sequence_for_scalar_skipped);
    RUN_TEST(test_object_wiretype_fixlen_subtype_skipped);

    RUN_TEST(test_object_wrapper_reopen_replaces);
    RUN_TEST(test_object_wrapper_reopen_replaces_blob);
    RUN_TEST(test_object_wrapper_single_open_unchanged);
    RUN_TEST(test_object_struct_reopen_merges);

    return UNITY_END();
}
