/*!
 * @file ostream.h
 * @brief SofaBuffers C - Output stream encoder for Sofab messages.
 *
 * This module implements a streaming encoder for SofaBuffers (Sofab), a compact,
 * TLV-like (Type-Length-Value) binary message format. The encoder writes
 * fields into a user-provided buffer and calls a user-supplied flush callback
 * when the buffer is full or when the user explicitly requests a flush.
 *
 * Typical usage:
 *  - Initialize an sofab_ostream_t with sofab_ostream_init(), providing a buffer
 *    and optionally a flush callback.
 *  - Use the sofab_ostream_write_*() functions to append fields (unsigned,
 *    signed, fixed-length, arrays, nested sequences).
 *  - If a flush callback is provided, it will be invoked automatically when
 *    the internal buffer needs space; the callback is responsible for handling
 *    (sending, persisting) the buffered bytes and usually calls
 *    sofab_ostream_buffer_set() to provide a fresh buffer.
 *  - Call sofab_ostream_flush() to flush any remaining bytes before teardown.
 *
 * Ownership and semantics (brief):
 *  - The sofab_ostream_t holds pointers into a buffer supplied by the caller.
 *    The stream never allocates or frees that buffer. The caller owns the
 *    memory and must ensure it remains valid until replaced with
 *    sofab_ostream_buffer_set() or the stream is destroyed.
 *  - The flush callback is optional (may be NULL). If NULL and the buffer
 *    becomes full, write calls will return an appropriate error code.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _SOFAB_OSTREAM_H
#define _SOFAB_OSTREAM_H

/**
 * @defgroup c_api C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SOFAB_OSTREAM_C
# define SOFAB_OSTREAM_EXTERN extern
#else
# define SOFAB_OSTREAM_EXTERN
#endif

/* includes *******************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "sofab/sofab.h"

/* types **********************************************************************/

/*!
 * @brief Opaque Sofab output stream context.
 *
 * This structure holds all state related to the incremental encoding of
 * Sofab-encoded data. Users should not access its fields directly; instead,
 * they interact with it through the provided API.
 */
typedef struct sofab_ostream sofab_ostream_t;

/*!
 * @brief Output stream flush callback type.
 *
 * The flush callback is invoked when the output stream needs to free or hand
 * off previously written bytes (for example, when the internal buffer is full
 * or when sofab_ostream_flush() is called). The callback implementation is
 * responsible for processing the bytes (e.g. sending them over a transport,
 * writing to storage) and for providing, if desired, a new buffer through
 * sofab_ostream_buffer_set().
 *
 * The callback must not block the stream reentrantly (the library does not
 * perform internal locking). If required, the callback should copy or queue
 * data and return quickly.
 *
 * @param usrptr  User-provided pointer.
 * @param data    Pointer to the buffer that contains bytes to flush.
 * @param len     Number of bytes available at @p data to flush.
 */
typedef void (*sofab_ostream_flush_cb_t) (
    sofab_ostream_t *ctx, const uint8_t *data, size_t len, void *usrptr);

/*!
 * @brief Sofab output stream context.
 *
 * This structure holds the state required to encode Sofab fields into a
 * contiguous buffer. The user initializes it with sofab_ostream_init(), then
 * repeatedly calls writer functions (sofab_ostream_write_*). The application
 * is responsible for supplying and owning the buffer memory; the stream only
 * holds pointers into that memory.
 */
struct sofab_ostream
{
    uint8_t *buffer;                /*!< Pointer to the start of the active buffer. */
    uint8_t *bufend;                /*!< Pointer one-past-the-end of the active buffer. */
    uint8_t *offset;                /*!< Current write cursor within the buffer. */
    sofab_ostream_flush_cb_t flush; /*!< Optional flush callback invoked when buffer data must be handled. */
    void *usrptr;                   /*!< User-provided pointer passed to the flush callback. */
};

/* prototypes *****************************************************************/

/*!
 * @brief Initialize an Sofab output stream context.
 *
 * Prepares the sofab_ostream_t for usage. The supplied buffer will be used for
 * subsequent writes. If @p flush is non-NULL it will be called whenever the
 * implementation needs to hand off buffered bytes (for example when the buffer
 * is full or when sofab_ostream_flush() is invoked). The user pointer @p usrptr
 * is passed unchanged to that callback.
 *
 * The function does not copy the provided buffer; the caller retains ownership
 * and must ensure the memory remains valid until replaced by
 * sofab_ostream_buffer_set() or the stream is destroyed.
 *
 * @param ctx      Pointer to the output stream context to initialize.
 * @param buffer   Pointer to a writable buffer used to accumulate encoded bytes.
 * @param buflen   Size of @p buffer in bytes.
 * @param offset   Initial offset (0..buflen) to start writing from. The offset
 *                 is applied relative to @p buffer; e.g. offset==0 writes at
 *                 buffer[0], offset==buflen makes the buffer effectively full.
 * @param flush    Optional flush callback invoked when the buffer data must be handled.
 * @param usrptr   Optional user pointer passed to @p flush when invoked.
 */
