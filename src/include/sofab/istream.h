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
 * Typical usage:
 *  - Initialize a sofab_istream with a user context and a field callback.
 *  - Feed incoming data incrementally using sofab_istream_feed().
 *  - Dispatch fields in the callback and select the appropriate sofab_istream_read_*() function for reception.
 *  - Continue feeding data until the message is fully consumed.
 *  - After all data has been fed, the message data is available in the user-provided buffers.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_ISTREAM_H
#define SOFAB_ISTREAM_H

/**
 * @defgroup c_api C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SOFAB_ISTREAM_C
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
 * @brief Opaque Sofab input stream context.
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
 * void my_field_cb(sofab_istream_t *ctx, sofab_id_t id,
 *                  size_t size, size_t count, void *usrptr) {
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
 * @param size      Size of the field's value in bytes (for fixed-length types).
 * @param count     Number of elements (for array types).
 * @param usrptr    Optional user-provided pointer (provided at initialization).
 */
typedef void (*sofab_istream_field_cb_t) (
    sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr);

/*!
 * @brief Decoder context used for Sofab sequences.
 *
 * Each sequence has its own decoder instance. Decoders form a parent-child
 * chain and inherit the decoding state related to sequence depth and skip levels.
 */
typedef struct sofab_istream_decoder
{
    struct sofab_istream_decoder *parent;       /*!< Pointer to parent decoder for nested levels */
    sofab_istream_field_cb_t field_callback;    /*!< User callback for handling field IDs */
    void *usrptr;                               /*!< Optional user pointer for the field callback */
    uint8_t state;                              /*!< Internal parsing state machine */
    uint8_t skip_depth;                         /*!< Counter for skipped nested fields */
} sofab_istream_decoder_t;

/*!
 * @brief Internal state of the Sofab input stream.
 */
struct sofab_istream
{
    /* Members are grouped by alignment (widest first: value-word / pointers /
     * size_t, then the 32-bit id, then the 8-bit flags) so the context packs
     * without internal padding — the struct is opaque, so field order carries no
     * meaning and this keeps its footprint minimal on every target.
     *
     * The length/count members are size_t: they hold byte lengths and element
     * counts bounded by SOFAB_FIXLEN_MAX / SOFAB_ARRAY_MAX (both derived from
     * SIZE_MAX) and are fed straight from the size_t read-function arguments, so
     * they mirror that width exactly and need no narrowing cast — smaller than a
     * fixed uint32_t on 16-bit-size_t targets. */
    sofab_unsigned_t varint_value;              /*!< Accumulated varint value under construction */
    sofab_istream_decoder_t default_decoder;    /*!< Top-level decoder instance */
    uint8_t *target_ptr;                        /*!< Pointer to output buffer for field data */
#if SOFAB_STRICT_UTF8
    uint8_t *utf8_start;                        /*!< Start of a materialized string payload to
                                                 *!< UTF-8-validate at completion; NULL = don't
                                                 *!< validate (non-string, skipped, or empty). */
#endif
    sofab_istream_decoder_t *decoder;           /*!< Currently active decoder (may be nested) */
    size_t fixlen_remaining;                    /*!< Remaining bytes to read for a fixed-length field */
    size_t target_len;                          /*!< Target element size or total buffer length */
    size_t target_count;                        /*!< Number of elements to read into the target array */
    size_t array_wire_count;                    /*!< Element count declared on the wire (0..capacity) */
    sofab_id_t id;                              /*!< Current field ID being processed */
    uint8_t target_opt;                         /*!< Field options (used for type checks and flags) */
    uint8_t varint_shift;                       /*!< Current shift offset for varint decoding */
    uint8_t invalid;                            /*!< Sticky flag: a callback rejected the message */
};

/* prototypes *****************************************************************/

/*!
 * @brief Initializes a Sofab input stream context.
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
 * The decoder is resumable, so a call may end mid-field. The outcome is
 * three-valued (there is no separate finalize step):
 *  - @ref SOFAB_RET_OK: the consumed bytes end exactly on a field boundary — a
 *    complete message so far.
 *  - @ref SOFAB_RET_INCOMPLETE: the consumed bytes end inside a field (a partial
 *    varint, a fixlen/array payload shorter than declared) or with an open
 *    sequence. This is a valid but partial decode, NOT an error: the caller owns
 *    end-of-input and may feed more bytes to continue. Truncated input is
 *    reported here — it is neither accepted as complete nor rejected as invalid.
 *  - @ref SOFAB_RET_E_INVALID_MSG: the bytes are malformed regardless of what
 *    follows (varint too wide, length/count/id over the limit, bad type, ...).
 *
 * @param ctx       Pointer to the input stream context.
 * @param data      Pointer to the raw serialized data.
 * @param datalen   Length of @p data in bytes.
 *
 * @return SOFAB_RET_OK on a complete boundary, SOFAB_RET_INCOMPLETE on a partial
 *         (mid-field / open-sequence) decode, or another sofab_ret_t error code.
 */
extern sofab_ret_t sofab_istream_feed (
    sofab_istream_t *ctx, const void *data, size_t datalen);

