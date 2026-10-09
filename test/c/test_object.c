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

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_ARGUMENT, "ret != SOFAB_RET_E_ARGUMENT");
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

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_ARGUMENT, "ret != SOFAB_RET_E_ARGUMENT");
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

    TEST_ASSERT_EQUAL_MESSAGE(ret, SOFAB_RET_E_ARGUMENT, "ret != SOFAB_RET_E_ARGUMENT");
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
 * sequence contributes nothing to the wire.
 *
 * MESSAGE_SPEC §2 omits an all-default sequence FIELD outright instead of framing
 * it empty, which makes this case sharper than it was before the rule: seq_a
 * produces no bytes at all, so the only sequence on the wire is seq_b, at
 * nested_idx 1. Encoder and decoder must still key off that field's own
 * nested_idx; a naive running counter over *emitted* sequences would now hand
 * seq_b the nested_list[0] descriptor and corrupt the stream.
 *
 * Layout: seq_a (nested_idx 0) is all-default -> omitted entirely; seq_b
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
    regr_seq_a_t a; /* sequence, all-default -> omitted entirely (§2) */
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
    /* leave `a` all-default -> omitted entirely (§2); only `b` carries data */
    strncpy(in.b.s, "hello", sizeof(in.b.s));
    in.b.y = 0xABCD;

    sofab_ostream_t octx;
    uint8_t buffer[128];
    sofab_ostream_init(&octx, buffer, sizeof(buffer), 0, NULL, NULL);
    sofab_ret_t enc = sofab_object_encode(&octx, &_info_regr_msg, &in);
    size_t used = sofab_ostream_flush(&octx);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, enc, "encode failed");
    /* seq_a left no trace: the stream opens directly with seq_b's header,
     * (1 << 3) | SEQUENCE_START = 0x0E. That is what makes the nested_idx of the
     * *following* sequence differ from its position among the emitted ones. */
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0E, buffer[0],
        "an all-default sequence field must be omitted, not framed empty");

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
 * MESSAGE_SPEC §2: a nested object (SEQUENCE) whose fields are all at their
 * default is OMITTED, not framed empty -- the ≠-default test is per field and a
 * sequence is no exception. The predicate is the recursive per-child comparison
 * of _field_is_default, never a whole-object memcmp/_iszero over raw storage
 * (which would compare struct padding and mishandle a non-zero nested default).
 * Absence reconstructs the same value via sofab_object_init, so dropping the two
 * framing bytes is value-preserving.
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

static size_t _defseq_encode (const defseq_msg_t *in, uint8_t *out, size_t outlen)
{
    sofab_ostream_t octx;
    sofab_ostream_init(&octx, out, outlen, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, &_info_defseq_msg, in), "encode failed");
    return sofab_ostream_flush(&octx);
}

static void test_object_default_sequence_omitted (void)
{
    defseq_msg_t in;
    memset(&in, 0, sizeof(in)); /* inner all-default */

    uint8_t buffer[32];
    size_t used = _defseq_encode(&in, buffer, sizeof(buffer));

    /*
     * Omitted entirely: the pre-§2-uniform encoder wrote the empty wrapper
     * sequence_begin(7) = (7<<3)|0b110 = 0x3e plus sequence_end 0x07. The whole
     * message is all-default, so the canonical encoding is the empty byte string.
     */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, used,
        "an all-default nested sequence must be omitted, not framed empty");
}

/* A single non-default child is enough to bring the frame back. */
static void test_object_nondefault_sequence_framed (void)
{
    defseq_msg_t in;
    memset(&in, 0, sizeof(in));
    in.inner.y = 3;

    uint8_t buffer[32];
    size_t used = _defseq_encode(&in, buffer, sizeof(buffer));

    /* 3e = seq_begin(7); 08 03 = id 1 unsigned 3; 07 = seq_end. */
    static const uint8_t expected[] = { 0x3e, 0x08, 0x03, 0x07 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "a sequence with a non-default child must still be framed");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, buffer, used,
        "framed nested sequence bytes mismatch");
}

/*
 * The omission is value-preserving: the zero-byte message and the empty frame
 * both decode to the all-default value, so the empty frame stays acceptable as a
 * non-canonical encoding of it (§2 accept-and-normalize).
 */
static void test_object_default_sequence_roundtrips_both_forms (void)
{
    static const uint8_t empty_frame[] = { 0x3e, 0x07 };
    const uint8_t *forms[] = { NULL, empty_frame };
    const size_t lens[] = { 0, sizeof(empty_frame) };

    for (size_t f = 0; f < 2; f++)
    {
        defseq_msg_t out;
        memset(&out, 0x55, sizeof(out));
        sofab_object_init(&_info_defseq_msg, &out);

        struct
        {
            sofab_istream_t ctx;
            sofab_object_decoder_t decoder[4];
        } dec;
        memset(&dec, 0, sizeof(dec));
        dec.decoder[0].info = &_info_defseq_msg;
        dec.decoder[0].dst = (uint8_t *)&out;
        dec.decoder[0].depth = sizeof(dec.decoder) / sizeof(dec.decoder[0]) - 1;
        sofab_istream_init(&dec.ctx, sofab_object_field_cb, (void *)&dec.decoder[0]);

        /* A zero-length feed is legal and returns the outcome so far -- and the
         * canonical all-default message is fed the natural way, as (NULL, 0):
         * forms[0] IS NULL, no dummy pointer. This call therefore also pins that
         * sofab_istream_feed() does not assert on a null zero-length feed. */
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
            sofab_istream_feed(&dec.ctx, forms[f], lens[f]),
            "decode failed");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, out.inner.x, "inner.x must be the default");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, out.inner.y, "inner.y must be the default");
    }
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
 * MESSAGE_SPEC §3 count-is-a-capacity rule, both directions, on the C
 * object-descriptor path.
 *
 * `count: N` bounds the array; the wire count M IS its length. A plain
 * (capacity-only) descriptor has nowhere to keep a length, so its value occupies
 * all N slots and the encoder writes every one of them -- a trailing element
 * equal to the element default INCLUDED, because dropping it would shorten the
 * array ([1,2,3,0,0] and [1,2,3] are different values). A decoder still accepts a
 * shorter wire (M < N) and leaves [M, N) at the element default, never letting a
 * schema `default:` image survive into that tail once the field is present.
 *
 * (This supersedes the trim-on-encode / fill-on-decode pair of issue #86 /
 * Crucible F-0010, which was correct only while `count` meant a fixed length.
 * Carrying a real length needs SOFAB_OBJECT_FIELD_ARRAY_SIZED -- tested below.)
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

static void test_object_array_full_length_no_trim (void)
{
    uint8_t buffer[64];

    /* varint: value [7,8,9,0,0] is five elements and encodes as five -- the two
     * trailing defaults are part of the value, not padding (id 0 -> tag 0x03). */
    {
        trailrun_u32_t in;
        sofab_object_init(&_info_trailrun_u32, &in);
        in.u32s[0] = 7; in.u32s[1] = 8; in.u32s[2] = 9; /* [7,8,9,0,0] */
        size_t used = trailrun_encode(&_info_trailrun_u32, &in, buffer, sizeof(buffer));
        const uint8_t expected[] = {0x03, 0x05, 0x07, 0x08, 0x09, 0x00, 0x00};
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
            "a capacity-only array encodes every element it holds (count 5)");
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);
    }

    /* varint: an all-default value that still differs from a non-zero default
     * image is present -- as five zero elements, not as the empty array, which is
     * a different value (M = 0) since `count` stopped being a length. */
    {
        trailrun_u32_t in;
        sofab_object_init(&_info_trailrun_u32_def, &in); /* {1,2,3,0,0} */
        in.u32s[0] = 0; in.u32s[1] = 0; in.u32s[2] = 0;  /* [0,0,0,0,0] != default */
        size_t used = trailrun_encode(&_info_trailrun_u32_def, &in, buffer, sizeof(buffer));
        const uint8_t expected[] = {0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
            "an all-default array differing from its default image keeps its length");
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);
    }

    /* fixlen: fp32 [1.5, 2.5, 0, 0] likewise keeps its four elements. */
    {
        trailrun_fp32_t in;
        sofab_object_init(&_info_trailrun_fp32, &in);
        in.fp32s[0] = 1.5f; in.fp32s[1] = 2.5f; /* [1.5, 2.5, 0, 0] */
        size_t used = trailrun_encode(&_info_trailrun_fp32, &in, buffer, sizeof(buffer));
        /* [tag 0x05][count 4][fixlen_word 0x20][1.5f][2.5f][0f][0f] */
        const uint8_t expected[] = {
            0x05, 0x04, 0x20,
            0x00, 0x00, 0xC0, 0x3F,   /* 1.5f */
            0x00, 0x00, 0x20, 0x40,   /* 2.5f */
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00};
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
            "a fixlen array keeps its trailing defaults too (count 4)");
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);
    }

    /* decoding a SHORT wire (a peer that holds fewer elements) must still clear
     * [M, N) to the element default, not leave the schema default's u32s[2]==3. */
    {
        trailrun_u32_t msg;
        /* a two-element array from a length-carrying peer: [tag][count 2][1][2] */
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
 * SOFAB_OBJECT_FIELD_ARRAY_SIZED: a fixed-capacity array paired with an adjacent
 * element-count member, the array counterpart of SOFAB_OBJECT_FIELD_BLOB_SIZED.
 * It is what lets a C object hold an array SHORTER than its capacity, which
 * MESSAGE_SPEC §3 requires now that the wire count M is the length: encode writes
 * exactly `len` elements (trailing defaults included), decode stores the received
 * M back into `len`, and the capacity never reaches the wire.
 *
 * The length is declared immediately BEFORE the buffer and at least as wide as one
 * element, so no padding can creep between them -- an invariant the macro asserts
 * at compile time (a byte buffer gets it for free, a uint32_t[] does not).
 */
typedef struct
{
    uint32_t len;        /* elements in use, immediately before the buffer */
    uint32_t vals[5];
} arrsized_t;

static const sofab_object_descr_field_t _info_fields_arrsized[] =
{
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(0, arrsized_t, vals, len,
        SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
};

static const sofab_object_descr_t _info_arrsized =
    SOFAB_OBJECT_DESCR(_info_fields_arrsized, 1, NULL, 0);

/* A byte array takes a byte length with no padding either (alignment 1). */
typedef struct
{
    uint8_t len;
    uint8_t vals[4];
} arrsized_u8_t;

static const sofab_object_descr_field_t _info_fields_arrsized_u8[] =
{
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(0, arrsized_u8_t, vals, len,
        SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
};

static const sofab_object_descr_t _info_arrsized_u8 =
    SOFAB_OBJECT_DESCR(_info_fields_arrsized_u8, 1, NULL, 0);

static sofab_ret_t arrsized_decode (const sofab_object_descr_t *info, void *msg,
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

static void test_object_array_sized (void)
{
    uint8_t buffer[64];

    /* encode: len 3 of capacity 5, with a trailing DEFAULT element -- the whole
     * point of the length member: [1,2,0] stays three elements. */
    {
        arrsized_t in;
        sofab_object_init(&_info_arrsized, &in);
        in.len = 3; in.vals[0] = 1; in.vals[1] = 2; /* [1,2,0] */
        size_t used = trailrun_encode(&_info_arrsized, &in, buffer, sizeof(buffer));
        const uint8_t expected[] = {0x03, 0x03, 0x01, 0x02, 0x00};
        TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
            "a sized array encodes exactly len elements, trailing default included");
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);
    }

    /* encode: len 0 is the empty array, which equals the (undeclared) default and
     * is therefore omitted -- the storage image is irrelevant. */
    {
        arrsized_t in;
        sofab_object_init(&_info_arrsized, &in);
        in.vals[0] = 0xDEAD; in.vals[4] = 0xBEEF;   /* stale, len == 0 */
        size_t used = trailrun_encode(&_info_arrsized, &in, buffer, sizeof(buffer));
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0, used,
            "a zero-length sized array is the default and must be omitted");
    }

    /* decode: the received count lands in len, and [M, N) is the element default */
    {
        arrsized_t msg;
        memset(&msg, 0xAA, sizeof(msg));
        const uint8_t wire[] = {0x03, 0x02, 0x07, 0x08};
        TEST_ASSERT_EQUAL(SOFAB_RET_OK,
            arrsized_decode(&_info_arrsized, &msg, wire, sizeof(wire)));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, msg.len, "decode must record the wire count");
        const uint32_t expected[] = {7, 8, 0, 0, 0};
        TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, msg.vals, 5);
    }

    /* round-trip: a length shorter than the capacity survives, which a plain
     * SOFAB_OBJECT_FIELD_ARRAY cannot do (it would re-encode all five). */
    {
        arrsized_t in;
        sofab_object_init(&_info_arrsized, &in);
        in.len = 3; in.vals[0] = 1; in.vals[1] = 2; /* [1,2,0] */
        size_t used = trailrun_encode(&_info_arrsized, &in, buffer, sizeof(buffer));

        arrsized_t back;
        memset(&back, 0xAA, sizeof(back));
        TEST_ASSERT_EQUAL(SOFAB_RET_OK,
            arrsized_decode(&_info_arrsized, &back, buffer, used));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(3, back.len, "length must round-trip");
        TEST_ASSERT_EQUAL_MEMORY(&in, &back, sizeof(arrsized_t));
    }

    /* §7.3: a field whose wire type contradicts the descriptor is skipped, and
     * must NOT reset the length of the value already in the destination. */
    {
        arrsized_t msg;
        sofab_object_init(&_info_arrsized, &msg);
        msg.len = 3; msg.vals[0] = 1; msg.vals[1] = 2;
        const uint8_t wire[] = {0x00, 0x2A};   /* id 0 as an unsigned varint */
        TEST_ASSERT_EQUAL(SOFAB_RET_OK,
            arrsized_decode(&_info_arrsized, &msg, wire, sizeof(wire)));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(3, msg.len,
            "a skipped (wire-type mismatch) field must not clear the length");
        TEST_ASSERT_EQUAL_UINT32(1, msg.vals[0]);
    }

    /* over-count against the capacity is still INVALID (§7.1) */
    {
        arrsized_t msg;
        sofab_object_init(&_info_arrsized, &msg);
        const uint8_t wire[] = {0x03, 0x06, 1, 2, 3, 4, 5, 6};
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_INVALID_MSG,
            arrsized_decode(&_info_arrsized, &msg, wire, sizeof(wire)),
            "over-capacity array must be rejected");
    }

    /* a byte-wide length in front of a byte array behaves identically */
    {
        arrsized_u8_t in;
        sofab_object_init(&_info_arrsized_u8, &in);
        in.len = 2; in.vals[0] = 9;  /* [9,0] */
        size_t used = trailrun_encode(&_info_arrsized_u8, &in, buffer, sizeof(buffer));
        const uint8_t expected[] = {0x03, 0x02, 0x09, 0x00};
        TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);

        arrsized_u8_t back;
        memset(&back, 0xAA, sizeof(back));
        TEST_ASSERT_EQUAL(SOFAB_RET_OK,
            arrsized_decode(&_info_arrsized_u8, &back, buffer, used));
        TEST_ASSERT_EQUAL_UINT8(2, back.len);
        TEST_ASSERT_EQUAL_MEMORY(&in, &back, sizeof(arrsized_u8_t));
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

/*
 * A sized blob WITH a non-empty declared default: the used length is compared
 * against the default image's length and the used prefix against its bytes, like
 * a sized array (MESSAGE_SPEC §2). The default is {'H','i'}.
 */
static const blobsized_t _info_defaults_blobsized =
    { .used_len = 2, .data = { 'H', 'i' } };

static const sofab_object_descr_t _info_blobsized_with_default =
    SOFAB_OBJECT_DESCR_WITH_DEFAULTS(_info_fields_blobsized, 1, NULL, 0,
                                     &_info_defaults_blobsized);

/* Descriptor-driven, so the two blob layouts below (capacity 8 and 16) share it;
 * sofab_object_encode takes the object as a void * for the same reason. */
static size_t blobsized_def_encode (const sofab_object_descr_t *info,
                                    const void *in, uint8_t *buf, size_t n)
{
    sofab_ostream_t octx;
    sofab_ostream_init(&octx, buf, n, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, info, in), "encode failed");
    return sofab_ostream_flush(&octx);
}

static void blobsized_def_decode (const sofab_object_descr_t *info,
                                  void *out, const uint8_t *buf, size_t n)
{
    sofab_istream_t ictx;
    sofab_object_decoder_t dec;
    memset(&dec, 0, sizeof(dec));
    dec.info = info;
    dec.dst = (uint8_t *)out;
    sofab_istream_init(&ictx, sofab_object_field_cb, &dec);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_istream_feed(&ictx, buf, n),
        "decode failed");
}