extern void sofab_ostream_init (
    sofab_ostream_t *ctx, uint8_t *buffer, size_t buflen, size_t offset,
    sofab_ostream_flush_cb_t flush, void *usrptr);

/*!
 * @brief Flush any pending bytes from the output stream.
 *
 * If a flush callback was registered during initialization, it is invoked with
 * any pending bytes currently present in the active buffer. After the call,
 * the internal buffer is considered empty; offset is reset to the start of the
 * buffer (unless the flush callback calls sofab_ostream_buffer_set() to provide
 * a different buffer).
 *
 * @param ctx   Pointer to the output stream context.
 * @return Number of bytes flushed.
 */
extern size_t sofab_ostream_flush (sofab_ostream_t *ctx);

/*!
 * @brief Get the number of bytes used in the current buffer.
 *
 * This function returns the number of bytes that have been written into the
 * active buffer since the last flush or initialization.
 *
 * @param ctx      Pointer to the output stream context.
 * @return Number of bytes used in the current buffer.
 */
extern size_t sofab_ostream_bytes_used (sofab_ostream_t *ctx);

/*!
 * @brief Replace the active buffer used by the output stream.
 *
 * This function updates the stream to use a new buffer region. Typical usage
 * is inside a flush callback: after handing off the old buffer bytes the
 * callback calls sofab_ostream_buffer_set() to present a fresh buffer and resume
 * encoding without losing data.
 *
 * The buffer is not copied; the stream simply adjusts pointers to the new
 * region. The caller retains ownership of the memory.
 *
 * @param ctx      Pointer to the output stream context.
 * @param buffer   Pointer to the new buffer to use.
 * @param buflen   Size of @p buffer in bytes.
 * @param offset   Initial offset within the new buffer (0..buflen).
 */
extern void sofab_ostream_buffer_set (
    sofab_ostream_t *ctx, uint8_t *buffer, size_t buflen, size_t offset);

/* write functions ************************************************************/

/*!
 * @brief Write an unsigned integer field.
 *
 * Encodes a field header for the given @p id with an unsigned varint payload
 * and writes the varint-encoded @p value into the stream buffer.
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier (sofab_id_t).
 * @param value  Unsigned integer value to encode.
 *
 * @return SOFAB_RET_OK on success, otherwise an sofab_ret_t error code will be returned.
 */
extern sofab_ret_t sofab_ostream_write_unsigned (
    sofab_ostream_t *ctx, sofab_id_t id, sofab_unsigned_t value);

/*!
 * @brief Write a signed integer field.
 *
 * Encodes a field header for @p id and writes a signed integer value. The
 * value encoding typically uses zigzag transformation followed by varint
 * encoding.
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier.
 * @param value  Signed integer value to encode.
 *
 * @return SOFAB_RET_OK on success, otherwise an error code.
 */
extern sofab_ret_t sofab_ostream_write_signed (
    sofab_ostream_t *ctx, sofab_id_t id, sofab_signed_t value);

/*!
 * @brief Write a fixed-length field.
 *
 * Encodes a fixed-length field header for @p id and copies @p datalen bytes
 * from @p data into the output stream. The @p type parameter specifies the
 * semantic type of the fixed-length data.
 *
 * @param ctx      Pointer to the output stream context.
 * @param id       Field identifier.
 * @param data     Pointer to bytes to write.
 * @param datalen  Number of bytes to write from @p data.
 * @param type     Semantic fixed-length type (sofab_fixlentype_t).
 *
 * @return SOFAB_RET_OK on success, otherwise an sofab_ret_t error code.
 */
extern sofab_ret_t sofab_ostream_write_fixlen (
    sofab_ostream_t *ctx, sofab_id_t id,
    const void *data, int32_t datalen,
    sofab_fixlentype_t type);

/* inline convenience functions ***********************************************/

/*!
 * @brief Write a boolean value (1 byte).
 *
 * Convenience wrapper that encodes a boolean as an unsigned value (0 or 1).
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier.
 * @param value  Boolean value to encode.
 *
 * @return See sofab_ostream_write_unsigned().
 */
