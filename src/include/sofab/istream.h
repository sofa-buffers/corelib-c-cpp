/*!
 * @file istream.h
 * @brief SofaBuffers C - Input stream decoder for Sofab messages.
 *
 * This module implements a streaming decoder for SofaBuffers (Sofab), a compact,
 * TLV-like (Type-Length-Value) serialization format. The decoder accepts data
 * incrementally through @ref sofab_istream_feed, interprets field headers,
 * and automatically invokes user-provided field callbacks when new fields
 * begin. Within these callbacks, the user must select an appropriate
 * sofab_istream_read_*() function to bind a destination buffer or to start
 * decoding nested sequences.
 *
 * Features:
 * - Streaming operation (partial chunks supported)
 * - Nested sequences with independent decoders
 * - Variable-length integers (varints), signed and unsigned
 * - Fixed-length fields
 * - Arrays of primitive and fixed-size data
 * - Callback-based field routing
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _SOFAB_ISTREAM_H
#define _SOFAB_ISTREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SOFAB_ISTREAM_C
# define SOFAB_ISTREAM_EXTERN extern
#else
# define SOFAB_ISTREAM_EXTERN
#endif

/* includes *******************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "sofab/sofab.h"

/* macros *********************************************************************/
#define SOFAB_ISTREAM_OPT_FIELDTYPE(type)      ((type) & 0x07)
#define SOFAB_ISTREAM_OPT_FIXLENTYPE(type)     (((type) & 0x07) << 3)
#define SOFAB_ISTREAM_OPT_STRINGTERM           (0x40)

/* types **********************************************************************/

/*!
 * @brief Opaque Sofab Pointer to the input stream context.
 *
 * This structure contains all state related to the incremental parsing of
 * Sofab-encoded data. Users should not access its fields directly; instead,
 * they interact with it through the provided API.
 */
typedef struct sofab_istream sofab_istream_t;

/*!
 * @brief Field callback invoked whenever a new field ID is decoded.
 *
 * The callback is triggered once the decoder has determined a complete field
 * ID. The user must then call the appropriate sofab_istream_read_* function
 * to bind a destination buffer or initiate a nested sequence.
 * If no read function is called, the field will be skipped.
 *
 * Example usage:
 * @code
 * void my_field_cb(sofab_istream_t *ctx, sofab_id_t id, void *usrptr) {
 *     switch(id) {
 *         case FIELD_TEMP: sofab_istream_read_fp32(ctx, &state.temperature); break;
 *         case FIELD_NAME: sofab_istream_read_string(ctx, buffer, sizeof(buffer)); break;
 *         case FIELD_CHILD: sofab_istream_read_sequence(ctx, &child_decoder, child_cb, NULL); break;
 *     }
 * }
 * @endcode
 *
 * @param ctx       Pointer to the active input stream context.
 * @param id        Field ID that was encountered.
 * @param usrptr    Optional user-provided pointer (provided at initialization).
 */
typedef void (*sofab_istream_field_cb_t) (
    sofab_istream_t *ctx, sofab_id_t id, size_t size, void *usrptr);

/*!
 * @brief Decoder context used for Sofab sequences.
 *
 * Each sequence has its own decoder instance. Decoders form a parent-child
 * chain and inherit the decoding state related to sequence depth and skip levels.
 */
typedef struct sofab_decoder
{
    struct sofab_decoder *parent;               /*!< Pointer to parent decoder for nested levels */
    sofab_istream_field_cb_t field_callback;    /*!< User callback for handling field IDs */
    void *usrptr;                               /*!< Optional user pointer for the field callback */
    uint8_t state;                              /*!< Internal parsing state machine */
    uint8_t skip_depth;                         /*!< Counter for skipped nested fields */
} sofab_decoder_t;
/*!
 * @brief Internal state of the Sofab input stream.
 */
struct sofab_istream
{
    uint64_t varint_value;              /*!< Accumulated varint value under construction */
    sofab_decoder_t default_decoder;    /*!< Top-level decoder instance */
    uint32_t id;                        /*!< Current field ID being processed */
    uint32_t fixlen_remaining;          /*!< Remaining bytes to read for a fixed-length field */
    uint32_t target_len;                /*!< Target element size or total buffer length */
    uint32_t target_count;              /*!< Number of elements expected for array reads */
    uint8_t *target_ptr;                /*!< Pointer to output buffer for field data */
    sofab_decoder_t *decoder;           /*!< Currently active decoder (may be nested) */
    uint8_t target_opts;                /*!< Field options (used for type checks and flags) */
    uint8_t varint_shift;               /*!< Current shift offset for varint decoding */
};

/* prototypes *****************************************************************/

/*!
 * @brief Initializes an Sofab Pointer to the input stream context.
 *
 * Resets all decoder state, assigns the default decoder, and registers the
 * callback that will be triggered for each incoming field ID at the top level sequence.
 *
 * @param ctx              Pointer to the input stream context.
 * @param field_callback   User callback invoked for each field ID.
 * @param usrptr           Optional user pointer passed to the callback.
 */