static void test_object_blob_sized_default_nonempty (void)
{
    uint8_t buf[32];
    blobsized_t in, out;

    /* 1. left at its default: omitted. */
    sofab_object_init(&_info_blobsized_with_default, &in);
    TEST_ASSERT_EQUAL_UINT8(2, in.used_len);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0,
        blobsized_def_encode(&_info_blobsized_with_default, &in, buf, sizeof(buf)),
        "a sized blob equal to its non-empty default must be omitted");

    /* 2. same length, different bytes: written. */
    in.data[1] = 'o';
    size_t n = blobsized_def_encode(&_info_blobsized_with_default, &in, buf, sizeof(buf));
    const uint8_t exp_ho[] = { 0x02, 0x13, 'H', 'o' };
    TEST_ASSERT_EQUAL_size_t(sizeof(exp_ho), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp_ho, buf, n);

    /* 3. a prefix of the default -- same bytes, shorter length: written, because
     *    the length is part of the value (§3). */
    sofab_object_init(&_info_blobsized_with_default, &in);
    in.used_len = 1;                       /* "H": a prefix of the default */
    n = blobsized_def_encode(&_info_blobsized_with_default, &in, buf, sizeof(buf));
    const uint8_t exp_h[] = { 0x02, 0x0B, 'H' };
    TEST_ASSERT_EQUAL_size_t(sizeof(exp_h), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp_h, buf, n);

    /* 4. absent decodes to the default; a written value round-trips. */
    sofab_object_init(&_info_blobsized_with_default, &out);
    blobsized_def_decode(&_info_blobsized_with_default, &out, buf, 0);
    TEST_ASSERT_EQUAL_UINT8(2, out.used_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY("Hi", out.data, 2);

    sofab_object_init(&_info_blobsized_with_default, &in);
    in.data[1] = 'o';
    n = blobsized_def_encode(&_info_blobsized_with_default, &in, buf, sizeof(buf));
    sofab_object_init(&_info_blobsized_with_default, &out);
    blobsized_def_decode(&_info_blobsized_with_default, &out, buf, n);
    TEST_ASSERT_EQUAL_UINT8(2, out.used_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY("Ho", out.data, 2);
}

static void test_object_blob_sized_default_explicit_empty (void)
{
    uint8_t buf[32];
    blobsized_t in, out;
    size_t n;

    /* explicit empty over a non-empty default: written as an empty blob, and
     *    it decodes back as empty (not as the default). */
    sofab_object_init(&_info_blobsized_with_default, &in);
    in.used_len = 0;
    n = blobsized_def_encode(&_info_blobsized_with_default, &in, buf, sizeof(buf));
    const uint8_t exp_empty[] = { 0x02, 0x03 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(exp_empty), n,
        "an explicit empty blob over a non-empty default must be written");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp_empty, buf, n);

    sofab_object_init(&_info_blobsized_with_default, &out);
    blobsized_def_decode(&_info_blobsized_with_default, &out, buf, n);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, out.used_len, "empty must decode as empty");
}

static void test_object_blob_sized_no_default_image_unchanged (void)
{
    uint8_t buf[32];
    blobsized_t in;

    /* No default image: the logical default stays the empty blob. */
    sofab_object_init(&_info_blobsized, &in);
    in.data[0] = 0x55;                     /* dirty buffer, length 0 */
    TEST_ASSERT_EQUAL_size_t(0, blobsized_encode(&in, buf, sizeof(buf)));

    in.used_len = 1;
    const uint8_t exp[] = { 0x02, 0x0B, 0x55 };
    size_t n = blobsized_encode(&in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(sizeof(exp), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp, buf, n);

    /* A default image whose blob is empty behaves the same as none. */
    static const blobsized_t empty_def = { .used_len = 0 };
    static const sofab_object_descr_t info_empty_def =
        SOFAB_OBJECT_DESCR_WITH_DEFAULTS(_info_fields_blobsized, 1, NULL, 0, &empty_def);
    sofab_object_init(&info_empty_def, &in);
    TEST_ASSERT_EQUAL_size_t(0,
        blobsized_def_encode(&info_empty_def, &in, buf, sizeof(buf)));
    in.used_len = 1; in.data[0] = 0x55;
    TEST_ASSERT_EQUAL_size_t(sizeof(exp),
        blobsized_def_encode(&info_empty_def, &in, buf, sizeof(buf)));
}

/*
 * Capacity 16: the descriptor records sizeof(data) & 0xF == 0 in element_size, so
 * a sized blob that read element_size to derive its capacity would divide by zero
 * here. The default-image comparison must stay defined and byte-exact.
 */
typedef struct
{
    uint8_t used_len;
    uint8_t data[16];
} blobsized_cap16_t;

static const sofab_object_descr_field_t _info_fields_blobsized_cap16[] =
{
    SOFAB_OBJECT_FIELD_BLOB_SIZED(0, blobsized_cap16_t, data, used_len),
};

static const blobsized_cap16_t _info_defaults_blobsized_cap16 =
    { .used_len = 3, .data = { 'a', 'b', 'c' } };

static const sofab_object_descr_t _info_blobsized_cap16 =
    SOFAB_OBJECT_DESCR_WITH_DEFAULTS(_info_fields_blobsized_cap16, 1, NULL, 0,
                                     &_info_defaults_blobsized_cap16);

static void test_object_blob_sized_default_capacity_16 (void)
{
    uint8_t buf[32];
    blobsized_cap16_t in;
    size_t n;

    /* at its default: omitted, and the capacity division is well-defined. */
    sofab_object_init(&_info_blobsized_cap16, &in);
    TEST_ASSERT_EQUAL_UINT8(3, in.used_len);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0,
        blobsized_def_encode(&_info_blobsized_cap16, &in, buf, sizeof(buf)),
        "a capacity-16 blob at its default must be omitted");

    /* explicit empty over that default: written. */
    in.used_len = 0;
    n = blobsized_def_encode(&_info_blobsized_cap16, &in, buf, sizeof(buf));
    const uint8_t exp_empty[] = { 0x02, 0x03 };
    TEST_ASSERT_EQUAL_size_t(sizeof(exp_empty), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp_empty, buf, n);

    /* a full-capacity value is compared -- and carried -- over all 16 bytes, not
     * over 16 % 16 == 0 of them. Round-tripped rather than matched against a
     * literal, so the test does not restate the length varint's width. */
    blobsized_cap16_t out;
    sofab_object_init(&_info_blobsized_cap16, &in);
    for (uint8_t i = 0; i < 16; i++) in.data[i] = (uint8_t)(0x40 + i);
    in.used_len = 16;
    n = blobsized_def_encode(&_info_blobsized_cap16, &in, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN_size_t(16, n);

    sofab_object_init(&_info_blobsized_cap16, &out);
    blobsized_def_decode(&_info_blobsized_cap16, &out, buf, n);
    TEST_ASSERT_EQUAL_UINT8(16, out.used_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in.data, out.data, 16);
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
//

/* CORELIB_PLAN §4.4 through the descriptor route: the transcoder has to reach
 * the same normalizing read the hand-written sofab_istream_read_bool() reaches.
 * Before SOFAB_OBJECT_FIELDTYPE_BOOLEAN existed a descriptor could only call a
 * boolean UNSIGNED, which stored the wire value raw and rejected anything wider
 * than its destination. */

typedef struct
{
    bool flag;
    bool flags[5];
} boolmsg_t;

const sofab_object_descr_field_t _info_fields_boolmsg[] =
{
    SOFAB_OBJECT_FIELD(0, boolmsg_t, flag, SOFAB_OBJECT_FIELDTYPE_BOOLEAN),
    SOFAB_OBJECT_FIELD_ARRAY(1, boolmsg_t, flags, SOFAB_OBJECT_FIELDTYPE_ARRAY_BOOLEAN),
};

const sofab_object_descr_t _info_boolmsg =
    SOFAB_OBJECT_DESCR(_info_fields_boolmsg, 2, NULL, 0);

static sofab_ret_t boolmsg_decode (boolmsg_t *msg, const uint8_t *buf, size_t len)
{
    sofab_istream_t ctx;
    sofab_object_decoder_t dec[2];
    memset(dec, 0, sizeof(dec));
    dec[0].info = &_info_boolmsg;
    dec[0].dst = (uint8_t *)msg;
    dec[0].depth = (uint8_t)(sizeof(dec) / sizeof(dec[0]) - 1);
    sofab_istream_init(&ctx, sofab_object_field_cb, (void *)&dec[0]);
    return sofab_istream_feed(&ctx, buf, len);
}

static void test_object_boolean_tolerant_decode (void)
{
    /* id 0 boolean = 256 (two varint bytes, wider than the destination);
     * id 1 boolean[5] = 0, 1, 2, 256, 2^64-1 */
    const uint8_t buffer[] = {
        0x00, 0x80, 0x02,
        0x0B, 0x05,
            0x00,
            0x01,
            0x02,
            0x80, 0x02,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01
    };

    boolmsg_t msg;
    memset(&msg, 0, sizeof(msg));

    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, boolmsg_decode(&msg, buffer, sizeof(buffer)),
        "a boolean carries no width bound: 256 is true, not INVALID (S4.4)");

    /* Read back through a character type: the assertion is that the bool objects
     * hold a representation they are allowed to have, which a bool lvalue could
     * not have told us. */
    uint8_t stored[1 + 5];
    memcpy(&stored[0], &msg.flag, 1);
    memcpy(&stored[1], msg.flags, 5);

    const uint8_t expected[] = { 1, 0, 1, 1, 1, 1 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, stored, sizeof(expected),
        "every non-zero value is normalized to 1 on store (S4.4)");
}

static void test_object_boolean_roundtrip (void)
{
    uint8_t buffer[32];

    boolmsg_t in;
    memset(&in, 0, sizeof(in));
    in.flag = true;
    in.flags[1] = true;
    in.flags[4] = true;

    sofab_ostream_t octx;
    sofab_ostream_init(&octx, buffer, sizeof(buffer), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, &_info_boolmsg, &in), "encode failed");
    size_t used = sofab_ostream_flush(&octx);

    /* canonical on encode: true is 1, and the array rides the unsigned varint
     * array form -- id 1, type 0b011 -> 0x0B */
    const uint8_t expected[] = { 0x00, 0x01, 0x0B, 0x05, 0x00, 0x01, 0x00, 0x00, 0x01 };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer, used);

    boolmsg_t out;
    memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, boolmsg_decode(&out, buffer, used));
    TEST_ASSERT_EQUAL(0, memcmp(&in, &out, sizeof(in)));
}

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
// like an unknown id, not rejected as an error. The single-byte header packs
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

/* MESSAGE_SPEC §7.3 + §7.4: a wrapper-array id that arrives with a contradicting
 * wire type is skipped -- and skipped means the array keeps what it had. The
 * §7.4 replace-whole reset runs when the wrapper is opened, so it has to sit
 * behind the type decision; otherwise a single mistyped occurrence empties an
 * array it was never entitled to touch. */
static void test_object_wiretype_wrapper_for_scalar_keeps_array (void)
{
    _overidx_str_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    /* fill element 0 legitimately: SEQUENCE_START id 200, string id 0 = "a" */
    const uint8_t fill[] = {0xC6, 0x0C, 0x02, 0x0A, 0x61, 0x07};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, fill, sizeof(fill)));
    TEST_ASSERT_EQUAL_STRING("a", msg.arr.strings[0]);

    /* same id 200, now an unsigned varint: skipped, the array survives intact */
    const uint8_t clash[] = {0xC0, 0x0C, 0x2A};
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, clash, sizeof(clash)));
    TEST_ASSERT_EQUAL_STRING("a", msg.arr.strings[0]);
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

/*
 * Reusing one destination for several messages: the decoder writes only what the
 * wire carries, so the caller re-initialises between decodes. That has always
 * been true for a leaf field; MESSAGE_SPEC §2 makes it visible for a whole
 * sequence field, because an all-default one is now OMITTED rather than framed
 * empty -- and the §7.4 wrapper-replace reset in object.c fires when a wrapper is
 * OPENED, which an omitted field never does. sofab_object_init() is the reset,
 * and it must reach into the nested holder, not just the top-level fields.
 */
static void test_object_reuse_needs_init_between_decodes (void)
{
    _wrap_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    const uint8_t first[] = { 0xC6, 0x0C, 0x02, 0x0A, 0x41, 0x07 };  /* ["A","",""] */
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wrap_msg, &msg, first, sizeof(first)));
    TEST_ASSERT_EQUAL_STRING("A", msg.arr.strings[0]);

    /* The next message is all-default, i.e. the empty byte string (§2). Fed into
     * the re-initialised destination it must yield the declared defaults -- the
     * "A" of the previous message must not survive. */
    sofab_object_init(&_wrap_msg, &msg);
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wrap_msg, &msg, NULL, 0));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", msg.arr.strings[0],
        "sofab_object_init must reset the nested wrapper before a re-used decode");
    TEST_ASSERT_EQUAL_STRING("", msg.arr.strings[1]);
    TEST_ASSERT_EQUAL_STRING("", msg.arr.strings[2]);

    /* And the non-canonical form of that same message -- the empty wrapper frame
     * -- still resets the array by itself, without any help from the caller:
     * opening the wrapper is what triggers the §7.4 replace. */
    const uint8_t reopen[] = { 0xC6, 0x0C, 0x02, 0x0A, 0x41, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wrap_msg, &msg, reopen, sizeof(reopen)));
    TEST_ASSERT_EQUAL_STRING("A", msg.arr.strings[0]);

    const uint8_t empty_frame[] = { 0xC6, 0x0C, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_wrap_msg, &msg, empty_frame, sizeof(empty_frame)));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", msg.arr.strings[0],
        "an empty wrapper frame replaces the array whole (§7.4)");
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

    /* and it re-encodes as *nothing* — never as a stale "00 00" blob for element 0.
     * The empty array equals the field's declared default (no default declared →
     * the empty collection), so §2 omits the field; the empty wrapper C6 0C 07 the
     * input carried is a non-canonical encoding of the same value and normalizes
     * away here. */
    uint8_t out[32];
    sofab_ostream_t octx;
    sofab_ostream_init(&octx, out, sizeof(out), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&octx, &_bwrap_msg, &msg), "re-encode failed");
    size_t used = sofab_ostream_flush(&octx);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, used, "re-encoded empty wrapper must be omitted");
}

//