static inline sofab_ret_t sofab_ostream_write_boolean (
    sofab_ostream_t *ctx, sofab_id_t id, bool value)
{
    return sofab_ostream_write_unsigned(ctx, id, value ? 1 : 0);
}

/*!
 * @brief Write a 32-bit floating-point value.
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier.
 * @param value  Float value to encode.
 *
 * @return See sofab_ostream_write_fixlen().
 */
static inline sofab_ret_t sofab_ostream_write_fp32 (
    sofab_ostream_t *ctx, sofab_id_t id, float value)
{
    return sofab_ostream_write_fixlen(ctx, id, &value, sizeof(value), SOFAB_FIXLENTYPE_FP32);
}

/*!
 * @brief Write a 64-bit floating-point value.
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier.
 * @param value  Double value to encode.
 *
 * @return See sofab_ostream_write_fixlen().
 */
static inline sofab_ret_t sofab_ostream_write_fp64 (
    sofab_ostream_t *ctx, sofab_id_t id, double value)
{
    return sofab_ostream_write_fixlen(ctx, id, &value, sizeof(value), SOFAB_FIXLENTYPE_FP64);
}

/*!
 * @brief Write a null-terminated string.
 *
 * Convenience wrapper that writes a string field by using strlen()
 * to determine length.
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier.
 * @param text   Null-terminated C string to write.
 *
 * @return See sofab_ostream_write_fixlen().
 */
static inline sofab_ret_t sofab_ostream_write_string (
    sofab_ostream_t *ctx, sofab_id_t id, const char *text)
{
    return sofab_ostream_write_fixlen(ctx, id, text, strlen(text), SOFAB_FIXLENTYPE_STRING);
}

/*!
 * @brief Write a binary blob field.
 *
 * Convenience wrapper that writes raw bytes as a blob fixed-length field.
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier.
 * @param data   Pointer to binary data.
 * @param size   Number of bytes to write.
 *
 * @return See sofab_ostream_write_fixlen().
 */
static inline sofab_ret_t sofab_ostream_write_blob (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data, size_t size)
{
    return sofab_ostream_write_fixlen(ctx, id, data, size, SOFAB_FIXLENTYPE_BLOB);
}

/* write array functions ******************************************************/

/*!
 * @brief Write an array of unsigned integers.
 *
 * Encodes an array field composed of unsigned integer elements. Each array
 * element is encoded according to the element size and unsigned varint rules.
 * Supported types: uint8_t, uint16_t, uint32_t, uint64_t.
 *
 * @param ctx            Pointer to the output stream context.
 * @param id             Field identifier.
 * @param data           Pointer to the array data.
 * @param element_size   Size of each element in bytes.
 * @param element_count  Number of elements in @p data.
 *
 * @return SOFAB_RET_OK on success, otherwise an error code.
 */
extern sofab_ret_t sofab_ostream_write_array_of_unsigned (
    sofab_ostream_t *ctx, sofab_id_t id,
    const void *data, int32_t element_count, int32_t element_size);

/*!
 * @brief Write an array of signed integers.
 *
 * Encodes an array field composed of signed integer elements. Each array
 * element is encoded according to the element size and signed varint rules.
 * Supported types: int8_t, int16_t, int32_t, int64_t.
 *
 * @param ctx            Pointer to the output stream context.
 * @param id             Field identifier.
 * @param data           Pointer to the array data.
 * @param element_size   Size of each element in bytes.
 * @param element_count  Number of elements in @p data.
 *
 * @return SOFAB_RET_OK on success, otherwise an error code.
 */
extern sofab_ret_t sofab_ostream_write_array_of_signed (
    sofab_ostream_t *ctx, sofab_id_t id,
    const void *data, int32_t element_count, int32_t element_size);

/*!
 * @brief Write an array of fixed-length elements.
 *
 * Writes a contiguous block representing @p element_count elements of fixed
 * size @p element_size. The @p type describes the semantic fixed-length type
 * for the array elements.
 * Only floating-point types (FP32, FP64) are supported for fixed-length arrays.
 *
 * @param ctx            Pointer to the output stream context.
 * @param id             Field identifier.
 * @param data           Pointer to element data.
 * @param element_size   Size of each element in bytes.
 * @param element_count  Number of elements.
 * @param type           Semantic fixed-length type of elements.
 *
 * @return SOFAB_RET_OK on success, otherwise an error code.
 */
extern sofab_ret_t sofab_ostream_write_array_of_fixlen (
    sofab_ostream_t *ctx, sofab_id_t id,
    const void *data, int32_t element_count, int32_t element_size,
    sofab_fixlentype_t type);

/* inline convenience array functions *****************************************/