extern void sofab_istream_init (
    sofab_istream_t *ctx, sofab_istream_field_cb_t field_callback, void *usrptr);

/*!
 * @brief Feeds raw Sofab-encoded bytes into the input stream.
 *
 * The decoder consumes the buffer incrementally. Field callbacks are triggered
 * when field headers are parsed. If the callback binds a destination buffer
 * via a sofab_istream_read_* function, subsequent bytes are written there.
 *
 * @param ctx       Pointer to the input stream context.
 * @param data      Pointer to the raw serialized data.
 * @param datalen   Length of @p data in bytes.
 *
 * @return SOFAB_RET_OK on success, or another sofab_ret_t error code.
 */
extern sofab_ret_t sofab_istream_feed (
    sofab_istream_t *ctx, const void *data, size_t datalen);

/* read functions *************************************************************/

/*!
 * @brief Reads an unsigned integer field.
 *
 * This function must only be invoked inside a field callback. The value is
 * written to @p var using @p varlen bytes.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Output variable.
 * @param varlen    Size of the output variable in bytes.
 */
extern void sofab_istream_read_unsigned (
    sofab_istream_t *ctx, void *var, size_t varlen);

/*!
 * @brief Reads a signed integer field.
 *
 * Must be called from within a field callback. Applies zigzag decoding to the
 * incoming varint and writes the result to @p var.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Output variable.
 * @param varlen    Size of the output variable in bytes.
 */
extern void sofab_istream_read_signed (
    sofab_istream_t *ctx, void *var, size_t varlen);

/*!
 * @brief Reads a fixed-length field.
 *
 * Must be called from within a field callback. Reads exactly @p varlen bytes
 * into the provided buffer.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Destination buffer.
 * @param varlen    Number of bytes to read.
 */
extern void sofab_istream_read_fixlen (
    sofab_istream_t *ctx, void *var, size_t varlen, uint8_t opt);

/* inline convenience functions ***********************************************/

/*!
 * @brief Reads an 8-bit signed integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_i8 (sofab_istream_t *ctx, int8_t *var)
{
    sofab_istream_read_signed(ctx, var, sizeof(int8_t));
}

/*!
 * @brief Reads an 8-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u8 (sofab_istream_t *ctx, uint8_t *var)
{
    sofab_istream_read_unsigned(ctx, var, sizeof(uint8_t));
}

/*!
 * @brief Reads a 16-bit signed integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_i16 (sofab_istream_t *ctx, int16_t *var)
{
    sofab_istream_read_signed(ctx, var, sizeof(int16_t));
}

/*!
 * @brief Reads a 16-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u16 (sofab_istream_t *ctx, uint16_t *var)
{
    sofab_istream_read_unsigned(ctx, var, sizeof(uint16_t));
}

/*!
 * @brief Reads a 32-bit signed integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_i32 (sofab_istream_t *ctx, int32_t *var)
{
    sofab_istream_read_signed(ctx, var, sizeof(int32_t));
}

/*!
 * @brief Reads a 32-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u32 (sofab_istream_t *ctx, uint32_t *var)
{
    sofab_istream_read_unsigned(ctx, var, sizeof(uint32_t));
}

/*!
 * @brief Reads a 64-bit signed integer.
*
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_i64 (sofab_istream_t *ctx, int64_t *var)
{
    sofab_istream_read_signed(ctx, var, sizeof(int64_t));
}

/*!
 * @brief Reads a 64-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u64 (sofab_istream_t *ctx, uint64_t *var)
{
    sofab_istream_read_unsigned(ctx, var, sizeof(uint64_t));
}

/*!
 * @brief Reads a boolean value (1 byte).
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_bool (sofab_istream_t *ctx, bool *var)
{
    sofab_istream_read_unsigned(ctx, var, 1);
}

/*!
 * @brief Reads a 32-bit floating-point value.
*
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_fp32 (sofab_istream_t *ctx, float *var)
{
    sofab_istream_read_fixlen(ctx, var, sizeof(float),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32));
}

/*!
 * @brief Reads a 64-bit floating-point value.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_fp64 (sofab_istream_t *ctx, double *var)
{
    sofab_istream_read_fixlen(ctx, var, sizeof(double),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64));
}

/*!
 * @brief Reads a string of fixed maximum size.
 *
 * A a null terminator is always appended to the end of the string.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Destination buffer.
 * @param varlen    Maximum number of bytes to read.
 */
static inline void sofab_istream_read_string (sofab_istream_t *ctx, char *var, size_t varlen)
{
    sofab_istream_read_fixlen(ctx, var, varlen,
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_STRING) |
        SOFAB_ISTREAM_OPT_STRINGTERM);
}

/*!
 * @brief Reads a string of fixed maximum size.
 *
 * No null terminator is appended to the end of the string.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Destination buffer.
 * @param varlen    Maximum number of bytes to read.
 */