// MESSAGE_SPEC §2/§5.1, positional element rule: a wrapper array carries no
// length, so the decoded length is *highest present id + 1*. Nothing that carries
// it may be elided and everything else may be — an INTERIOR element equal to its
// default is omitted whatever its kind (leaf or sequence-form, leaving an id gap),
// and the element at the LAST index is always written (a leaf as its value, a
// sequence element as an empty frame).
//
// For a C holder every slot is materialized and there is no length member, so the
// last index is field_count - 1 unconditionally; an all-default holder is the one
// case that cannot be told from the empty array, and the FIELD-level ≠-default
// test omits it whole (§2).
//
// (This replaces the trailing-run elision of issue #109 / Crucible F-0030, which
// was the fixed-length reading of `count`.)
//
// holder: se_kv e[5] whose element slots are themselves SEQUENCE fields.
typedef struct { uint8_t k; uint8_t v; } _se_kv_t;
static const sofab_object_descr_field_t _se_kv_fields[] = {
    SOFAB_OBJECT_FIELD(0, _se_kv_t, k, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, _se_kv_t, v, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};
static const sofab_object_descr_t _se_kv =
    SOFAB_OBJECT_DESCR(_se_kv_fields, 2, NULL, 0);

#define _SE_CAP 5
typedef struct { _se_kv_t e[_SE_CAP]; } _se_holder_t;
static const sofab_object_descr_field_t _se_holder_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _se_holder_t, e[0], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(1, _se_holder_t, e[1], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(2, _se_holder_t, e[2], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(3, _se_holder_t, e[3], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(4, _se_holder_t, e[4], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _se_holder_nested[] = { &_se_kv };
static const sofab_object_descr_t _se_holder =   /* fixed_seq = 1 (wrapper) */
    SOFAB_OBJECT_DESCR_SEQ(_se_holder_fields, _SE_CAP, _se_holder_nested, 1);

typedef struct { _se_holder_t arr; } _se_msg_t;
static const sofab_object_descr_field_t _se_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _se_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _se_msg_nested[] = { &_se_holder };
static const sofab_object_descr_t _se_msg =
    SOFAB_OBJECT_DESCR(_se_msg_fields, 1, _se_msg_nested, 1);

static size_t _se_encode (const _se_msg_t *m, uint8_t *out, size_t cap)
{
    sofab_ostream_t o;
    sofab_ostream_init(&o, out, cap, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&o, &_se_msg, m), "struct-element encode failed");
    return sofab_ostream_flush(&o);
}

static void test_object_struct_wrapper_all_default_empty (void)
{
    /* every element all-default: a C holder cannot distinguish that from the empty
     * array, so it IS the empty array here -- the wrapper equals the holder's
     * declared default and the FIELD-level test omits it whole (§2). */
    _se_msg_t m;
    memset(&m, 0, sizeof(m));
    uint8_t out[64];
    size_t used = _se_encode(&m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, used,
        "all-default array-of-struct must be omitted, not framed empty");
}

static void test_object_struct_wrapper_last_element_framed (void)
{
    /* only element 0 carries data -> elements 1..3 are interior defaults and are
     * omitted (id gaps), while element 4 sits at the LAST index and is written as
     * an empty frame: that frame is what recovers the length. */
    _se_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.e[0].k = 1; m.arr.e[0].v = 2;
    uint8_t out[64];
    size_t used = _se_encode(&m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,                   /* wrapper open (id 200) */
        0x06, 0x00, 0x01, 0x08, 0x02, 0x07, /* e0: {k=1, v=2} */
        0x26, 0x07,                   /* e4: empty frame (last index) */
        0x07,                         /* wrapper close */
    };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "the last element must be framed even when all-default");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_struct_wrapper_interior_default_omitted (void)
{
    /* elements 0 and 2 set: the all-default interior elements 1 and 3 leave id
     * gaps -- a sequence-form element is no longer framed there -- and element 4
     * closes the array as an empty frame. */
    _se_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.e[0].k = 1; m.arr.e[0].v = 2;
    m.arr.e[2].k = 3; m.arr.e[2].v = 4;
    uint8_t out[64];
    size_t used = _se_encode(&m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x06, 0x00, 0x01, 0x08, 0x02, 0x07, /* e0 = {1,2} */
        /* e1: gap */
        0x16, 0x00, 0x03, 0x08, 0x04, 0x07, /* e2 = {3,4} */
        /* e3: gap */
        0x26, 0x07,                         /* e4 = empty frame (last index) */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "an interior all-default element must not be framed");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_struct_wrapper_leading_defaults_omitted (void)
{
    /* only the LAST element carries data -> the leading defaults 0..3 are all
     * interior and vanish; one element remains, at id 4. */
    _se_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.e[4].k = 9;
    uint8_t out[64];
    size_t used = _se_encode(&m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x26, 0x00, 0x09, 0x07,       /* e4 = {9,0} */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "leading all-default elements must leave id gaps");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

/*
 * The same positional rule on a LEAF-element holder (string[5]): the interior
 * default elements leave gaps, and the last one is written as its (empty) value
 * rather than dropped -- previously the per-field skip dropped every default leaf,
 * so ["a","","","",""] and ["a"] encoded alike.
 */
static void test_object_string_wrapper_last_element_written (void)
{
    _overidx_str_msg_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.arr.strings[0], "a");
    strcpy(m.arr.strings[2], "c");

    sofab_ostream_t o;
    uint8_t out[64];
    sofab_ostream_init(&o, out, sizeof(out), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&o, &_overidx_str_msg, &m), "string-element encode failed");
    size_t used = sofab_ostream_flush(&o);

    const uint8_t expected[] = {
        0xC6, 0x0C,                   /* wrapper open (id 200) */
        0x02, 0x0A, 0x61,             /* e0 = "a"  (string, len 1) */
        /* e1: gap */
        0x12, 0x0A, 0x63,             /* e2 = "c" */
        /* e3: gap */
        0x22, 0x02,                   /* e4 = ""   (string, len 0) */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "the last leaf element must be written even when it is the default");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_struct_wrapper_roundtrip (void)
{
    /* value equality across encode -> decode for the interior-default case:
     * both the canonical and (pre-fix) verbose forms decode identically, but
     * the value must survive the newly-elided trailing run via the N-fill. */
    _se_msg_t in;
    memset(&in, 0, sizeof(in));
    in.arr.e[0].k = 1; in.arr.e[0].v = 2;
    in.arr.e[2].k = 3; in.arr.e[2].v = 4;
    uint8_t out[64];
    size_t used = _se_encode(&in, out, sizeof(out));

    /* two nesting levels below the message (holder sequence -> element struct),
     * so the decoder needs depth 2 and three handles -- more than _overidx_decode
     * provides. */
    _se_msg_t back;
    memset(&back, 0, sizeof(back));
    struct { sofab_istream_t ctx; sofab_object_decoder_t decoder[3]; } d;
    memset(&d, 0, sizeof(d));
    d.decoder[0].info  = &_se_msg;
    d.decoder[0].dst   = (uint8_t *)&back;
    d.decoder[0].depth = 2;
    sofab_istream_init(&d.ctx, sofab_object_field_cb, &d.decoder[0]);
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_istream_feed(&d.ctx, out, used));
    TEST_ASSERT_EQUAL_MEMORY(&in, &back, sizeof(_se_msg_t));
}

//
// A SIZED wrapper holder (SOFAB_OBJECT_DESCR_SEQ_SIZED) gives the holder the
// length an un-sized one lacks. MESSAGE_SPEC §5.1 gives a wrapper array the length
// *highest present id + 1*, i.e. any of 0..N, but a C holder that materializes all
// N slots and carries no length can express only 0 (every slot default -> the
// enclosing object omits the field) and N. The lengths 1..N-1 were therefore
// unrepresentable: [{k:1}] encoded with an empty frame at the LAST slot and decoded
// back as length N, and a holder that RECEIVED a shorter array lost that length on
// re-encode. With the length member the last index becomes `length - 1` instead of
// `field_count - 1`: slots at or past the length are not written at all, the
// interior stays sparse, and the slot at `length - 1` is always written.
//
// The count sits at OFFSET 0 of the holder, not one width before the first slot.
// A holder descriptor describes the whole object, so it can anchor at the object's
// start -- and it has to, because the byte before slot 0 is not free in every
// holder: a BLOB element and a NATIVE INNER-ARRAY row are themselves SIZED and
// begin with their own used-length, so the old "one width before the slots" anchor
// read element 0's length instead and those two kinds had to stay un-sized. Offset
// 0 works for all five element kinds and needs no adjacency argument (padding
// between a narrow count and strictly-aligned slots is harmless).
//
// The tests below pin lengths 0, 1, N-1 and N for a LEAF holder (string[5]), a
// STRUCT holder (kv[5]), a BLOB holder and a NATIVE-ROW holder -- the 1..N-1 rows
// are the ones that were impossible, and the last two kinds could express no
// length at all before.
//

/* leaf holder: uint8 count + string[5]. The count is the holder's FIRST member;
 * offset 0 is where the descriptor reads it, whatever the slots' alignment
 * (SOFAB_OBJECT_ASSERT_LEN_FIRST checks it). */
#define _SZS_CAP 5
typedef struct {
    uint8_t len;
    char    s[_SZS_CAP][8];
} _szs_holder_t;
static const sofab_object_descr_field_t _szs_fields[] = {
    SOFAB_OBJECT_FIELD(0, _szs_holder_t, s[0], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(1, _szs_holder_t, s[1], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(2, _szs_holder_t, s[2], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(3, _szs_holder_t, s[3], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(4, _szs_holder_t, s[4], SOFAB_OBJECT_FIELDTYPE_STRING),
};
static const sofab_object_descr_t _szs_holder =
    SOFAB_OBJECT_DESCR_SEQ_SIZED(_szs_fields, _SZS_CAP, NULL, 0,
                                 _szs_holder_t, len);

typedef struct { _szs_holder_t arr; } _szs_msg_t;
static const sofab_object_descr_field_t _szs_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _szs_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _szs_nested[] = { &_szs_holder };
static const sofab_object_descr_t _szs_msg =
    SOFAB_OBJECT_DESCR(_szs_msg_fields, 1, _szs_nested, 1);

/* struct holder: uint32 count + kv[5] (the _se_kv element descriptor above). A
 * 4-byte count in front of 1-byte-aligned slots is fine now -- nothing is measured
 * from the slots, so the count's width is free. */
#define _SZK_CAP 5
typedef struct {
    uint32_t len;
    _se_kv_t e[_SZK_CAP];
} _szk_holder_t;
static const sofab_object_descr_field_t _szk_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _szk_holder_t, e[0], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(1, _szk_holder_t, e[1], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(2, _szk_holder_t, e[2], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(3, _szk_holder_t, e[3], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(4, _szk_holder_t, e[4], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _szk_holder_nested[] = { &_se_kv };
static const sofab_object_descr_t _szk_holder =
    SOFAB_OBJECT_DESCR_SEQ_SIZED(_szk_fields, _SZK_CAP, _szk_holder_nested, 1,
                                 _szk_holder_t, len);

/* blob holder: uint8 count + { uint8 l; uint8 b[4]; }[5]. THE POINT: every slot
 * already begins with its own used-length (SOFAB_OBJECT_FIELD_BLOB_SIZED), so the
 * byte before slot 0 is element 0's length -- the old anchor read that and this
 * holder could carry no count at all. Offset 0 is free and unambiguous. */
#define _SZB_CAP 5
typedef struct {
    uint8_t len;
    struct { uint8_t l; uint8_t b[4]; } e[_SZB_CAP];
} _szb_holder_t;
static const sofab_object_descr_field_t _szb_fields[] = {
    SOFAB_OBJECT_FIELD_BLOB_SIZED(0, _szb_holder_t, e[0].b, e[0].l),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(1, _szb_holder_t, e[1].b, e[1].l),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(2, _szb_holder_t, e[2].b, e[2].l),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(3, _szb_holder_t, e[3].b, e[3].l),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(4, _szb_holder_t, e[4].b, e[4].l),
};
static const sofab_object_descr_t _szb_holder =
    SOFAB_OBJECT_DESCR_SEQ_SIZED(_szb_fields, _SZB_CAP, NULL, 0,
                                 _szb_holder_t, len);

typedef struct { _szb_holder_t arr; } _szb_msg_t;
static const sofab_object_descr_field_t _szb_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _szb_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _szb_nested[] = { &_szb_holder };
static const sofab_object_descr_t _szb_msg =
    SOFAB_OBJECT_DESCR(_szb_msg_fields, 1, _szb_nested, 1);

/* native-row holder (array<array<u8>>): uint8 count + { uint8 l; uint8 v[3]; }[5].
 * The other kind the old anchor could not serve: each row is a SIZED array
 * (MESSAGE_SPEC §3 makes the wire count the row's length), so the row's own count
 * occupies the byte before slot 0. */
#define _SZR_CAP 5
#define _SZR_ICAP 3
typedef struct {
    uint8_t len;
    struct { uint8_t l; uint8_t v[_SZR_ICAP]; } r[_SZR_CAP];
} _szr_holder_t;
static const sofab_object_descr_field_t _szr_fields[] = {
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(0, _szr_holder_t, r[0].v, r[0].l, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(1, _szr_holder_t, r[1].v, r[1].l, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(2, _szr_holder_t, r[2].v, r[2].l, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(3, _szr_holder_t, r[3].v, r[3].l, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(4, _szr_holder_t, r[4].v, r[4].l, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
};
static const sofab_object_descr_t _szr_holder =
    SOFAB_OBJECT_DESCR_SEQ_SIZED(_szr_fields, _SZR_CAP, NULL, 0,
                                 _szr_holder_t, len);

typedef struct { _szr_holder_t arr; } _szr_msg_t;
static const sofab_object_descr_field_t _szr_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _szr_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _szr_nested[] = { &_szr_holder };
static const sofab_object_descr_t _szr_msg =
    SOFAB_OBJECT_DESCR(_szr_msg_fields, 1, _szr_nested, 1);

typedef struct { _szk_holder_t arr; } _szk_msg_t;
static const sofab_object_descr_field_t _szk_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(200, _szk_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _szk_msg_nested[] = { &_szk_holder };
static const sofab_object_descr_t _szk_msg =
    SOFAB_OBJECT_DESCR(_szk_msg_fields, 1, _szk_msg_nested, 1);

static size_t _sz_encode (const sofab_object_descr_t *info, const void *m,
                          uint8_t *out, size_t cap)
{
    sofab_ostream_t o;
    sofab_ostream_init(&o, out, cap, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        sofab_object_encode(&o, info, m), "sized-holder encode failed");
    return sofab_ostream_flush(&o);
}

/* two nesting levels (holder sequence -> element struct), so three handles */
static sofab_ret_t _szk_decode (const sofab_object_descr_t *info, void *dst,
                                const uint8_t *buf, size_t len)
{
    struct { sofab_istream_t ctx; sofab_object_decoder_t decoder[3]; } d;
    memset(&d, 0, sizeof(d));
    d.decoder[0].info  = info;
    d.decoder[0].dst   = (uint8_t *)dst;
    d.decoder[0].depth = 2;
    sofab_istream_init(&d.ctx, sofab_object_field_cb, &d.decoder[0]);
    return sofab_istream_feed(&d.ctx, buf, len);
}

/* --- leaf holder: lengths 0, 1, N-1, N ---------------------------------- */

static void test_object_sized_wrapper_leaf_len0_omitted (void)
{
    /* length 0 is the empty array: the FIELD-level ≠-default test reads the
     * holder's length (never its slots) and omits the whole wrapper (§2). The
     * slots deliberately hold junk to prove the length alone decides. */
    _szs_msg_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.arr.s[0], "junk");
    m.arr.len = 0;

    uint8_t out[64];
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szs_msg, &m, out, sizeof(out)),
        "length 0 must omit the wrapper entirely");
}

static void test_object_sized_wrapper_leaf_len1 (void)
{
    /* length 1 -- previously unrepresentable: an un-sized holder always wrote up
     * to slot 4. Slot 0 is the LAST index now, so it is written; slots 1..4 are
     * past the length and are not walked at all. */
    _szs_msg_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.arr.s[0], "a");
    strcpy(m.arr.s[3], "past");   /* past the length -- must never be written */
    m.arr.len = 1;

    uint8_t out[64];
    size_t used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,          /* wrapper open (id 200) */
        0x02, 0x0A, 0x61,    /* e0 = "a" (last index) */
        0x07,                /* wrapper close */
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_leaf_len1_default_element (void)
{
    /* [""] -- length 1 whose single element IS the element default. It sits at the
     * last index, so it is written as its (empty) value: that is what tells [""]
     * from the empty array, which the previous test's length-0 case encodes as
     * nothing at all. */
    _szs_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.len = 1;

    uint8_t out[64];
    size_t used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    const uint8_t expected[] = { 0xC6, 0x0C, 0x02, 0x02, 0x07 };  /* e0 = "" */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "[\"\"] must be an empty element at id 0, not the empty array");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_leaf_len_n_minus_1 (void)
{
    /* length 4 of capacity 5: slot 3 is the last index and is written even though
     * it is default; slots 1..2 are interior defaults (id gaps) and slot 4 is past
     * the length. */
    _szs_msg_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.arr.s[0], "a");
    strcpy(m.arr.s[4], "past");   /* past the length -- must not be written */
    m.arr.len = 4;

    uint8_t out[64];
    size_t used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x02, 0x0A, 0x61,    /* e0 = "a" */
        /* e1, e2: gaps */
        0x1A, 0x02,          /* e3 = ""  (last index) */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "length N-1 must end at id N-2 and never reach the capacity slot");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_leaf_len_n (void)
{
    /* length 5 == capacity: byte-for-byte what the un-sized holder produces, so
     * the sized descriptor is a superset and not a different encoder. */
    _szs_msg_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.arr.s[0], "a");
    strcpy(m.arr.s[2], "c");
    m.arr.len = _SZS_CAP;

    uint8_t out[64];
    size_t used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x02, 0x0A, 0x61,    /* e0 = "a" */
        0x12, 0x0A, 0x63,    /* e2 = "c" */
        0x22, 0x02,          /* e4 = ""  (last index) */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_leaf_decode_stores_length (void)
{
    /* decode records *highest present id + 1* in the length member, exactly as a
     * sized blob records its received byte length -- so the value re-encodes as the
     * length it arrived with instead of growing back to the capacity. */
    _szs_msg_t m;
    memset(&m, 0, sizeof(m));
    const uint8_t wire[] = { 0xC6, 0x0C, 0x02, 0x0A, 0x61, 0x1A, 0x02, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_szs_msg, &m, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, m.arr.len, "received length not stored");
    TEST_ASSERT_EQUAL_STRING("a", m.arr.s[0]);
    TEST_ASSERT_EQUAL_STRING("",  m.arr.s[3]);

    uint8_t out[64];
    size_t used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(wire), used, "re-encode changed the length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(wire, out, used);

    /* length 1 arrives (highest id 0) and survives a round trip too */
    memset(&m, 0, sizeof(m));
    const uint8_t one[] = { 0xC6, 0x0C, 0x02, 0x0A, 0x61, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_szs_msg, &m, one, sizeof(one)));
    TEST_ASSERT_EQUAL_UINT8(1, m.arr.len);
    used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(one), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one, out, used);
}

static void test_object_sized_wrapper_leaf_decode_empty_and_reopen (void)
{
    _szs_msg_t m;
    memset(&m, 0, sizeof(m));

    /* an empty wrapper frame is the explicit empty array: length 0, and the
     * re-encode normalizes it to the omitted (canonical) form */
    const uint8_t empty_frame[] = { 0xC6, 0x0C, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_szs_msg, &m, empty_frame, sizeof(empty_frame)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, m.arr.len, "empty frame must decode as length 0");
    uint8_t out[64];
    TEST_ASSERT_EQUAL_size_t(0, _sz_encode(&_szs_msg, &m, out, sizeof(out)));

    /* §7.4: a re-opened wrapper replaces the array whole, length included -- a
     * length-4 occurrence followed by a length-1 one must report 1, not 4. */
    const uint8_t reopen[] = {
        0xC6, 0x0C, 0x1A, 0x0A, 0x64, 0x07,   /* open, e3 = "d" (length 4), close */
        0xC6, 0x0C, 0x02, 0x0A, 0x61, 0x07,   /* re-open, e0 = "a" (length 1), close */
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_szs_msg, &m, reopen, sizeof(reopen)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, m.arr.len,
        "a §7.4 re-open must reset the holder length, not merge it");
    TEST_ASSERT_EQUAL_STRING("a", m.arr.s[0]);
    TEST_ASSERT_EQUAL_STRING("",  m.arr.s[3]);
}

/* --- struct holder: lengths 0, 1, N-1, N -------------------------------- */

static void test_object_sized_wrapper_struct_len0_omitted (void)
{
    _szk_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.e[0].k = 7;      /* junk past the length */
    m.arr.len = 0;

    uint8_t out[64];
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szk_msg, &m, out, sizeof(out)),
        "length 0 must omit the wrapper entirely");
}

static void test_object_sized_wrapper_struct_len1 (void)
{
    /* [{k:1,v:2}] -- the case the gap named: an un-sized holder wrote this value
     * with an empty frame at slot 4 and decoded it back as length 5. */
    _szk_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.e[0].k = 1; m.arr.e[0].v = 2;
    m.arr.e[2].k = 9;                    /* past the length -- never written */
    m.arr.len = 1;

    uint8_t out[64];
    size_t used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x06, 0x00, 0x01, 0x08, 0x02, 0x07,   /* e0 = {k=1, v=2} (last index) */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "[{k:1,v:2}] must be one element, with no frame at the capacity slot");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_struct_len1_default_element (void)
{
    /* [{}] -- one all-default sequence element. At the last index it keeps its
     * empty frame (that frame is the length); length 0 above writes nothing. */
    _szk_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.len = 1;

    uint8_t out[64];
    size_t used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    const uint8_t expected[] = { 0xC6, 0x0C, 0x06, 0x07, 0x07 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "an all-default last element keeps its empty frame");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_struct_len_n_minus_1 (void)
{
    _szk_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.e[0].k = 1; m.arr.e[0].v = 2;
    m.arr.e[4].k = 9;                    /* past the length -- never written */
    m.arr.len = _SZK_CAP - 1;

    uint8_t out[64];
    size_t used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x06, 0x00, 0x01, 0x08, 0x02, 0x07,   /* e0 = {1,2} */
        /* e1, e2: gaps */
        0x1E, 0x07,                           /* e3 = empty frame (last index) */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "length N-1 must frame id N-2 and never reach the capacity slot");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_struct_len_n (void)
{
    /* length == capacity reproduces the un-sized holder's bytes exactly
     * (cf. test_object_struct_wrapper_interior_default_omitted) */
    _szk_msg_t m;
    memset(&m, 0, sizeof(m));
    m.arr.e[0].k = 1; m.arr.e[0].v = 2;
    m.arr.e[2].k = 3; m.arr.e[2].v = 4;
    m.arr.len = _SZK_CAP;

    uint8_t out[64];
    size_t used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    const uint8_t expected[] = {
        0xC6, 0x0C,
        0x06, 0x00, 0x01, 0x08, 0x02, 0x07,   /* e0 = {1,2} */
        0x16, 0x00, 0x03, 0x08, 0x04, 0x07,   /* e2 = {3,4} */
        0x26, 0x07,                           /* e4 = empty frame (last index) */
        0x07,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

static void test_object_sized_wrapper_struct_decode_stores_length (void)
{
    /* the received length round-trips: [{k:1,v:2}] decodes as length 1 and
     * re-encodes as one element (an un-sized holder re-encoded it as five) */
    _szk_msg_t m;
    memset(&m, 0, sizeof(m));
    const uint8_t one[] = {
        0xC6, 0x0C, 0x06, 0x00, 0x01, 0x08, 0x02, 0x07, 0x07,
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _szk_decode(&_szk_msg, &m, one, sizeof(one)));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, m.arr.len, "received length not stored");
    TEST_ASSERT_EQUAL_UINT8(1, m.arr.e[0].k);
    TEST_ASSERT_EQUAL_UINT8(2, m.arr.e[0].v);
    TEST_ASSERT_EQUAL_UINT8(0, m.arr.e[4].k);

    uint8_t out[64];
    size_t used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(one), used, "re-encode changed the length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one, out, used);

    /* length N-1, gaps included: highest present id is 3 -> length 4 */
    memset(&m, 0, sizeof(m));
    const uint8_t four[] = {
        0xC6, 0x0C, 0x06, 0x00, 0x01, 0x08, 0x02, 0x07, 0x1E, 0x07, 0x07,
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _szk_decode(&_szk_msg, &m, four, sizeof(four)));
    TEST_ASSERT_EQUAL_UINT32(4, m.arr.len);
    used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(four), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(four, out, used);
}

static void test_object_sized_wrapper_struct_overindex_still_rejected (void)
{
    /* the capacity still bounds the array: element id 5 >= N is INVALID (§7/§7.1),
     * unchanged by the length member (which the reject never even reads) */
    _szk_msg_t m;
    memset(&m, 0, sizeof(m));
    const uint8_t over[] = { 0xC6, 0x0C, 0x2E, 0x00, 0x01, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG,
        _szk_decode(&_szk_msg, &m, over, sizeof(over)));
}

/* --- §7.3 at an ELEMENT position: skipped means NOT PRESENT ------------- */
//
// MESSAGE_SPEC §7.3: a field whose header wire type contradicts the declared type
// "MUST be skipped, exactly as a field with an unknown id is skipped" -- and an
// unknown id leaves nothing behind. At an element position that settles the §5.1
// count: the length is *highest present id + 1*, and the ids it counts are the ones
// actually consumed as elements, not the ones that merely appeared on the wire. A
// mistyped child mutates the container in no way at all -- the array is byte for
// byte what it would have been had the child never arrived.
//
// The control is the other half and must NOT be swallowed by the same test: a
// well-typed but EMPTY element (an empty frame, an empty string) IS present and
// does count. Each test below pins both rows, plus the mixed case where a mistyped
// child sits at a HIGHER id than a well-typed one and must not raise the length to
// its own id + 1 ("does not occupy its id").
//

static void test_object_sized_wrapper_leaf_mistyped_element_absent (void)
{
    _szs_msg_t m;
    uint8_t out[64];

    /* element 0 declared STRING, arrives as an unsigned varint (value 7): skipped
     * like an unknown id -> length 0, i.e. the EMPTY array, which re-encodes to
     * nothing at all (§2). */
    memset(&m, 0, sizeof(m));
    const uint8_t mistyped[] = { 0xC6, 0x0C, 0x00, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_szs_msg, &m, mistyped, sizeof(mistyped)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, m.arr.len,
        "a wire-type-mismatched element must not count toward the length");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", m.arr.s[0],
        "a skipped element must leave the slot at its element default");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szs_msg, &m, out, sizeof(out)),
        "a mistyped element must leave the container as if it never arrived");

    /* CONTROL: the same id, now a well-typed but EMPTY string. That element IS
     * present -- length 1, and the frame survives a round trip. */
    memset(&m, 0, sizeof(m));
    const uint8_t empty_elem[] = { 0xC6, 0x0C, 0x02, 0x02, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_szs_msg, &m, empty_elem, sizeof(empty_elem)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, m.arr.len,
        "an empty (but well-typed) element is present and must count");
    size_t used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(empty_elem), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(empty_elem, out, used);

    /* MIXED: a present element at id 0 and a mistyped one at id 3. The mistyped id
     * is not occupied, so the length stays 1 and the re-encode drops back to the
     * control's bytes. */
    memset(&m, 0, sizeof(m));
    const uint8_t mixed[] = { 0xC6, 0x0C, 0x02, 0x02, 0x18, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_szs_msg, &m, mixed, sizeof(mixed)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, m.arr.len,
        "a mistyped element must not occupy its id");
    TEST_ASSERT_EQUAL_STRING("", m.arr.s[3]);
    used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(empty_elem), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(empty_elem, out, used);
}

static void test_object_sized_wrapper_struct_mistyped_element_absent (void)
{
    _szk_msg_t m;
    uint8_t out[64];

    /* element 0 declared a SEQUENCE (a struct element), arrives as an unsigned
     * varint: skipped like an unknown id -> the empty array. */
    memset(&m, 0, sizeof(m));
    const uint8_t mistyped[] = { 0xC6, 0x0C, 0x00, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _szk_decode(&_szk_msg, &m, mistyped, sizeof(mistyped)));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, m.arr.len,
        "a wire-type-mismatched element must not count toward the length");
    TEST_ASSERT_EQUAL_UINT8(0, m.arr.e[0].k);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szk_msg, &m, out, sizeof(out)),
        "a mistyped element must leave the container as if it never arrived");

    /* CONTROL: the same id as a well-typed EMPTY FRAME -- [{}], length 1. */
    memset(&m, 0, sizeof(m));
    const uint8_t empty_frame[] = { 0xC6, 0x0C, 0x06, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _szk_decode(&_szk_msg, &m, empty_frame, sizeof(empty_frame)));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, m.arr.len,
        "an empty element FRAME is a present element and must count");
    size_t used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(empty_frame), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(empty_frame, out, used);

    /* MIXED: {k:1,v:2} at id 0 and a mistyped scalar at id 3 -- length 1, and the
     * re-encode is exactly the one-element form. */
    memset(&m, 0, sizeof(m));
    const uint8_t mixed[] = {
        0xC6, 0x0C, 0x06, 0x00, 0x01, 0x08, 0x02, 0x07, 0x18, 0x07, 0x07,
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _szk_decode(&_szk_msg, &m, mixed, sizeof(mixed)));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, m.arr.len,
        "a mistyped element must not occupy its id");
    TEST_ASSERT_EQUAL_UINT8(1, m.arr.e[0].k);
    TEST_ASSERT_EQUAL_UINT8(0, m.arr.e[3].k);
    const uint8_t one[] = { 0xC6, 0x0C, 0x06, 0x00, 0x01, 0x08, 0x02, 0x07, 0x07 };
    used = _sz_encode(&_szk_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(one), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one, out, used);
}

/* --- issue #117 (Crucible F-0041): §7.3 is decided BEFORE the §7 bound --- */
//
// An element header that is wrong twice over -- an id past the schema count AND a
// wire type (or fixlen subtype) that contradicts the declared element type -- must
// be SKIPPED, not rejected. §7.3: "against a schema bound, this clause wins", and a
// skipped field is not an element, so its id is not an array index and there is no
// index left for the count to bound (§7.4: "an occurrence skipped under §7.3 is not
// an occurrence for this clause"; CORELIB_PLAN §4.8: "the field was never this
// array's value").
//
// Each test pins the isolate together with its controls, because a "fix" that
// simply drops the bound passes the isolate alone:
//   - a correctly typed over-index element still REJECTS (the §7/§7.1 bound), and
//   - an in-range mistyped element is still SKIPPED (the plain §7.3 rule, which was
//     never in dispute), and
//   - the ordering relaxes exactly one window, header -> fixlen word: from the word
//     on the reject fires without waiting for a byte of payload, and the format
//     ceilings fire whatever the subtype turns out to be.
//
// Wrapper id 200 = 0xC6 0x0C; element headers pack (id << 3) | wire_type, so
// 0x40 = id 8 UNSIGNED, 0x42 = id 8 FIXLEN, 0x46 = id 8 SEQUENCE_START; a fixlen
// word packs (length << 3) | subtype, so 0x0A = 1 byte of string, 0x0B = 1 byte of
// blob. Capacity is 5 throughout, so id 8 is over-index.
//

static void test_object_overindex_mistyped_string_element_skipped (void)
{
    _overidx_str_msg_t msg;

    /* THE ISOLATE: id 8 (>= capacity 5) arriving as an UNSIGNED varint where a
     * string is declared. §7.3 skips it, so no element and no index exist. */
    memset(&msg, 0, sizeof(msg));
    const uint8_t isolate[] = { 0xC6, 0x0C, 0x40, 0x01, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, isolate, sizeof(isolate)),
        "a mistyped over-index element must be skipped (§7.3), not rejected");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", msg.arr.strings[0],
        "a skipped element must leave the array untouched");

    /* CONTROL: the same over-index id, CORRECTLY typed (string "A"). It survives
     * §7.3, so the schema bound applies -- this must keep rejecting. */
    memset(&msg, 0, sizeof(msg));
    const uint8_t welltyped[] = { 0xC6, 0x0C, 0x42, 0x0A, 0x41, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_INVALID_MSG,
        _overidx_decode(&_overidx_str_msg, &msg, welltyped, sizeof(welltyped)),
        "a well-typed over-index element must still be INVALID (§7/§7.1)");

    /* CONTROL: an IN-RANGE mistyped id (2) -- the plain §7.3 skip, unchanged. */
    memset(&msg, 0, sizeof(msg));
    const uint8_t inrange[] = { 0xC6, 0x0C, 0x10, 0x01, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, inrange, sizeof(inrange)),
        "an in-range mistyped element must be skipped");

    /* SUBTYPE, not just wire type: id 8 with the declared FIXLEN wire type but a
     * BLOB subtype where a string is declared. Gating on the header alone would
     * still wrongly reject this. */
    memset(&msg, 0, sizeof(msg));
    const uint8_t subtype[] = { 0xC6, 0x0C, 0x42, 0x0B, 0x41, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, subtype, sizeof(subtype)),
        "a contradicting fixlen SUBTYPE is a §7.3 skip too");

    /* CONTROL for it: the same subtype mismatch at an in-range id 2. */
    memset(&msg, 0, sizeof(msg));
    const uint8_t subtype_inrange[] = { 0xC6, 0x0C, 0x12, 0x0B, 0x41, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        _overidx_decode(&_overidx_str_msg, &msg, subtype_inrange, sizeof(subtype_inrange)),
        "an in-range subtype mismatch must be skipped");
    TEST_ASSERT_EQUAL_STRING("", msg.arr.strings[2]);
}