/*!
 * @brief Reject the message in progress from within a field callback.
 *
 * A field callback usually binds a destination (a sofab_istream_read_* call) or
 * skips the field by binding nothing. Some declared bounds, however, can only be
 * checked by the callback itself — most notably an element whose wire index is at
 * or beyond a fixed-count array's capacity, delivered to the callback keyed by
 * its wire id with no way for the core to know the schema count. For those cases
 * a silent skip would let a malformed message decode as valid, violating
 * MESSAGE_SPEC §7/§7.1 ("a declared bound binds every target").
 *
 * Calling this sets a sticky invalid flag on the stream: the enclosing
 * @ref sofab_istream_feed (and every subsequent one) returns
 * @ref SOFAB_RET_E_INVALID_MSG regardless of what else is decoded. The flag is
 * set synchronously inside the callback, so it takes effect even when the field's
 * payload fill is deferred across chunked feeds. It is cleared only by
 * @ref sofab_istream_init.
 *
 * @param ctx  Pointer to the input stream context.
 */
extern void sofab_istream_invalidate (sofab_istream_t *ctx);

/* read functions *************************************************************/

/*!
 * @brief Reads a variable-length integer field.
 *
 * This function must only be invoked inside a field callback. The value is
 * written to @p var using @p varlen bytes.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Output variable.
 * @param varlen    Size of the output variable in bytes.
 * @param opt       Field options (type and flags).
 */
extern void sofab_istream_read_field (
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
    sofab_istream_read_field(ctx, var, sizeof(int8_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED));
}

/*!
 * @brief Reads an 8-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u8 (sofab_istream_t *ctx, uint8_t *var)
{
    sofab_istream_read_field(ctx, var, sizeof(uint8_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
}

/*!
 * @brief Reads a 16-bit signed integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_i16 (sofab_istream_t *ctx, int16_t *var)
{
    sofab_istream_read_field(ctx, var, sizeof(int16_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED));
}

/*!
 * @brief Reads a 16-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u16 (sofab_istream_t *ctx, uint16_t *var)
{
    sofab_istream_read_field(ctx, var, sizeof(uint16_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
}

/*!
 * @brief Reads a 32-bit signed integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_i32 (sofab_istream_t *ctx, int32_t *var)
{
    sofab_istream_read_field(ctx, var, sizeof(int32_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED));
}

/*!
 * @brief Reads a 32-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u32 (sofab_istream_t *ctx, uint32_t *var)
{
    sofab_istream_read_field(ctx, var, sizeof(uint32_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
}

#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
/*!
 * @brief Reads a 64-bit signed integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_i64 (sofab_istream_t *ctx, int64_t *var)
{
    sofab_istream_read_field(ctx, var, sizeof(int64_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED));
}

/*!
 * @brief Reads a 64-bit unsigned integer.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_u64 (sofab_istream_t *ctx, uint64_t *var)
{
    sofab_istream_read_field(ctx, var, sizeof(uint64_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
}
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */

/*!
 * @brief Reads a boolean value (1 byte).
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_bool (sofab_istream_t *ctx, bool *var)
{
    sofab_istream_read_field(ctx, var, 1,
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
}

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
/*!
 * @brief Reads a 32-bit floating-point value.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_fp32 (sofab_istream_t *ctx, float *var)
{
    sofab_istream_read_field(ctx, var, sizeof(float),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32));
}

#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
/*!
 * @brief Reads a 64-bit floating-point value.
 *
 * @param ctx   Pointer to the input stream context.
 * @param var   Pointer to destination variable.
 */
static inline void sofab_istream_read_fp64 (sofab_istream_t *ctx, double *var)
{
    sofab_istream_read_field(ctx, var, sizeof(double),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64));
}
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */

/*!
 * @brief Reads a string of fixed maximum size.
 *
 * A null terminator is always appended to the end of the string.
 *
 * @param ctx       Pointer to the input stream context.
 * @param var       Destination buffer.
 * @param varlen    Maximum number of bytes to read.
 */
static inline void sofab_istream_read_string (sofab_istream_t *ctx, char *var, size_t varlen)
{
    sofab_istream_read_field(ctx, var, varlen,
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
    sofab_istream_read_field(ctx, var, varlen,
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
    sofab_istream_read_field(ctx, var, varlen,
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_BLOB));
}
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

/* read array functions *******************************************************/

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
/*!
 * @brief Reads an array of elements.
 *
 * @param ctx            Pointer to the input stream context.
 * @param var            Destination array.
 * @param element_count  Number of fixed-size elements.
 * @param element_size   Size of each element in bytes.
 * @param opt            Field options (type and flags).
 */
extern void sofab_istream_read_array (
    sofab_istream_t *ctx, void *var, size_t element_count, size_t element_size, uint8_t opt);
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

/* inline convenience array functions *****************************************/

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(int8_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED));
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(uint8_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED));
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(int16_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED));
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(uint16_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED));
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(int32_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED));
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(uint32_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED));
}

#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(int64_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED));
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(uint64_t),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED));
}
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(float),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32));
}

#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
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
    sofab_istream_read_array(ctx, var, element_count, sizeof(double),
        SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY) |
        SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64));
}
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

/* nested sequence functions **************************************************/

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
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
    sofab_istream_t *ctx, sofab_istream_decoder_t *decoder,
    sofab_istream_field_cb_t field_callback, void *usrptr);
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

#ifdef __cplusplus
}
#endif

/** @} */ // end of defgroup

#endif /* SOFAB_ISTREAM_H */
