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

int test_object_main (void)
{
    UNITY_BEGIN();

    RUN_TEST(test_object_serialize);
    RUN_TEST(test_object_deserialize);

    RUN_TEST(test_object_serialize_invalid_unsigned_size);
    RUN_TEST(test_object_serialize_invalid_signed_size);
    RUN_TEST(test_object_serialize_invalid_field_type);

    RUN_TEST(test_object_deserialize_invalid_nested_depth);
    RUN_TEST(test_object_deserialize_invalid_field_type);

    return UNITY_END();
}