static void test_object_overindex_reject_window_unchanged (void)
{
    _overidx_str_msg_t msg;

    /* The ONE window that relaxes: the message ends between the element header and
     * its fixlen word, so the subtype -- and with it the answer to "is this an
     * element at all?" -- is not yet known. INCOMPLETE, not INVALID (§5.2; the
     * analogue of CORELIB_PLAN §4.8's ruling for the fixlen array's two words). */
    memset(&msg, 0, sizeof(msg));
    const uint8_t truncated[] = { 0xC6, 0x0C, 0x42 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_INCOMPLETE,
        _overidx_decode(&_overidx_str_msg, &msg, truncated, sizeof(truncated)),
        "a message ending before the fixlen word is INCOMPLETE");

    /* From the fixlen word ON the reject is immediate: the word says string
     * (subtype 2), length 33, and not one payload byte may be waited for. */
    memset(&msg, 0, sizeof(msg));
    const uint8_t no_payload[] = { 0xC6, 0x0C, 0x42, 0x8A, 0x02 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_INVALID_MSG,
        _overidx_decode(&_overidx_str_msg, &msg, no_payload, sizeof(no_payload)),
        "the over-index reject must not wait for the payload");

    /* The format ceilings are not subordinated: an over-index element whose own
     * metadata is malformed regardless of subtype (here an 11-byte count varint on
     * an array element) is INVALID whatever §7.3 would have done with it. */
    memset(&msg, 0, sizeof(msg));
    const uint8_t overlong[] = {
        0xC6, 0x0C, 0x43,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
    };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_INVALID_MSG,
        _overidx_decode(&_overidx_str_msg, &msg, overlong, sizeof(overlong)),
        "§7.3 subordinates the schema bound only, never a format ceiling");
}