static inline void sofab_istream_read_string_noterm (sofab_istream_t *ctx, char *var, size_t varlen)
{
    sofab_istream_read_fixlen(ctx, var, varlen,
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_STRING));
}

/*!
 * @brief Reads a binary blob of fixed maximum size.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Destination buffer.
 * @param varlen    Maximum number of bytes to read.
 */
static inline void sofab_istream_read_blob (sofab_istream_t *ctx, void *var, size_t varlen)
{
    sofab_istream_read_fixlen(ctx, var, varlen,
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_BLOB));
}

/* read array functions *******************************************************/

/*!
 * @brief Reads an array of unsigned integers.
 *
 * Each element is decoded using unsigned varint rules.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 * @param element_size   Size of each element in bytes.
 */
extern void sofab_istream_read_array_of_unsigned (
    sofab_istream_t *ctx, void *var, size_t element_count, size_t element_size);

/*!
 * @brief Reads an array of signed integers.
 *
 * Each element is decoded using signed varint rules.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 * @param element_size   Size of each element in bytes.
 */
extern void sofab_istream_read_array_of_signed (
    sofab_istream_t *ctx, void *var, size_t element_count, size_t element_size);

/*!
 * @brief Reads an array of fixed-length elements.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of fixed-size elements.
 * @param element_size   Size of each element in bytes.
 */
extern void sofab_istream_read_array_of_fixlen (
    sofab_istream_t *ctx, void *var, size_t element_count, size_t element_size, uint8_t opt);

/* inline convenience array functions *****************************************/

/*!
 * @brief Reads an array of 8-bit signed integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_i8 (
    sofab_istream_t *ctx, int8_t *var, size_t element_count)
{
    sofab_istream_read_array_of_signed(ctx, var, element_count, sizeof(int8_t));
}

/*!
 * @brief Reads an array of 8-bit unsigned integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_u8 (
    sofab_istream_t *ctx, uint8_t *var, size_t element_count)
{
    sofab_istream_read_array_of_unsigned(ctx, var, element_count, sizeof(uint8_t));
}

/*!
 * @brief Reads an array of 16-bit signed integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_i16 (
    sofab_istream_t *ctx, int16_t *var, size_t element_count)
{
    sofab_istream_read_array_of_signed(ctx, var, element_count, sizeof(int16_t));
}

/*!
 * @brief Reads an array of 16-bit unsigned integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_u16 (
    sofab_istream_t *ctx, uint16_t *var, size_t element_count)
{
    sofab_istream_read_array_of_unsigned(ctx, var, element_count, sizeof(uint16_t));
}

/*!
 * @brief Reads an array of 32-bit signed integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_i32 (
    sofab_istream_t *ctx, int32_t *var, size_t element_count)
{
    sofab_istream_read_array_of_signed(ctx, var, element_count, sizeof(int32_t));
}

/*!
 * @brief Reads an array of 32-bit unsigned integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_u32 (
    sofab_istream_t *ctx, uint32_t *var, size_t element_count)
{
    sofab_istream_read_array_of_unsigned(ctx, var, element_count, sizeof(uint32_t));
}

/*!
 * @brief Reads an array of 64-bit signed integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_i64 (
    sofab_istream_t *ctx, int64_t *var, size_t element_count)
{
    sofab_istream_read_array_of_signed(ctx, var, element_count, sizeof(int64_t));
}

/*!
 * @brief Reads an array of 64-bit unsigned integers.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_u64 (
    sofab_istream_t *ctx, uint64_t *var, size_t element_count)
{
    sofab_istream_read_array_of_unsigned(ctx, var, element_count, sizeof(uint64_t));
}

/*!
 * @brief Reads an array of 32-bit floating-point values.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_fp32 (
    sofab_istream_t *ctx, float *var, size_t element_count)
{
    sofab_istream_read_array_of_fixlen(ctx, var, element_count, sizeof(float),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32));
}

/*!
 * @brief Reads an array of 64-bit floating-point values.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of elements to read.
 */
static inline void sofab_istream_read_array_of_fp64 (
    sofab_istream_t *ctx, double *var, size_t element_count)
{
    sofab_istream_read_array_of_fixlen(ctx, var, element_count, sizeof(double),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64));
}

/* nested sequence functions **************************************************/

/*!
 * @brief Begins decoding a nested Sofab sequence.
 *
 * A nested sequence has its own decoder and callback. This allows hierarchical
 * message structures with independent interpretation of child fields.
 *
 * @param ctx            Pointer to the input stream context.
 * @param decoder        Decoder instance used for the new sequence.
 * @param field_callback Callback invoked for fields inside the nested sequence.
 * @param usrptr         Optional user pointer for the nested callback.
 */
extern void sofab_istream_read_sequence (
    sofab_istream_t *ctx, sofab_decoder_t *decoder,
    sofab_istream_field_cb_t field_callback, void *usrptr);

#ifdef __cplusplus
}
#endif

#endif /* _SOFAB_ISTREAM_H */
