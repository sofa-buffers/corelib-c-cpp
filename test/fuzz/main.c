/*!
 * @file main.c
 * @brief SofaBuffers C - Fuzz testing for input stream C API
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sofab/istream.h"

/*****************************************************************************/
/* tests */
/*****************************************************************************/

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

static sofab_istream_decoder_t _full_scale_decoder[2];

static void _full_scale_example_struct(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)size;
    (void)count;
    full_scale_seq_struct_t *seq = (full_scale_seq_struct_t *)usrptr;

    if (rand() % 1) return;

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

    if (rand() % 1) return;

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

    if (rand() % 1) return;

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

    if (rand() % 1) return;

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

    if (rand() % 1) return;

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

static void mutate(uint8_t *buf, size_t len)
{
    int mutations = 1 + (rand() % 6);

    for (int i = 0; i < mutations; i++)
    {
        size_t pos = rand() % len;

        switch (rand() % 6)
        {
            case 0: buf[pos] ^= 0x01; break;       // single bit
            case 1: buf[pos] ^= 0x80; break;       // varint continuation
            case 2: buf[pos] = 0x00; break;
            case 3: buf[pos] = 0xFF; break;
            case 4: buf[pos] = rand() & 0xFF; break;
            case 5: buf[pos] ^= (1 << (rand() % 8)); break;
        }
    }
}

int main (void)
{
    srand(0xC0FFEE);

    for (unsigned long iter = 0; ; iter++)
    {
        uint8_t input[sizeof(buffer)];
        memcpy(input, buffer, sizeof(buffer));

        mutate(input, sizeof(input));

        sofab_istream_t ctx;
        full_scale_example_t value;

        memset(&value, 0, sizeof(value));
        sofab_istream_init(&ctx, _full_scale_example, &value);

        sofab_istream_feed(&ctx, input, sizeof(input));

        if ((iter % 100000) == 0)
        {
            fprintf(stderr, "fuzzing iterations: %lu\n", iter);
        }
    }
}