static void test_object_overindex_mistyped_blob_and_struct_element_skipped (void)
{
    /* The test is on the declared ELEMENT type, so it must run in both directions
     * and for a sequence-typed element too. */
    _overidx_blob_msg_t bmsg;

    /* blob[5]: id 8 as an UNSIGNED varint -> skipped */
    memset(&bmsg, 0, sizeof(bmsg));
    const uint8_t b_isolate[] = { 0xC6, 0x0C, 0x40, 0x01, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _overidx_decode(&_overidx_blob_msg, &bmsg, b_isolate, sizeof(b_isolate)));

    /* blob[5]: id 8 as a STRING-subtyped fixlen -> the mirror of the string
     * holder's blob-subtyped element, and skipped for the same reason */
    memset(&bmsg, 0, sizeof(bmsg));
    const uint8_t b_string[] = { 0xC6, 0x0C, 0x42, 0x0A, 0x41, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        _overidx_decode(&_overidx_blob_msg, &bmsg, b_string, sizeof(b_string)),
        "a string-subtyped element in a blob array must be skipped");

    /* CONTROL: id 8 as a correctly typed blob -> still INVALID */
    memset(&bmsg, 0, sizeof(bmsg));
    const uint8_t b_welltyped[] = { 0xC6, 0x0C, 0x42, 0x0B, 0x41, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_INVALID_MSG,
        _overidx_decode(&_overidx_blob_msg, &bmsg, b_welltyped, sizeof(b_welltyped)),
        "a well-typed over-index blob element must still be INVALID");

    /* struct[5]: the element type is a SEQUENCE, which the header alone settles --
     * no metadata word to wait for. id 8 as an UNSIGNED varint -> skipped. */
    _szk_msg_t kmsg;
    uint8_t out[64];

    memset(&kmsg, 0, sizeof(kmsg));
    const uint8_t k_isolate[] = { 0xC6, 0x0C, 0x40, 0x01, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        _szk_decode(&_szk_msg, &kmsg, k_isolate, sizeof(k_isolate)),
        "a mistyped over-index struct element must be skipped");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, kmsg.arr.len,
        "a skipped over-index element must not count toward the length");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szk_msg, &kmsg, out, sizeof(out)),
        "the array must be exactly what it would have been without the element");

    /* CONTROL: id 8 as a correctly typed (empty) element frame -> still INVALID */
    memset(&kmsg, 0, sizeof(kmsg));
    const uint8_t k_welltyped[] = { 0xC6, 0x0C, 0x46, 0x07, 0x07 };
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_INVALID_MSG,
        _szk_decode(&_szk_msg, &kmsg, k_welltyped, sizeof(k_welltyped)),
        "a well-typed over-index struct element must still be INVALID");
}

static void test_object_overindex_skip_leaves_nothing_behind (void)
{
    /* The skip must not disturb what already arrived (§5.1 length, §7.4): a valid
     * element 0 followed by a mistyped over-index element decodes to the array of
     * just that one element -- length 1, re-encoding to the one-element wrapper. */
    _szs_msg_t m;
    uint8_t out[64];

    memset(&m, 0, sizeof(m));
    const uint8_t wire[] = { 0xC6, 0x0C, 0x02, 0x0A, 0x41, 0x40, 0x01, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_szs_msg, &m, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, m.arr.len,
        "a skipped over-index element must not raise the length");
    TEST_ASSERT_EQUAL_STRING("A", m.arr.s[0]);

    const uint8_t expected[] = { 0xC6, 0x0C, 0x02, 0x0A, 0x41, 0x07 };
    size_t used = _sz_encode(&_szs_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, used);
}

/* --- blob holder: lengths 0, 1, N-1, N ---------------------------------- */
//
// A blob element is a SIZED slot -- { l; b[]; } -- so before the count moved to
// offset 0 this holder could carry none: "one width before slot 0" is element 0's
// own used-length. It expressed exactly two lengths, 0 and N. Every row here is
// new capability. Grouped into one test per holder: on an 8 KB ATmega8 the
// sofabtest image is flash-bound, and Unity charges a name string per RUN_TEST.
//

static void test_object_sized_wrapper_blob_lengths (void)
{
    _szb_msg_t m;
    uint8_t out[64];
    size_t used;

    /* length 0 is the empty array; the slots deliberately hold junk (including a
     * non-zero used-length) to prove the holder count alone decides. */
    memset(&m, 0, sizeof(m));
    m.arr.e[0].l = 2; m.arr.e[0].b[0] = 0xDE; m.arr.e[0].b[1] = 0xAD;
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szb_msg, &m, out, sizeof(out)),
        "length 0 must omit the wrapper");

    /* length 1: [de ad]. An un-sized blob holder wrote this with an empty blob at
     * slot 4 and read it back as length 5. Slots past the length never written. */
    memset(&m, 0, sizeof(m));
    m.arr.e[0].l = 2; m.arr.e[0].b[0] = 0xDE; m.arr.e[0].b[1] = 0xAD;
    m.arr.e[3].l = 1; m.arr.e[3].b[0] = 0x99;
    m.arr.len = 1;
    const uint8_t one[] = { 0xC6, 0x0C, 0x02, 0x13, 0xDE, 0xAD, 0x07 };
    used = _sz_encode(&_szb_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(one), used, "length 1");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one, out, used);

    /* length 1 whose single element IS the element default: at the last index it
     * is written as its (empty) value, which is what tells it from length 0. */
    memset(&m, 0, sizeof(m));
    m.arr.len = 1;
    const uint8_t one_empty[] = { 0xC6, 0x0C, 0x02, 0x03, 0x07 };
    used = _sz_encode(&_szb_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(one_empty), used, "[<empty blob>]");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one_empty, out, used);

    /* length N-1: slot 3 is the last index and is written even though it is the
     * element default; slots 1..2 are interior gaps, slot 4 is past the length. */
    memset(&m, 0, sizeof(m));
    m.arr.e[0].l = 2; m.arr.e[0].b[0] = 0xDE; m.arr.e[0].b[1] = 0xAD;
    m.arr.e[4].l = 1; m.arr.e[4].b[0] = 0x99;
    m.arr.len = _SZB_CAP - 1;
    const uint8_t nm1[] = { 0xC6, 0x0C, 0x02, 0x13, 0xDE, 0xAD, 0x1A, 0x03, 0x07 };
    used = _sz_encode(&_szb_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(nm1), used, "length N-1");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(nm1, out, used);

    /* length N == capacity: byte-for-byte what the un-sized holder produced, so
     * the sized descriptor is a superset and not a different encoder. */
    memset(&m, 0, sizeof(m));
    m.arr.e[0].l = 2; m.arr.e[0].b[0] = 0xDE; m.arr.e[0].b[1] = 0xAD;
    m.arr.e[2].l = 1; m.arr.e[2].b[0] = 0x01;
    m.arr.len = _SZB_CAP;
    const uint8_t full[] = {
        0xC6, 0x0C, 0x02, 0x13, 0xDE, 0xAD, 0x12, 0x0B, 0x01, 0x22, 0x03, 0x07,
    };
    used = _sz_encode(&_szb_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(full), used, "length N");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(full, out, used);
}

static void test_object_sized_wrapper_blob_decode_stores_length (void)
{
    /* decode records *highest present id + 1* in the holder count, which is a
     * DIFFERENT member from every element's own used-length: reading the two apart
     * is exactly what the offset-0 anchor buys. */
    _szb_msg_t m;
    uint8_t out[64];
    size_t used;

    memset(&m, 0, sizeof(m));
    const uint8_t nm1[] = { 0xC6, 0x0C, 0x02, 0x13, 0xDE, 0xAD, 0x1A, 0x03, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_szb_msg, &m, nm1, sizeof(nm1)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, m.arr.len, "holder count not stored");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, m.arr.e[0].l, "element length clobbered");
    TEST_ASSERT_EQUAL_UINT8(0xDE, m.arr.e[0].b[0]);
    TEST_ASSERT_EQUAL_UINT8(0, m.arr.e[3].l);
    used = _sz_encode(&_szb_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(nm1), used, "re-encode changed the length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(nm1, out, used);

    /* length 1 arrives and stays 1 -- an un-sized holder grew it back to N here */
    memset(&m, 0, sizeof(m));
    const uint8_t one[] = { 0xC6, 0x0C, 0x02, 0x13, 0xDE, 0xAD, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_szb_msg, &m, one, sizeof(one)));
    TEST_ASSERT_EQUAL_UINT8(1, m.arr.len);
    used = _sz_encode(&_szb_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(one), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one, out, used);

    /* §7.4: a re-open replaces the array whole, holder count included */
    memset(&m, 0, sizeof(m));
    const uint8_t reopen[] = {
        0xC6, 0x0C, 0x1A, 0x0B, 0x64, 0x07,
        0xC6, 0x0C, 0x02, 0x13, 0xDE, 0xAD, 0x07,
    };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_szb_msg, &m, reopen, sizeof(reopen)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, m.arr.len, "re-open must reset the count");
    TEST_ASSERT_EQUAL_UINT8(0, m.arr.e[3].l);
}

/* --- native-row holder: lengths 0, 1, N-1, N ---------------------------- */
//
// array<array<u8>>: each row is a compact (native) array whose own element count
// sits immediately before its values, so this holder had the same blocked anchor
// as the blob one and the same two expressible lengths, 0 and N.
//

static void test_object_sized_wrapper_row_lengths (void)
{
    _szr_msg_t m;
    uint8_t out[64];
    size_t used;

    /* length 0 is the empty array of rows; the slots hold junk */
    memset(&m, 0, sizeof(m));
    m.arr.r[0].l = 2; m.arr.r[0].v[0] = 1; m.arr.r[0].v[1] = 2;
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szr_msg, &m, out, sizeof(out)),
        "length 0 must omit the wrapper");

    /* length 1: [[1,2]] of a possible five rows */
    memset(&m, 0, sizeof(m));
    m.arr.r[0].l = 2; m.arr.r[0].v[0] = 1; m.arr.r[0].v[1] = 2;
    m.arr.r[3].l = 1; m.arr.r[3].v[0] = 9;
    m.arr.len = 1;
    const uint8_t one[] = { 0xC6, 0x0C, 0x03, 0x02, 0x01, 0x02, 0x07 };
    used = _sz_encode(&_szr_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(one), used, "length 1");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one, out, used);

    /* [[]] -- one EMPTY row. At the last index it is written as an empty array;
     * the empty array OF ROWS above writes nothing at all. */
    memset(&m, 0, sizeof(m));
    m.arr.len = 1;
    const uint8_t one_empty[] = { 0xC6, 0x0C, 0x03, 0x00, 0x07 };
    used = _sz_encode(&_szr_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(one_empty), used, "[[]]");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one_empty, out, used);

    /* length N-1 */
    memset(&m, 0, sizeof(m));
    m.arr.r[0].l = 2; m.arr.r[0].v[0] = 1; m.arr.r[0].v[1] = 2;
    m.arr.r[4].l = 1; m.arr.r[4].v[0] = 9;
    m.arr.len = _SZR_CAP - 1;
    const uint8_t nm1[] = { 0xC6, 0x0C, 0x03, 0x02, 0x01, 0x02, 0x1B, 0x00, 0x07 };
    used = _sz_encode(&_szr_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(nm1), used, "length N-1");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(nm1, out, used);

    /* length N == capacity */
    memset(&m, 0, sizeof(m));
    m.arr.r[0].l = 2; m.arr.r[0].v[0] = 1; m.arr.r[0].v[1] = 2;
    m.arr.r[2].l = 1; m.arr.r[2].v[0] = 9;
    m.arr.len = _SZR_CAP;
    const uint8_t full[] = {
        0xC6, 0x0C, 0x03, 0x02, 0x01, 0x02, 0x13, 0x01, 0x09, 0x23, 0x00, 0x07,
    };
    used = _sz_encode(&_szr_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(full), used, "length N");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(full, out, used);
}

static void test_object_sized_wrapper_row_decode_stores_length (void)
{
    /* the outer row count and each row's own length are separate members and both
     * round-trip: [[1,2],[],[],[]] is four rows, not five, and row 0 keeps 2. */
    _szr_msg_t m;
    uint8_t out[64];
    size_t used;

    memset(&m, 0, sizeof(m));
    const uint8_t nm1[] = { 0xC6, 0x0C, 0x03, 0x02, 0x01, 0x02, 0x1B, 0x00, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_szr_msg, &m, nm1, sizeof(nm1)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, m.arr.len, "holder count not stored");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, m.arr.r[0].l, "row length clobbered");
    TEST_ASSERT_EQUAL_UINT8(1, m.arr.r[0].v[0]);
    TEST_ASSERT_EQUAL_UINT8(0, m.arr.r[3].l);
    used = _sz_encode(&_szr_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(nm1), used, "re-encode changed the length");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(nm1, out, used);

    /* one row arrives and stays one row (an un-sized holder re-encoded five) */
    memset(&m, 0, sizeof(m));
    const uint8_t one[] = { 0xC6, 0x0C, 0x03, 0x02, 0x01, 0x02, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _overidx_decode(&_szr_msg, &m, one, sizeof(one)));
    TEST_ASSERT_EQUAL_UINT8(1, m.arr.len);
    used = _sz_encode(&_szr_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(one), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one, out, used);
}

static void test_object_sized_wrapper_init_clears_length (void)
{
    /* sofab_object_init must reach the holder count, which no field descriptor
     * covers (it sits at offset 0, ahead of every slot) -- the same blind spot the
     * sized blob had in issue #106. Without it a re-used destination re-encodes a
     * stale length's worth of elements. */
    _szs_msg_t m;
    memset(&m, 0xFF, sizeof(m));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_object_init(&_szs_msg, &m));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, m.arr.len, "init must clear the holder length");

    uint8_t out[64];
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _sz_encode(&_szs_msg, &m, out, sizeof(out)),
        "an initialized sized holder is the empty array");

    _szk_msg_t k;
    memset(&k, 0xFF, sizeof(k));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_object_init(&_szk_msg, &k));
    TEST_ASSERT_EQUAL_UINT32(0, k.arr.len);
    TEST_ASSERT_EQUAL_size_t(0, _sz_encode(&_szk_msg, &k, out, sizeof(out)));

    /* the two kinds that could not carry a count before: init must clear the holder
     * count AND each slot's own used-length, which live at different addresses. */
    _szb_msg_t b;
    memset(&b, 0xFF, sizeof(b));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_object_init(&_szb_msg, &b));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, b.arr.len, "init must clear the blob holder count");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, b.arr.e[0].l, "init must clear each blob slot's length");
    TEST_ASSERT_EQUAL_size_t(0, _sz_encode(&_szb_msg, &b, out, sizeof(out)));

    _szr_msg_t r;
    memset(&r, 0xFF, sizeof(r));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_object_init(&_szr_msg, &r));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, r.arr.len, "init must clear the row holder count");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, r.arr.r[0].l, "init must clear each row's own length");
    TEST_ASSERT_EQUAL_size_t(0, _sz_encode(&_szr_msg, &r, out, sizeof(out)));
}

//

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) \
    && !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
/* ---- tagged union (SOFAB_OBJECT_DESCR_UNION) ----------------------------------
 * shape: union { num u16 @0, name string[16] @1, pt struct{x=7,y i32} @2 },
 * default_id 2 -- a SEQUENCE option, so init has to seed it through its own
 * descriptor, and the image is the tag alone (a prefix, never a full image).
 * list: array<union { a u8 @0, b fp32 @1 }, count 4>, default_id 0 with an
 * all-zero option: no image at all (NULL = tag 0). */
typedef struct { int32_t x; int32_t y; } _un_pt_t;
typedef struct {
    sofab_object_descr_id_t which;
    union { uint16_t num; char name[17]; _un_pt_t pt; } u;
} _un_shape_t;
typedef struct { sofab_object_descr_id_t which; union { uint8_t a; float b; } u; } _un_le_t;
typedef struct { uint8_t len; _un_le_t e[4]; } _un_list_t;
typedef struct { _un_shape_t shape; _un_list_t list; } _un_msg_t;

static const sofab_object_descr_field_t _un_pt_fields[] = {
    SOFAB_OBJECT_FIELD(0, _un_pt_t, x, SOFAB_OBJECT_FIELDTYPE_SIGNED),
    SOFAB_OBJECT_FIELD(1, _un_pt_t, y, SOFAB_OBJECT_FIELDTYPE_SIGNED),
};
static const _un_pt_t _un_pt_default = { .x = 7 };
static const sofab_object_descr_t _un_pt = SOFAB_OBJECT_DESCR_WITH_DEFAULTS(
    _un_pt_fields, 2, NULL, 0, &_un_pt_default);