/*!
 * @brief Writes an array of 8-bit signed integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_signed().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_i8 (
    sofab_ostream_t *ctx, sofab_id_t id, const int8_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_signed(ctx, id, var, element_count, sizeof(int8_t));
}

/*!
 * @brief Writes an array of 8-bit unsigned integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_unsigned().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_u8 (
    sofab_ostream_t *ctx, sofab_id_t id, const uint8_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_unsigned(ctx, id, var, element_count, sizeof(uint8_t));
}

/*!
 * @brief Writes an array of 16-bit signed integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_signed().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_i16 (
    sofab_ostream_t *ctx, sofab_id_t id, const int16_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_signed(ctx, id, var, element_count, sizeof(int16_t));
}

/*!
 * @brief Writes an array of 16-bit unsigned integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_unsigned().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_u16 (
    sofab_ostream_t *ctx, sofab_id_t id, const uint16_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_unsigned(ctx, id, var, element_count, sizeof(uint16_t));
}

/*!
 * @brief Writes an array of 32-bit signed integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_signed().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_i32 (
    sofab_ostream_t *ctx, sofab_id_t id, const int32_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_signed(ctx, id, var, element_count, sizeof(int32_t));
}

/*!
 * @brief Writes an array of 32-bit unsigned integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_unsigned().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_u32 (
    sofab_ostream_t *ctx, sofab_id_t id, const uint32_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_unsigned(ctx, id, var, element_count, sizeof(uint32_t));
}

/*!
 * @brief Writes an array of 64-bit signed integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_signed().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_i64 (
    sofab_ostream_t *ctx, sofab_id_t id, const int64_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_signed(ctx, id, var, element_count, sizeof(int64_t));
}

/*!
 * @brief Writes an array of 64-bit unsigned integers.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param var            Destination array.
 * @param element_count  Number of elements to write.
 *
 * @return See sofab_ostream_write_array_of_unsigned().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_u64 (
    sofab_ostream_t *ctx, sofab_id_t id, const uint64_t *var, int32_t element_count)
{
    return sofab_ostream_write_array_of_unsigned(ctx, id, var, element_count, sizeof(uint64_t));
}

/*!
 * @brief Write an array of 32-bit floating-point values.
 *
 * @param ctx            Pointer to the output stream context.
 * @param id             Field identifier.
 * @param data           Pointer to float array.
 * @param element_count  Number of float elements.
 *
 * @return See sofab_ostream_write_array_of_fixlen().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_fp32 (
    sofab_ostream_t *ctx, sofab_id_t id, const float *data, int32_t element_count)
{
    return sofab_ostream_write_array_of_fixlen(
        ctx, id, data, element_count, sizeof(float), SOFAB_FIXLENTYPE_FP32);
}

/*!
 * @brief Write an array of 64-bit floating-point values.
 *
 * @param ctx            Pointer to the output stream context.
 * @param id             Field identifier.
 * @param data           Pointer to double array.
 * @param element_count  Number of double elements.
 *
 * @return See sofab_ostream_write_array_of_fixlen().
 */
static inline sofab_ret_t sofab_ostream_write_array_of_fp64 (
    sofab_ostream_t *ctx, sofab_id_t id, const double *data, int32_t element_count)
{
    return sofab_ostream_write_array_of_fixlen(
        ctx, id, data, element_count, sizeof(double), SOFAB_FIXLENTYPE_FP64);
}

/* nested sequence functions **************************************************/

/*!
 * @brief Begin encoding a nested sequence (start marker).
 *
 * Writes the header that begins a nested sequence for field @p id. After this
 * call, subsequent writes belong to the nested sequence until
 * sofab_ostream_write_sequence_end() is called. Nested sequences allow
 * hierarchical message structures with a new field identifier space.
 *
 * @param ctx    Pointer to the output stream context.
 * @param id     Field identifier representing the sequence.
 *
 * @return SOFAB_RET_OK on success, otherwise an sofab_ret_t error code.
 */
extern sofab_ret_t sofab_ostream_write_sequence_begin (sofab_ostream_t *ctx, sofab_id_t id);

/*!
 * @brief End encoding a nested sequence (end marker).
 *
 * Writes the termination marker for the current nested sequence started via
 * sofab_ostream_write_sequence_begin().
 *
 * @param ctx   Pointer to the output stream context.
 *
 * @return SOFAB_RET_OK on success, otherwise an sofab_ret_t error code.
 */
extern sofab_ret_t sofab_ostream_write_sequence_end (sofab_ostream_t *ctx);

#ifdef __cplusplus
}
#endif

/** @} */ // end of defgroup

#endif /* _SOFAB_OSTREAM_H */