static const sofab_object_descr_field_t _un_shape_fields[] = {
    SOFAB_OBJECT_FIELD(0, _un_shape_t, u.num, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, _un_shape_t, u.name, SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD_SEQUENCE(2, _un_shape_t, u.pt, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _un_shape_nested[] = { &_un_pt };
/* the tag-only prefix image: default_id names a sequence option */
static const struct { sofab_object_descr_id_t which; } _un_shape_default = { 2 };
static const sofab_object_descr_t _un_shape = SOFAB_OBJECT_DESCR_UNION(
    _un_shape_fields, 3, _un_shape_nested, 1, &_un_shape_default, _un_shape_t, which);
static const sofab_object_descr_field_t _un_le_fields[] = {
    SOFAB_OBJECT_FIELD(0, _un_le_t, u.a, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, _un_le_t, u.b, SOFAB_OBJECT_FIELDTYPE_FP32),
};
/* NULL image, through the macro: default_id 0 and an all-zero option. A
 * regression of the macro to an address-of form fails to compile here. */
static const sofab_object_descr_t _un_le = SOFAB_OBJECT_DESCR_UNION(
    _un_le_fields, 2, NULL, 0, NULL, _un_le_t, which);
static const sofab_object_descr_field_t _un_list_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _un_list_t, e[0], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(1, _un_list_t, e[1], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(2, _un_list_t, e[2], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(3, _un_list_t, e[3], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _un_list_nested[] = { &_un_le };
static const sofab_object_descr_t _un_list = SOFAB_OBJECT_DESCR_SEQ_SIZED(
    _un_list_fields, 4, _un_list_nested, 1, _un_list_t, len);
static const sofab_object_descr_field_t _un_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _un_msg_t, shape, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(1, _un_msg_t, list, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 1),
};
static const sofab_object_descr_t *const _un_msg_nested[] = { &_un_shape, &_un_list };
static const sofab_object_descr_t _un_msg = SOFAB_OBJECT_DESCR(_un_msg_fields, 2, _un_msg_nested, 2);

static size_t _un_encode_any (const sofab_object_descr_t *info, const void *m,
                              uint8_t *out, size_t cap)
{
    sofab_ostream_t o;
    sofab_ostream_init(&o, out, cap, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_object_encode(&o, info, m),
                              "union encode failed");
    return sofab_ostream_flush(&o);
}

/* init + decode into a destination scribbled first, so nothing survives from
 * before init; `levels` decoder handles below the root */
static sofab_ret_t _un_decode_any (const sofab_object_descr_t *info, void *m, size_t size,
                                   const uint8_t *buf, size_t len)
{
    sofab_istream_t is;
    sofab_object_decoder_t d[5];
    memset(d, 0, sizeof(d));
    memset(m, 0xA5, size);
    sofab_object_init(info, m);
    d[0].info = info;
    d[0].dst = (uint8_t *)m;
    d[0].depth = 4;
    sofab_istream_init(&is, sofab_object_field_cb, &d[0]);
    return sofab_istream_feed(&is, buf, len);
}

static size_t _un_encode (const _un_msg_t *m, uint8_t *out, size_t cap)
{
    return _un_encode_any(&_un_msg, m, out, cap);
}

static sofab_ret_t _un_decode (_un_msg_t *m, const uint8_t *buf, size_t len)
{
    return _un_decode_any(&_un_msg, m, sizeof(*m), buf, len);
}

static void test_object_union_init_holds_default_id (void)
{
    _un_msg_t m;
    uint8_t out[64];
    memset(&m, 0xA5, sizeof(m));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_object_init(&_un_msg, &m));
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, m.shape.which, "init must hold default_id");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(7, m.shape.u.pt.x,
        "a sequence default_id option is seeded through its own descriptor");
    TEST_ASSERT_EQUAL_INT32(0, m.shape.u.pt.y);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, _un_encode(&m, out, sizeof(out)),
        "an all-default message is zero bytes (§2)");
}

static void test_object_union_null_image_holds_id_0 (void)
{
    _un_le_t e;
    uint8_t out[16];
    TEST_ASSERT_NULL(_un_le.default_values);
    memset(&e, 0xA5, sizeof(e));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_object_init(&_un_le, &e));
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, e.which, "a NULL image means tag 0");
    TEST_ASSERT_EQUAL_UINT8(0, e.u.a);
    TEST_ASSERT_EQUAL_size_t(0, _un_encode_any(&_un_le, &e, out, sizeof(out)));

    /* option 1 held at its own (zero) default: forced, written as its value */
    e.which = 1; e.u.b = 0.0f;
    static const uint8_t b0[] = { 0x0A, 0x20, 0x00, 0x00, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_size_t(sizeof(b0), _un_encode_any(&_un_le, &e, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(b0, out, sizeof(b0));
}

static void test_object_union_encodes_only_the_held_option (void)
{
    _un_msg_t m;
    uint8_t out[64];
    sofab_object_init(&_un_msg, &m);

    m.shape.which = 0; m.shape.u.num = 7;
    static const uint8_t num7[] = { 0x06, 0x00, 0x07, 0x07 };
    TEST_ASSERT_EQUAL_size_t(sizeof(num7), _un_encode(&m, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(num7, out, sizeof(num7));

    /* switching overlays the storage; the frame still carries ONE child */
    m.shape.which = 1; strcpy(m.shape.u.name, "x");
    static const uint8_t namex[] = { 0x06, 0x0A, 0x0A, 0x78, 0x07 };
    TEST_ASSERT_EQUAL_size_t(sizeof(namex), _un_encode(&m, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(namex, out, sizeof(namex));

    /* the held option is not default_id, so at its own default (the empty
     * string) it is still WRITTEN: omitting it would read back as default_id */
    m.shape.u.name[0] = '\0';
    static const uint8_t name0[] = { 0x06, 0x0A, 0x02, 0x07 };
    TEST_ASSERT_EQUAL_size_t(sizeof(name0), _un_encode(&m, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(name0, out, sizeof(name0));
}

static void test_object_union_last_option_wins (void)
{
    _un_msg_t m;
    uint8_t out[64];
    /* one frame, three options: num=7, name="x", pt{x=1} */
    static const uint8_t three[] = { 0x06, 0x00, 0x07, 0x0A, 0x0A, 0x78,
                                     0x16, 0x01, 0x02, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&m, three, sizeof(three)));
    TEST_ASSERT_EQUAL_UINT(2, m.shape.which);
    TEST_ASSERT_EQUAL_INT32(1, m.shape.u.pt.x);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(0, m.shape.u.pt.y, "the pt option must start from its default");
    static const uint8_t canon[] = { 0x06, 0x16, 0x01, 0x02, 0x07, 0x07 };
    TEST_ASSERT_EQUAL_size_t(sizeof(canon), _un_encode(&m, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(canon, out, sizeof(canon));

    /* a re-opened union frame with a different option replaces it, and the
     * canonical re-encode carries that one option only */
    static const uint8_t reopen[] = { 0x06, 0x00, 0x07, 0x07, 0x06, 0x0A, 0x0A, 0x78, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&m, reopen, sizeof(reopen)));
    TEST_ASSERT_EQUAL_UINT(1, m.shape.which);
    TEST_ASSERT_EQUAL_STRING("x", m.shape.u.name);
    static const uint8_t canonx[] = { 0x06, 0x0A, 0x0A, 0x78, 0x07 };
    TEST_ASSERT_EQUAL_size_t(sizeof(canonx), _un_encode(&m, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(canonx, out, sizeof(canonx));
}

static void test_object_union_same_option_merges (void)
{
    _un_msg_t m;
    /* num=5 first, then pt{x=1} and pt{y=3} in two re-opened frames: the switch
     * to pt seeds it from its default (no bytes of num leak into it), and the
     * second pt merges into the first (§7.4 per field id) */
    static const uint8_t bytes[] = { 0x06, 0x00, 0x05, 0x07,
                                     0x06, 0x16, 0x01, 0x02, 0x07, 0x07,
                                     0x06, 0x16, 0x09, 0x06, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&m, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(2, m.shape.which);
    TEST_ASSERT_EQUAL_INT32(1, m.shape.u.pt.x);
    TEST_ASSERT_EQUAL_INT32(3, m.shape.u.pt.y);
}

static void test_object_union_away_and_back_starts_from_default (void)
{
    _un_msg_t m;
    /* pt{x=1}, then num=5, then pt{y=2} in three frames: coming back to pt must
     * start it from ITS default (x=7), not from what it held before the switch */
    static const uint8_t bytes[] = { 0x06, 0x16, 0x01, 0x02, 0x07, 0x07,
                                     0x06, 0x00, 0x05, 0x07,
                                     0x06, 0x16, 0x09, 0x04, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&m, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(2, m.shape.which);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(7, m.shape.u.pt.x, "discarded state must not survive");
    TEST_ASSERT_EQUAL_INT32(2, m.shape.u.pt.y);
}

static void test_object_union_skipped_option_does_not_switch (void)
{
    _un_msg_t m;
    /* num=7, then id 1 as a signed varint -- `name` is a string, so §7.3 skips
     * it, and a skipped occurrence is no occurrence: num stays held */
    static const uint8_t bytes[] = { 0x06, 0x00, 0x07, 0x09, 0x05, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&m, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(0, m.shape.which);
    TEST_ASSERT_EQUAL_UINT16(7, m.shape.u.num);

    /* name="ab", then a BLOB at the string option's id: the subtype contradicts
     * (§7.3), so neither a switch nor a write into the held string happens */
    static const uint8_t blob_at_string[] = { 0x06, 0x0A, 0x12, 0x61, 0x62,
                                              0x0A, 0x0B, 0x01, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&m, blob_at_string, sizeof(blob_at_string)));
    TEST_ASSERT_EQUAL_UINT(1, m.shape.which);
    TEST_ASSERT_EQUAL_STRING("ab", m.shape.u.name);

    /* an unknown id inside the frame does not switch either */
    static const uint8_t unknown[] = { 0x06, 0x00, 0x09, 0x98, 0x06, 0x01, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&m, unknown, sizeof(unknown)));
    TEST_ASSERT_EQUAL_UINT(0, m.shape.which);
    TEST_ASSERT_EQUAL_UINT16(9, m.shape.u.num);
}

static void test_object_union_array_elements_roundtrip (void)
{
    _un_msg_t m, d;
    uint8_t out[64];
    sofab_object_init(&_un_msg, &m);
    m.list.len = 3;
    m.list.e[0].which = 0; m.list.e[0].u.a = 5;
    m.list.e[1].which = 1; m.list.e[1].u.b = 1.5f;   /* e[2] stays default */
    size_t n = _un_encode(&m, out, sizeof(out));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode(&d, out, n));
    TEST_ASSERT_EQUAL_UINT8(3, d.list.len);
    TEST_ASSERT_EQUAL_UINT(0, d.list.e[0].which);
    TEST_ASSERT_EQUAL_UINT8(5, d.list.e[0].u.a);
    TEST_ASSERT_EQUAL_UINT(1, d.list.e[1].which);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, d.list.e[1].u.b);
    TEST_ASSERT_EQUAL_UINT(0, d.list.e[2].which);
    TEST_ASSERT_EQUAL_UINT8(0, d.list.e[2].u.a);
}

/* ---- forced write: every option kind held at its own default ------------------
 * uf: union { num u16 = 5 @0 (default_id), s string[8] @1, bl blob[4] @2,
 *             arr u16[4] @3, fa fp32[2] @4, box struct{z=3} @5,
 *             strs string[4][2] @6, inner union{a u8 @0, b i8 = -2 @1} @7,
 *             f fp32 @8 }
 * default_id 0 with a NON-ZERO leaf default: the image is the tag plus that one
 * option (a prefix, never sizeof(_uf_t)) -- run under ASan, a read past it is a
 * global-buffer-overflow. r: union { nu = the inner union @0 (default_id),
 * ar u8[2] @1 }: default_id 0 on a sequence option, so no image. */
typedef struct { uint8_t z; } _uf_box_t;
typedef struct { sofab_object_descr_id_t which; union { uint8_t a; int8_t b; } u; } _uf_inner_t;
typedef struct { uint8_t len; char items[2][5]; } _uf_strs_t;
typedef struct {
    sofab_object_descr_id_t which;
    union {
        uint16_t num;
        char s[9];
        struct { uint8_t len; uint8_t data[4]; } bl;
        struct { uint16_t len; uint16_t items[4]; } arr;
        struct { uint32_t len; float items[2]; } fa;
        _uf_box_t box;
        _uf_strs_t strs;
        _uf_inner_t inner;
        float f;
    } u;
} _uf_t;
typedef struct {
    sofab_object_descr_id_t which;
    union { _uf_inner_t nu; struct { uint8_t len; uint8_t items[2]; } ar; } u;
} _ur_t;
typedef struct { _uf_t f; _ur_t r; uint8_t w; } _uf_msg_t;

static const sofab_object_descr_field_t _uf_box_fields[] = {
    SOFAB_OBJECT_FIELD(0, _uf_box_t, z, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};
static const _uf_box_t _uf_box_default = { .z = 3 };
static const sofab_object_descr_t _uf_box = SOFAB_OBJECT_DESCR_WITH_DEFAULTS(
    _uf_box_fields, 1, NULL, 0, &_uf_box_default);
static const sofab_object_descr_field_t _uf_inner_fields[] = {
    SOFAB_OBJECT_FIELD(0, _uf_inner_t, u.a, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, _uf_inner_t, u.b, SOFAB_OBJECT_FIELDTYPE_SIGNED),
};
typedef struct { sofab_object_descr_id_t which; union { int8_t b; uint8_t _align; } u; } _uf_inner_img_t;
typedef char _uf_inner_img_ok[(offsetof(_uf_inner_img_t, u) == offsetof(_uf_inner_t, u)) ? 1 : -1];
static const _uf_inner_img_t _uf_inner_default = { .which = 1, .u.b = -2 };
static const sofab_object_descr_t _uf_inner = SOFAB_OBJECT_DESCR_UNION(
    _uf_inner_fields, 2, NULL, 0, &_uf_inner_default, _uf_inner_t, which);
static const sofab_object_descr_field_t _uf_strs_fields[] = {
    SOFAB_OBJECT_FIELD(0, _uf_strs_t, items[0], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(1, _uf_strs_t, items[1], SOFAB_OBJECT_FIELDTYPE_STRING),
};
static const sofab_object_descr_t _uf_strs = SOFAB_OBJECT_DESCR_SEQ_SIZED(
    _uf_strs_fields, 2, NULL, 0, _uf_strs_t, len);
static const sofab_object_descr_field_t _uf_fields[] = {
    SOFAB_OBJECT_FIELD(0, _uf_t, u.num, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(1, _uf_t, u.s, SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(2, _uf_t, u.bl.data, u.bl.len),
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(3, _uf_t, u.arr.items, u.arr.len, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(4, _uf_t, u.fa.items, u.fa.len, SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32),
    SOFAB_OBJECT_FIELD_SEQUENCE(5, _uf_t, u.box, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(6, _uf_t, u.strs, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 1),
    SOFAB_OBJECT_FIELD_SEQUENCE(7, _uf_t, u.inner, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 2),
    SOFAB_OBJECT_FIELD(8, _uf_t, u.f, SOFAB_OBJECT_FIELDTYPE_FP32),
};
static const sofab_object_descr_t *const _uf_nested[] = { &_uf_box, &_uf_strs, &_uf_inner };
/* the tag + default-option prefix image: _align gives its union the alignment
 * of _uf_t's option union, so the option sits at the same offset in both */
typedef struct { sofab_object_descr_id_t which; union { uint16_t num; uint32_t _align; } u; } _uf_img_t;
typedef char _uf_img_ok[(offsetof(_uf_img_t, u) == offsetof(_uf_t, u)) ? 1 : -1];
static const _uf_img_t _uf_default = { .which = 0, .u.num = 5 };
static const sofab_object_descr_t _uf = SOFAB_OBJECT_DESCR_UNION(
    _uf_fields, 9, _uf_nested, 3, &_uf_default, _uf_t, which);
static const sofab_object_descr_field_t _ur_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _ur_t, u.nu, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_ARRAY_SIZED(1, _ur_t, u.ar.items, u.ar.len, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
};
static const sofab_object_descr_t *const _ur_nested[] = { &_uf_inner };
static const sofab_object_descr_t _ur = SOFAB_OBJECT_DESCR_UNION(
    _ur_fields, 2, _ur_nested, 1, NULL, _ur_t, which);
static const sofab_object_descr_field_t _uf_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _uf_msg_t, f, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(1, _uf_msg_t, r, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 1),
    SOFAB_OBJECT_FIELD(2, _uf_msg_t, w, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};
static const sofab_object_descr_t *const _uf_msg_nested[] = { &_uf, &_ur };
static const sofab_object_descr_t _uf_msg = SOFAB_OBJECT_DESCR(_uf_msg_fields, 3, _uf_msg_nested, 2);

/* Select option `id` of m->f at its own default, the way a caller does: the tag,
 * then the value (a sequence option through its own descriptor). */
static void _uf_select (_uf_msg_t *m, sofab_object_descr_id_t id)
{
    m->f.which = id;
    memset(&m->f.u, 0xA5, sizeof(m->f.u));   /* the previous option's bytes */
    switch (id)
    {
        case 0: m->f.u.num = 5; break;
        case 1: m->f.u.s[0] = '\0'; break;
        case 2: m->f.u.bl.len = 0; break;
        case 3: m->f.u.arr.len = 0; break;
        case 4: m->f.u.fa.len = 0; break;
        case 5: sofab_object_init(&_uf_box, &m->f.u.box); break;
        case 6: sofab_object_init(&_uf_strs, &m->f.u.strs); break;
        case 7: sofab_object_init(&_uf_inner, &m->f.u.inner); break;
        default: m->f.u.f = 0.0f; break;
    }
}

/* encode m, compare, then decode (one-shot and one byte per feed) and re-encode:
 * the forced form must come back as the SAME option and re-encode byte-exact */
static void _uf_check (const _uf_msg_t *m, const uint8_t *want, size_t want_len, const char *what)
{
    uint8_t out[64], again[64];
    _uf_msg_t d;
    size_t n = _un_encode_any(&_uf_msg, m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(want_len, n, what);
    if (n) TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(want, out, want_len, what);

    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK,
        _un_decode_any(&_uf_msg, &d, sizeof(d), out, n), what);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(m->f.which, d.f.which, what);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(m->r.which, d.r.which, what);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(n, _un_encode_any(&_uf_msg, &d, again, sizeof(again)), what);
    if (n) TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(out, again, n, what);

    {
        sofab_istream_t is;
        sofab_object_decoder_t dd[5];
        memset(dd, 0, sizeof(dd));
        memset(&d, 0xA5, sizeof(d));
        sofab_object_init(&_uf_msg, &d);
        dd[0].info = &_uf_msg; dd[0].dst = (uint8_t *)&d; dd[0].depth = 4;
        sofab_istream_init(&is, sofab_object_field_cb, &dd[0]);
        for (size_t i = 0; i < n; i++) (void)sofab_istream_feed(&is, out + i, 1);
        TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_OK, sofab_istream_feed(&is, NULL, 0), what);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(m->f.which, d.f.which, what);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(n, _un_encode_any(&_uf_msg, &d, again, sizeof(again)), what);
        if (n) TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(out, again, n, what);
    }
}

static void test_object_union_held_option_at_own_default_is_written (void)
{
    _uf_msg_t m;
    memset(&m, 0xA5, sizeof(m));
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, sofab_object_init(&_uf_msg, &m));
    TEST_ASSERT_EQUAL_UINT(0, m.f.which);
    TEST_ASSERT_EQUAL_UINT16(5, m.f.u.num);

    /* default_id at its default: the union is default, the message is empty */
    _uf_check(&m, NULL, 0, "default_id at its default is omitted");

    static const uint8_t num6[] = { 0x06, 0x00, 0x06, 0x07 };
    m.f.u.num = 6;
    _uf_check(&m, num6, sizeof(num6), "default_id set is framed normally");

    static const uint8_t s0[] = { 0x06, 0x0A, 0x02, 0x07 };
    _uf_select(&m, 1);
    _uf_check(&m, s0, sizeof(s0), "empty string option is written");

    static const uint8_t bl0[] = { 0x06, 0x12, 0x03, 0x07 };
    _uf_select(&m, 2);
    _uf_check(&m, bl0, sizeof(bl0), "empty sized blob option is written");

    static const uint8_t arr0[] = { 0x06, 0x1B, 0x00, 0x07 };
    _uf_select(&m, 3);
    _uf_check(&m, arr0, sizeof(arr0), "count-0 sized array option is written");

    /* a count-0 fp array keeps its fixlen_word (CORELIB_PLAN §4.8) */
    static const uint8_t fa0[] = { 0x06, 0x25, 0x00, 0x20, 0x07 };
    _uf_select(&m, 4);
    _uf_check(&m, fa0, sizeof(fa0), "count-0 fp32 array option keeps its fixlen_word");

    /* a struct option at its default (z=3, non-zero): a present, EMPTY frame */
    static const uint8_t box0[] = { 0x06, 0x2E, 0x07, 0x07 };
    _uf_select(&m, 5);
    TEST_ASSERT_EQUAL_UINT8(3, m.f.u.box.z);
    _uf_check(&m, box0, sizeof(box0), "struct option at its default is an empty frame");

    static const uint8_t strs0[] = { 0x06, 0x36, 0x07, 0x07 };
    _uf_select(&m, 6);
    _uf_check(&m, strs0, sizeof(strs0), "empty wrapper option is an empty frame");

    /* a union option holding ITS default_id at its default: an empty frame */
    static const uint8_t inner0[] = { 0x06, 0x3E, 0x07, 0x07 };
    _uf_select(&m, 7);
    TEST_ASSERT_EQUAL_UINT(1, m.f.u.inner.which);
    TEST_ASSERT_EQUAL_INT8(-2, m.f.u.inner.u.b);
    _uf_check(&m, inner0, sizeof(inner0), "union option at its default is an empty frame");

    /* ... and holding its non-default option at 0: forced one level down too */
    static const uint8_t innera[] = { 0x06, 0x3E, 0x00, 0x00, 0x07, 0x07 };
    m.f.u.inner.which = 0; m.f.u.inner.u.a = 0;
    _uf_check(&m, innera, sizeof(innera), "inner non-default option at 0 is written");

    static const uint8_t f0[] = { 0x06, 0x42, 0x20, 0x00, 0x00, 0x00, 0x00, 0x07 };
    _uf_select(&m, 8);
    _uf_check(&m, f0, sizeof(f0), "fp32 option at 0 is written");
}

static void test_object_union_nested_union_not_default_when_forced (void)
{
    _uf_msg_t m;
    uint8_t out[32];
    sofab_object_init(&_uf_msg, &m);
    TEST_ASSERT_EQUAL_UINT(0, m.r.which);
    TEST_ASSERT_EQUAL_UINT(1, m.r.u.nu.which);
    TEST_ASSERT_EQUAL_INT8(-2, m.r.u.nu.u.b);
    TEST_ASSERT_EQUAL_size_t(0, _un_encode_any(&_uf_msg, &m, out, sizeof(out)));

    /* r holds its default_id nu, but nu holds its NON-default option a = 0: r is
     * therefore not default although the value it holds is all zeros, and r must
     * be framed -- omitting it decodes as nu = {b: -2} */
    m.r.u.nu.which = 0; m.r.u.nu.u.a = 0;
    static const uint8_t want[] = { 0x0E, 0x06, 0x00, 0x00, 0x07, 0x07 };
    _uf_check(&m, want, sizeof(want), "union D holding its non-D option at 0");

    /* the same with a = 254: its bytes equal b's default (-2) in the overlay, so
     * a test that compared the held option against the image instead of asking
     * "is it default_id?" would call it default and drop r */
    m.r.u.nu.u.a = 0xFE;
    static const uint8_t want254[] = { 0x0E, 0x06, 0x00, 0xFE, 0x01, 0x07, 0x07 };
    _uf_check(&m, want254, sizeof(want254), "union D holding its non-D option at D's bytes");

    /* the empty frame of nu is nu at ITS default (b=-2), not its first option */
    _uf_msg_t d;
    static const uint8_t empty_nu[] = { 0x0E, 0x06, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode_any(&_uf_msg, &d, sizeof(d), empty_nu, sizeof(empty_nu)));
    TEST_ASSERT_EQUAL_UINT(0, d.r.which);
    TEST_ASSERT_EQUAL_UINT(1, d.r.u.nu.which);
    TEST_ASSERT_EQUAL_INT8(-2, d.r.u.nu.u.b);
    TEST_ASSERT_EQUAL_size_t(0, _un_encode_any(&_uf_msg, &d, out, sizeof(out)));

    /* the other option, a count-0 array, beside a union default_id */
    m.r.which = 1; m.r.u.ar.len = 0;
    static const uint8_t ar0[] = { 0x0E, 0x0B, 0x00, 0x07 };
    _uf_check(&m, ar0, sizeof(ar0), "array option at count 0 beside a union D");
}

static void test_object_union_stray_tag_holds_nothing (void)
{
    _uf_msg_t m;
    uint8_t out[32];
    sofab_object_init(&_uf_msg, &m);

    /* a tag that names no option (a caller's stray write) holds nothing: no
     * option is written and the union reads as default, so its parent omits
     * it -- which a receiver decodes as default_id at its default */
    m.f.which = 99;
    m.r.which = 99;
    TEST_ASSERT_EQUAL_size_t(0, _un_encode_any(&_uf_msg, &m, out, sizeof(out)));
}

static void test_object_union_prefix_image (void)
{
    _uf_msg_t m, d;
    uint8_t out[32];
    /* the image covers the tag and the default option only */
    TEST_ASSERT_TRUE(sizeof(_uf_img_t) < sizeof(_uf_t));
    TEST_ASSERT_TRUE(sizeof(_un_shape_default) < sizeof(_un_shape_t));

    /* every read of the image happens here: init seeds num=5, the ≠-default test
     * compares a held num against it, and a decode switching back to num reads
     * nothing from it (a leaf is overwritten whole) */
    memset(&m, 0xA5, sizeof(m));
    sofab_object_init(&_uf_msg, &m);
    TEST_ASSERT_EQUAL_UINT16(5, m.f.u.num);
    TEST_ASSERT_EQUAL_size_t(0, _un_encode_any(&_uf_msg, &m, out, sizeof(out)));
    m.f.u.num = 0;
    static const uint8_t num0[] = { 0x06, 0x00, 0x00, 0x07 };
    TEST_ASSERT_EQUAL_size_t(sizeof(num0), _un_encode_any(&_uf_msg, &m, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(num0, out, sizeof(num0));

    /* s="ab", then num=5 in one frame: the switch back to default_id at its
     * default decodes, and re-encodes as nothing */
    static const uint8_t bytes[] = { 0x06, 0x0A, 0x12, 0x61, 0x62, 0x00, 0x05, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode_any(&_uf_msg, &d, sizeof(d), bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(0, d.f.which);
    TEST_ASSERT_EQUAL_UINT16(5, d.f.u.num);
    TEST_ASSERT_EQUAL_size_t(0, _un_encode_any(&_uf_msg, &d, out, sizeof(out)));

    /* the tag-only image of shape (default_id 2, a struct option) */
    _un_msg_t s;
    memset(&s, 0xA5, sizeof(s));
    sofab_object_init(&_un_msg, &s);
    TEST_ASSERT_EQUAL_UINT(2, s.shape.which);
    TEST_ASSERT_EQUAL_size_t(0, _un_encode(&s, out, sizeof(out)));
}

/* ---- array of unions, default_id on the NON-first option ----------------------
 * v: array<union { i i32 @0, s string[8] @1 }, count 4>, default_id 1: every
 * element gap is `s` at "" -- not `i` at 0. */
typedef struct { sofab_object_descr_id_t which; union { int32_t i; char s[9]; } u; } _uv_e_t;
typedef struct { uint8_t len; _uv_e_t e[4]; } _uv_list_t;
typedef struct { _uv_list_t v; uint8_t w; } _uv_msg_t;
static const sofab_object_descr_field_t _uv_e_fields[] = {
    SOFAB_OBJECT_FIELD(0, _uv_e_t, u.i, SOFAB_OBJECT_FIELDTYPE_SIGNED),
    SOFAB_OBJECT_FIELD(1, _uv_e_t, u.s, SOFAB_OBJECT_FIELDTYPE_STRING),
};
typedef struct { sofab_object_descr_id_t which; union { char s[9]; uint32_t _align; } u; } _uv_e_img_t;
typedef char _uv_e_img_ok[(offsetof(_uv_e_img_t, u) == offsetof(_uv_e_t, u)) ? 1 : -1];
static const _uv_e_img_t _uv_e_default = { .which = 1 };
static const sofab_object_descr_t _uv_e = SOFAB_OBJECT_DESCR_UNION(
    _uv_e_fields, 2, NULL, 0, &_uv_e_default, _uv_e_t, which);
static const sofab_object_descr_field_t _uv_list_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _uv_list_t, e[0], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(1, _uv_list_t, e[1], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(2, _uv_list_t, e[2], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD_SEQUENCE(3, _uv_list_t, e[3], SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _uv_list_nested[] = { &_uv_e };
static const sofab_object_descr_t _uv_list = SOFAB_OBJECT_DESCR_SEQ_SIZED(
    _uv_list_fields, 4, _uv_list_nested, 1, _uv_list_t, len);
static const sofab_object_descr_field_t _uv_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(1, _uv_msg_t, v, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
    SOFAB_OBJECT_FIELD(2, _uv_msg_t, w, SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
};
static const sofab_object_descr_t *const _uv_msg_nested[] = { &_uv_list };
static const sofab_object_descr_t _uv_msg = SOFAB_OBJECT_DESCR(_uv_msg_fields, 2, _uv_msg_nested, 1);

static void test_object_union_array_default_id_not_first (void)
{
    _uv_msg_t m, d;
    uint8_t out[64];
    memset(&m, 0xA5, sizeof(m));
    sofab_object_init(&_uv_msg, &m);
    TEST_ASSERT_EQUAL_UINT8(0, m.v.len);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, m.v.e[2].which, "an element slot holds default_id");

    /* [{i:4}, {s:""}, {i:0}, {s:"z"}]: element 1 is the element default (a gap),
     * element 2 is a non-default_id option at its own default (framed, forced) */
    m.v.len = 4;
    m.v.e[0].which = 0; m.v.e[0].u.i = 4;
    m.v.e[1].which = 1; m.v.e[1].u.s[0] = '\0';
    m.v.e[2].which = 0; m.v.e[2].u.i = 0;
    m.v.e[3].which = 1; strcpy(m.v.e[3].u.s, "z");
    static const uint8_t want[] = { 0x0E,
                                    0x06, 0x01, 0x08, 0x07,
                                    0x16, 0x01, 0x00, 0x07,
                                    0x1E, 0x0A, 0x0A, 0x7A, 0x07,
                                    0x07 };
    size_t n = _un_encode_any(&_uv_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(want), n);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, sizeof(want));

    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode_any(&_uv_msg, &d, sizeof(d), out, n));
    TEST_ASSERT_EQUAL_UINT8(4, d.v.len);
    TEST_ASSERT_EQUAL_UINT(0, d.v.e[0].which);
    TEST_ASSERT_EQUAL_INT32(4, d.v.e[0].u.i);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, d.v.e[1].which, "the gap is default_id, not the first option");
    TEST_ASSERT_EQUAL_STRING("", d.v.e[1].u.s);
    TEST_ASSERT_EQUAL_UINT(0, d.v.e[2].which);
    TEST_ASSERT_EQUAL_INT32(0, d.v.e[2].u.i);
    TEST_ASSERT_EQUAL_UINT(1, d.v.e[3].which);
    TEST_ASSERT_EQUAL_STRING("z", d.v.e[3].u.s);

    /* the last element all-default: framed empty (it carries the length) */
    m.v.len = 2;
    m.v.e[0].which = 0; m.v.e[0].u.i = 1;
    m.v.e[1].which = 1; m.v.e[1].u.s[0] = '\0';
    static const uint8_t last[] = { 0x0E, 0x06, 0x01, 0x02, 0x07, 0x0E, 0x07, 0x07 };
    n = _un_encode_any(&_uv_msg, &m, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(last), n);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(last, out, sizeof(last));

    /* an element frame re-opened with the other option switches it (§7.4 + §7.4.1) */
    static const uint8_t reopen[] = { 0x0E, 0x06, 0x01, 0x08, 0x07, 0x06, 0x0A, 0x0A, 0x7A, 0x07, 0x07 };
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _un_decode_any(&_uv_msg, &d, sizeof(d), reopen, sizeof(reopen)));
    TEST_ASSERT_EQUAL_UINT8(1, d.v.len);
    TEST_ASSERT_EQUAL_UINT(1, d.v.e[0].which);
    TEST_ASSERT_EQUAL_STRING("z", d.v.e[0].u.s);
}
/* ---- switching to a leaf option starts it from that option's default ---------
 * ub: union { s string[8] @0 (default_id), bl blob[4] *SIZED* @1, ar u8[4] @2 }
 * -- @2 is capacity-only (no companion length), @1 carries one, and a
 * capacity-only BLOB option is forbidden (object.h; sofab_object_init asserts).
 *
 * Both arms get the same attack: ONE union frame that first sets the string
 * option to "SECRET" -- filling the shared storage -- and then switches to the
 * other option with a payload SHORTER than its capacity. MESSAGE_SPEC §7.4.1
 * starts the option switched to from ITS default, and §4.2 forbids a union
 * blob/array option a non-empty one, so what the wire does not carry must read
 * as empty, never as the option held before. The array answers through the
 * cleared tail (istream _bind_array_count), the blob through its length.
 */
typedef struct {
    sofab_object_descr_id_t which;
    union { char s[9]; struct { uint8_t len; uint8_t data[4]; } bl; uint8_t ar[4]; } u;
} _ub_t;
typedef struct { _ub_t f; } _ub_msg_t;
static const sofab_object_descr_field_t _ub_fields[] = {
    SOFAB_OBJECT_FIELD(0, _ub_t, u.s, SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD_BLOB_SIZED(1, _ub_t, u.bl.data, u.bl.len),
    SOFAB_OBJECT_FIELD_ARRAY(2, _ub_t, u.ar, SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED),
};
/* NULL image: default_id 0, the string at "" */
static const sofab_object_descr_t _ub = SOFAB_OBJECT_DESCR_UNION(
    _ub_fields, 3, NULL, 0, NULL, _ub_t, which);
static const sofab_object_descr_field_t _ub_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(0, _ub_msg_t, f, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _ub_msg_nested[] = { &_ub };
static const sofab_object_descr_t _ub_msg = SOFAB_OBJECT_DESCR(_ub_msg_fields, 1, _ub_msg_nested, 1);

static void test_object_union_switched_to_leaf_carries_no_earlier_option (void)
{
    _ub_msg_t d;
    uint8_t out[64];
    size_t n;

    /* { s:"SECRET" } then { ar:[0xFF] } -> {FF,00,00,00}, re-encoded as 4 elements */
    static const uint8_t ar_wire[] = { 0x06, 0x02, 0x32, 0x53, 0x45, 0x43, 0x52, 0x45,
                                       0x54, 0x13, 0x01, 0xFF, 0x01, 0x07 };
    static const uint8_t ar_want[] = { 0xFF, 0x00, 0x00, 0x00 };
    static const uint8_t ar_re[]   = { 0x06, 0x13, 0x04, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x07 };

    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _un_decode_any(&_ub_msg, &d, sizeof(d), ar_wire, sizeof(ar_wire)));
    TEST_ASSERT_EQUAL_UINT(2, d.f.which);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(ar_want, d.f.u.ar, 4,
        "the array slots past the wire count are the element default, not the string option");
    n = _un_encode_any(&_ub_msg, &d, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(ar_re), n);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ar_re, out, sizeof(ar_re));

    /* { s:"SECRET" } then { bl:FF } -> len 1, and the re-encode is that one byte:
     * the length is what keeps the bytes the wire never carried out of the value. */
    static const uint8_t bl_wire[] = { 0x06, 0x02, 0x32, 0x53, 0x45, 0x43, 0x52, 0x45,
                                       0x54, 0x0A, 0x0B, 0xFF, 0x07 };
    static const uint8_t bl_re[]   = { 0x06, 0x0A, 0x0B, 0xFF, 0x07 };

    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        _un_decode_any(&_ub_msg, &d, sizeof(d), bl_wire, sizeof(bl_wire)));
    TEST_ASSERT_EQUAL_UINT(1, d.f.which);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, d.f.u.bl.len, "the sized blob records what arrived");
    TEST_ASSERT_EQUAL_HEX8(0xFF, d.f.u.bl.data[0]);
    n = _un_encode_any(&_ub_msg, &d, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(sizeof(bl_re), n);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(bl_re, out, sizeof(bl_re),
        "the re-encode must not carry bytes of the option held before");
}

#endif /* sequence && fixlen && array support */

//
// Encode bounds (generator#656). A STRING member is char[maxlen + 1]: the encoder
// reads it bounded by that storage and refuses a value that fills it without a
// terminator (longer than maxlen) with SOFAB_RET_E_ARGUMENT, unless the build
// defines SOFAB_DISABLE_ENCODE_BOUNDS, where the read stays bounded and the whole
// buffer is emitted. A sized blob's length and a sized array's or holder's count
// past the capacity are CLAMPED to it -- the documented clamp contract, the same in
// both builds.
//

typedef struct
{
    char s[5];          /* maxlen 4 */
    char guard[4];      /* must never reach the wire */
} _eb_str_t;

static const sofab_object_descr_field_t _eb_str_fields[] =
{
    SOFAB_OBJECT_FIELD(0, _eb_str_t, s, SOFAB_OBJECT_FIELDTYPE_STRING),
};

static const sofab_object_descr_t _eb_str =
    SOFAB_OBJECT_DESCR(_eb_str_fields, 1, NULL, 0);

static sofab_ret_t _eb_encode (const sofab_object_descr_t *info, const void *in,
                               uint8_t *buf, size_t buflen, size_t *used)
{
    sofab_ostream_t octx;
    sofab_ret_t ret;
    sofab_ostream_init(&octx, buf, buflen, 0, NULL, NULL);
    ret = sofab_object_encode(&octx, info, in);
    *used = sofab_ostream_flush(&octx);
    return ret;
}

static void test_object_encode_string_at_bound (void)
{
    uint8_t buf[16];
    size_t used = 0;
    _eb_str_t in;
    memset(&in, 0, sizeof(in));
    memcpy(in.s, "abcd", 5);
    memcpy(in.guard, "zzz", 4);

    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _eb_encode(&_eb_str, &in, buf, sizeof(buf), &used));
    /* id 0 | FIXLEN = 0x02 ; (4 << 3) | STRING = 0x22 */
    const uint8_t expected[] = { 0x02, 0x22, 'a', 'b', 'c', 'd' };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, used);
}

static void test_object_encode_string_unterminated (void)
{
    uint8_t buf[16];
    size_t used = 0;
    _eb_str_t in;
    memset(&in, 0, sizeof(in));
    memcpy(in.s, "abcde", 5);       /* fills char[5]: no terminator, 5 > maxlen 4 */
    memcpy(in.guard, "zzz", 4);

    sofab_ret_t ret = _eb_encode(&_eb_str, &in, buf, sizeof(buf), &used);
#if !defined(SOFAB_DISABLE_ENCODE_BOUNDS)
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_ARGUMENT, ret,
        "a string that fills its storage without a terminator is over maxlen");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, used, "nothing of the refused field is written");
#else
    /* The opt-out keeps the read bounded: the five bytes, never the guard. */
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    const uint8_t expected[] = { 0x02, 0x2A, 'a', 'b', 'c', 'd', 'e' };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), used);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, used);
#endif
}

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
/* The same rule for an element of an array<string> (a sized wrapper holder). */
typedef struct {
    uint8_t len;
    char    s[3][5];
} _eb_strs_holder_t;
static const sofab_object_descr_field_t _eb_strs_fields[] = {
    SOFAB_OBJECT_FIELD(0, _eb_strs_holder_t, s[0], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(1, _eb_strs_holder_t, s[1], SOFAB_OBJECT_FIELDTYPE_STRING),
    SOFAB_OBJECT_FIELD(2, _eb_strs_holder_t, s[2], SOFAB_OBJECT_FIELDTYPE_STRING),
};
static const sofab_object_descr_t _eb_strs_holder =
    SOFAB_OBJECT_DESCR_SEQ_SIZED(_eb_strs_fields, 3, NULL, 0, _eb_strs_holder_t, len);

typedef struct { _eb_strs_holder_t arr; } _eb_strs_msg_t;
static const sofab_object_descr_field_t _eb_strs_msg_fields[] = {
    SOFAB_OBJECT_FIELD_SEQUENCE(5, _eb_strs_msg_t, arr, SOFAB_OBJECT_FIELDTYPE_SEQUENCE, 0),
};
static const sofab_object_descr_t *const _eb_strs_nested[] = { &_eb_strs_holder };
static const sofab_object_descr_t _eb_strs_msg =
    SOFAB_OBJECT_DESCR(_eb_strs_msg_fields, 1, _eb_strs_nested, 1);

static void test_object_encode_string_element_unterminated (void)
{
    uint8_t buf[32];
    size_t used = 0;
    _eb_strs_msg_t in;
    memset(&in, 0, sizeof(in));
    in.arr.len = 2;
    memcpy(in.arr.s[0], "ab", 3);
    memcpy(in.arr.s[1], "xxxxx", 5);   /* unterminated: over maxlen 4 */

    sofab_ret_t ret = _eb_encode(&_eb_strs_msg, &in, buf, sizeof(buf), &used);
#if !defined(SOFAB_DISABLE_ENCODE_BOUNDS)
    TEST_ASSERT_EQUAL_MESSAGE(SOFAB_RET_E_ARGUMENT, ret,
        "an over-maxlen element of an array<string> is refused");
#else
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, ret);
    (void)used;
#endif

    /* the control: the same element at its bound encodes */
    memcpy(in.arr.s[1], "xxxx", 5);
    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _eb_encode(&_eb_strs_msg, &in, buf, sizeof(buf), &used));
    TEST_ASSERT_TRUE(used > 0);
}

static void test_object_encode_holder_count_clamped (void)
{
    uint8_t buf[32];
    size_t used = 0;
    _eb_strs_msg_t in;
    memset(&in, 0, sizeof(in));
    in.arr.len = 9;                     /* past the 3 slots */
    memcpy(in.arr.s[0], "a", 2);
    memcpy(in.arr.s[1], "b", 2);
    memcpy(in.arr.s[2], "c", 2);

    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _eb_encode(&_eb_strs_msg, &in, buf, sizeof(buf), &used));
    /* seq 5 open = 0x2E; ids 0..2 one-byte strings; close 0x07 */
    const uint8_t expected[] = { 0x2E, 0x02, 0x0A, 'a', 0x0A, 0x0A, 'b', 0x12, 0x0A, 'c', 0x07 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "a holder count past the capacity is clamped to it");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, used);
}
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

static void test_object_encode_blob_len_clamped (void)
{
    uint8_t buf[32];
    size_t used = 0;
    blobsized_t in;
    memset(&in, 0, sizeof(in));
    for (uint8_t i = 0; i < 8; i++) in.data[i] = (uint8_t)(0xA0 + i);
    in.used_len = 200;                  /* past the 8-byte capacity */

    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _eb_encode(&_info_blobsized, &in, buf, sizeof(buf), &used));
    const uint8_t expected[] = { 0x02, 0x43, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "a blob length past the capacity is clamped to it");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, used);
}

static void test_object_encode_array_len_clamped (void)
{
    uint8_t buf[32];
    size_t used = 0;
    arrsized_t in;
    sofab_object_init(&_info_arrsized, &in);
    for (uint32_t i = 0; i < 5; i++) in.vals[i] = i + 1;
    in.len = 99;                        /* past the 5-element capacity */

    TEST_ASSERT_EQUAL(SOFAB_RET_OK, _eb_encode(&_info_arrsized, &in, buf, sizeof(buf), &used));
    const uint8_t expected[] = { 0x03, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05 };
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(expected), used,
        "an array count past the capacity is clamped to it");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, used);
}

int test_object_main (void)
{
    UNITY_BEGIN();

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) \
    && !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    RUN_TEST(test_object_union_init_holds_default_id);
    RUN_TEST(test_object_union_null_image_holds_id_0);
    RUN_TEST(test_object_union_encodes_only_the_held_option);
    RUN_TEST(test_object_union_last_option_wins);
    RUN_TEST(test_object_union_same_option_merges);
    RUN_TEST(test_object_union_away_and_back_starts_from_default);
    RUN_TEST(test_object_union_skipped_option_does_not_switch);
    RUN_TEST(test_object_union_array_elements_roundtrip);
    RUN_TEST(test_object_union_held_option_at_own_default_is_written);
    RUN_TEST(test_object_union_nested_union_not_default_when_forced);
    RUN_TEST(test_object_union_stray_tag_holds_nothing);
    RUN_TEST(test_object_union_prefix_image);
    RUN_TEST(test_object_union_array_default_id_not_first);
    RUN_TEST(test_object_union_switched_to_leaf_carries_no_earlier_option);
#endif

    RUN_TEST(test_object_serialize);
    RUN_TEST(test_object_deserialize);
    RUN_TEST(test_object_array_count_full_partial_empty);
    RUN_TEST(test_object_array_full_length_no_trim);
    RUN_TEST(test_object_array_sized);

    RUN_TEST(test_object_serialize_invalid_unsigned_size);
    RUN_TEST(test_object_serialize_invalid_signed_size);
    RUN_TEST(test_object_serialize_invalid_field_type);

    RUN_TEST(test_object_deserialize_invalid_nested_depth);
    RUN_TEST(test_object_deserialize_invalid_field_type);

    RUN_TEST(test_object_default_sequence_omitted);
    RUN_TEST(test_object_nondefault_sequence_framed);
    RUN_TEST(test_object_default_sequence_roundtrips_both_forms);
    RUN_TEST(test_object_roundtrip_empty_sequence_before_sequence);
    RUN_TEST(test_object_string_default_omission);
    RUN_TEST(test_object_blob_sized);
    RUN_TEST(test_object_blob_sized_default_nonempty);
    RUN_TEST(test_object_blob_sized_default_explicit_empty);
    RUN_TEST(test_object_blob_sized_no_default_image_unchanged);
    RUN_TEST(test_object_blob_sized_default_capacity_16);

    RUN_TEST(test_object_boolean_tolerant_decode);
    RUN_TEST(test_object_boolean_roundtrip);
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
    RUN_TEST(test_object_wiretype_wrapper_for_scalar_keeps_array);

    RUN_TEST(test_object_wrapper_reopen_replaces);
    RUN_TEST(test_object_wrapper_reopen_replaces_blob);
    RUN_TEST(test_object_wrapper_single_open_unchanged);
    RUN_TEST(test_object_reuse_needs_init_between_decodes);
    RUN_TEST(test_object_struct_reopen_merges);

    RUN_TEST(test_object_struct_wrapper_all_default_empty);
    RUN_TEST(test_object_struct_wrapper_last_element_framed);
    RUN_TEST(test_object_struct_wrapper_interior_default_omitted);
    RUN_TEST(test_object_struct_wrapper_leading_defaults_omitted);
    RUN_TEST(test_object_string_wrapper_last_element_written);
    RUN_TEST(test_object_struct_wrapper_roundtrip);

    RUN_TEST(test_object_sized_wrapper_leaf_len0_omitted);
    RUN_TEST(test_object_sized_wrapper_leaf_len1);
    RUN_TEST(test_object_sized_wrapper_leaf_len1_default_element);
    RUN_TEST(test_object_sized_wrapper_leaf_len_n_minus_1);
    RUN_TEST(test_object_sized_wrapper_leaf_len_n);
    RUN_TEST(test_object_sized_wrapper_leaf_decode_stores_length);
    RUN_TEST(test_object_sized_wrapper_leaf_decode_empty_and_reopen);

    RUN_TEST(test_object_sized_wrapper_struct_len0_omitted);
    RUN_TEST(test_object_sized_wrapper_struct_len1);
    RUN_TEST(test_object_sized_wrapper_struct_len1_default_element);
    RUN_TEST(test_object_sized_wrapper_struct_len_n_minus_1);
    RUN_TEST(test_object_sized_wrapper_struct_len_n);
    RUN_TEST(test_object_sized_wrapper_struct_decode_stores_length);
    RUN_TEST(test_object_sized_wrapper_struct_overindex_still_rejected);
    RUN_TEST(test_object_sized_wrapper_leaf_mistyped_element_absent);
    RUN_TEST(test_object_sized_wrapper_struct_mistyped_element_absent);

    RUN_TEST(test_object_overindex_mistyped_string_element_skipped);
    RUN_TEST(test_object_overindex_reject_window_unchanged);
    RUN_TEST(test_object_overindex_mistyped_blob_and_struct_element_skipped);
    RUN_TEST(test_object_overindex_skip_leaves_nothing_behind);

    RUN_TEST(test_object_sized_wrapper_blob_lengths);
    RUN_TEST(test_object_sized_wrapper_blob_decode_stores_length);
    RUN_TEST(test_object_sized_wrapper_row_lengths);
    RUN_TEST(test_object_sized_wrapper_row_decode_stores_length);

    RUN_TEST(test_object_sized_wrapper_init_clears_length);

    RUN_TEST(test_object_encode_string_at_bound);
    RUN_TEST(test_object_encode_string_unterminated);
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    RUN_TEST(test_object_encode_string_element_unterminated);
    RUN_TEST(test_object_encode_holder_count_clamped);
#endif
    RUN_TEST(test_object_encode_blob_len_clamped);
    RUN_TEST(test_object_encode_array_len_clamped);

    return UNITY_END();
}
