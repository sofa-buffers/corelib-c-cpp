/*!
 * @file ostream.c
 * @brief SofaBuffers C - Output stream encoder for Sofab messages.
 *
 * SPDX-License-Identifier: MIT
 */

#define SOFAB_OSTREAM_C

/* includes *******************************************************************/
#include "sofab/ostream.h"
#include "sofab/utf8.h"

#include <assert.h>

/* constants ******************************************************************/

/* macros *********************************************************************/
/*!
 * @brief Keep a shared helper out of line so its one copy is reused.
 *
 * Several writers share a common prefix (header + varint) or body (the varint
 * array loop). Forcing those helpers out of line makes the toolchain emit a
 * single copy instead of inlining — and thus duplicating — them into every
 * caller, which is a net size win on the small targets this corelib targets.
 * Falls back to nothing on compilers without the attribute (the code stays
 * correct, just potentially inlined).
 */
#if defined(__GNUC__)
# define SOFAB_NOINLINE __attribute__((noinline))
#else
# define SOFAB_NOINLINE
#endif

/* types **********************************************************************/

/* prototypes *****************************************************************/

/* static vars ****************************************************************/

/* functions ******************************************************************/

/*!
 * @brief ZigZag-encode a signed value to an unsigned one.
 *
 * Maps small-magnitude signed values to small unsigned values so they encode
 * compactly as a varint.
 *
 * @param v  Signed value to transform.
 * @return The ZigZag-encoded unsigned value.
 */
static inline sofab_unsigned_t _zigzag_encode (sofab_signed_t v)
{
    const int bits = sizeof(v) * 8;
    // Cast to unsigned before shifting: left-shifting a negative signed value
    // is undefined behavior (C11 6.5.7/4) and trips -fsanitize=undefined. The
    // sign-extending right shift below is implementation-defined on two's-
    // complement targets, which is what SofaBuffers assumes.
    return (((sofab_unsigned_t)v) << 1) ^ (sofab_unsigned_t)(v >> (bits - 1));
}

/*!
 * @brief Push a single byte to the buffer, flushing first if it is full.
 *
 * If the buffer is full and a flush callback is set, it is invoked and the
 * cursor reset; without a callback a full buffer is an overflow.
 *
 * @param ctx   Output stream context.
 * @param byte  Byte to append.
 * @return 0 on success, -1 on overflow (buffer full and no flush callback).
 */
static int _push_byte (sofab_ostream_t *ctx, uint8_t byte)
{
    if (ctx->offset >= ctx->bufend)
    {
        // buffer full, flush if possible
        if (ctx->flush)
        {
            size_t used = ctx->offset - ctx->buffer;
            ctx->flush(ctx, ctx->buffer, used, ctx->usrptr);
            ctx->offset = ctx->buffer;
        }
        else
        {
            // no flush callback, return buffer overflow
            return -1;
        }
    }

    *ctx->offset++ = byte;

    return 0;
}

/*!
 * @brief Write an unsigned value to the buffer as a LEB128 varint.
 *
 * @param ctx    Output stream context.
 * @param value  Unsigned value to encode.
 * @return 0 on success, negative on buffer overflow.
 */
static int _varint_encode (sofab_ostream_t *ctx, sofab_unsigned_t value)
{
    int ret = 0;

    do
    {
        uint8_t b = value & 0x7F;
        value >>= 7;
        if (value) b |= 0x80;
        if ((ret = _push_byte(ctx, b)) != 0)
        {
            return ret;
        }
    } while (value != 0);

    return 0;
}

/*!
 * @brief Combine a value and a 3-bit type tag into a single field header word.
 *
 * Shifts @p var left by 3 and packs @p type into the low 3 bits, ready to be
 * emitted as a varint.
 *
 * @param var   Value to shift into the upper bits (e.g. field id or length).
 * @param type  3-bit type tag to store in the low bits.
 * @return The combined @c (var<<3)|type word.
 */
static sofab_unsigned_t _type_encode (sofab_unsigned_t var, int type)
{
    return ((var << 3) | (type & 0x07));
}

/*!
 * @brief Write a field header (id + type) to the buffer as a varint.
 *
 * @param ctx   Output stream context.
 * @param id    Field identifier (rejected if greater than @ref SOFAB_ID_MAX).
 * @param type  Field type tag.
 * @return SOFAB_RET_OK on success, SOFAB_RET_E_ARGUMENT for an out-of-range id,
 *         or SOFAB_RET_E_BUFFER_FULL on overflow.
 */
SOFAB_NOINLINE
static sofab_ret_t _write_id_type (sofab_ostream_t *ctx, sofab_id_t id, sofab_type_t type)
{
    if (id > SOFAB_ID_MAX)
    {
        return SOFAB_RET_E_ARGUMENT;
    }

    if (_varint_encode(ctx, _type_encode(id, type)) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

    return SOFAB_RET_OK;
}

/*!
 * @brief Emit a field's (id, type) header followed by one varint payload.
 *
 * This id/type header + trailing varint is the common prefix of every writer:
 * for a scalar the payload is the (possibly ZigZag-encoded) value; for a fixlen
 * field it is the length/subtype word; for an array it is the element count.
 * Factoring it here (a single out-of-line copy) keeps that prefix — and its
 * overflow handling — from being duplicated across all the writers.
 *
 * @param ctx      Output stream context.
 * @param id       Field identifier.
 * @param type     Wire type tag.
 * @param payload  Varint payload to append after the header.
 * @return SOFAB_RET_OK on success, or an sofab_ret_t error code.
 */
SOFAB_NOINLINE
static sofab_ret_t _write_id_varint (
    sofab_ostream_t *ctx, sofab_id_t id, sofab_type_t type, sofab_unsigned_t payload)
{
    sofab_ret_t ret;

    if ((ret = _write_id_type(ctx, id, type)) != SOFAB_RET_OK)
    {
        return ret;
    }

    if (_varint_encode(ctx, payload) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

    return SOFAB_RET_OK;
}

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
/*!
 * @brief Copy fixed-length data to the buffer in source byte order.
 *
 * @param ctx      Output stream context.
 * @param data     Pointer to the bytes to write.
 * @param datalen  Number of bytes to write.
 * @return SOFAB_RET_OK on success, SOFAB_RET_E_BUFFER_FULL on overflow.
 */
static sofab_ret_t _write_fixlen (sofab_ostream_t *ctx, const void *data, int32_t datalen)
{
    const uint8_t *bytes = (const uint8_t *)data;

    for (int32_t i = 0; i < datalen; i++)
    {
        if (_push_byte(ctx, bytes[i]) != 0)
        {
            return SOFAB_RET_E_BUFFER_FULL;
        }
    }

    return SOFAB_RET_OK;
}

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
/*!
 * @brief Copy fixed-length data to the buffer in reversed byte order.
 *
 * Used on big-endian targets to emit little-endian floating-point payloads.
 *
 * @param ctx      Output stream context.
 * @param data     Pointer to the bytes to write.
 * @param datalen  Number of bytes to write.
 * @return SOFAB_RET_OK on success, SOFAB_RET_E_BUFFER_FULL on overflow.
 */
static sofab_ret_t _write_fixlen_reverse (sofab_ostream_t *ctx, const uint8_t *data, int32_t datalen)
{
    for (int32_t i = datalen - 1; i >= 0; i--)
    {
        if (_push_byte(ctx, data[i]) != 0)
        {
            return SOFAB_RET_E_BUFFER_FULL;
        }
    }

    return SOFAB_RET_OK;
}
#endif /* defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ */
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

//

extern void sofab_ostream_init (
    sofab_ostream_t *ctx, uint8_t *buffer, size_t buflen, size_t offset,
    sofab_ostream_flush_cb_t flush, void *usrptr)
{
    assert(ctx != NULL);
    assert(buffer != NULL);
    assert(buflen > 0);
    assert(offset < buflen);

    ctx->buffer = buffer;
    ctx->offset = buffer + offset;
    ctx->bufend = buffer + buflen;
    ctx->flush = flush;
    ctx->usrptr = usrptr;
}

extern size_t sofab_ostream_flush (sofab_ostream_t *ctx)
{
    size_t used;

    assert(ctx != NULL);

    used = ctx->offset - ctx->buffer;
    if (ctx->flush && used)
    {
        ctx->flush(ctx, ctx->buffer, used, ctx->usrptr);
        ctx->offset = ctx->buffer;
    }

    return used;
}

extern size_t sofab_ostream_bytes_used (sofab_ostream_t *ctx)
{
    assert(ctx != NULL);

    return (size_t)(ctx->offset - ctx->buffer);
}

extern void sofab_ostream_buffer_set	(
    sofab_ostream_t *ctx, uint8_t *buffer, size_t buflen, size_t offset)
{
    assert(ctx != NULL);
    assert(buffer != NULL);
    assert(buflen > 0);
    assert(offset < buflen);

    ctx->buffer = buffer;
    ctx->offset = buffer + offset;
    ctx->bufend = buffer + buflen;
}

extern sofab_ret_t sofab_ostream_write_unsigned (
    sofab_ostream_t *ctx, sofab_id_t id, sofab_unsigned_t value)
{
    assert(ctx != NULL);

    return _write_id_varint(ctx, id, SOFAB_TYPE_VARINT_UNSIGNED, value);
}

extern sofab_ret_t sofab_ostream_write_signed (
    sofab_ostream_t *ctx, sofab_id_t id, sofab_signed_t value)
{
    assert(ctx != NULL);

    return _write_id_varint(ctx, id, SOFAB_TYPE_VARINT_SIGNED, _zigzag_encode(value));
}

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
extern sofab_ret_t sofab_ostream_write_fixlen (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data, int32_t datalen,
    sofab_fixlentype_t type)
{
    sofab_ret_t ret;

    assert(ctx != NULL);
    assert(datalen == 0 || data != NULL);

#if SOFAB_STRICT_UTF8
    // A `string` value MUST be valid UTF-8 (MESSAGE_SPEC §8); refuse a non-UTF-8
    // one with the invalid-argument error before any bytes are emitted. This is
    // the encode-side half of the symmetric strict check and enforces the
    // producer-side MUST NOT: without it a strict ecosystem's own encoders could
    // emit bytes its decoders reject (CORELIB_PLAN §6.4). Only STRING is checked
    // - a blob is opaque bytes and is never validated.
    if (type == SOFAB_FIXLENTYPE_STRING &&
        !sofab_utf8_valid((const uint8_t *)data, (size_t)datalen))
    {
        return SOFAB_RET_E_ARGUMENT;
    }
#endif /* SOFAB_STRICT_UTF8 */

    if ((ret = _write_id_varint(ctx, id, SOFAB_TYPE_FIXLEN,
            _type_encode(datalen, type))) != SOFAB_RET_OK)
    {
        return ret;
    }

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    if (type == SOFAB_FIXLENTYPE_FP32 || type == SOFAB_FIXLENTYPE_FP64)
    {
        if ((ret = _write_fixlen_reverse(ctx, data, datalen)) != SOFAB_RET_OK)
        {
            return ret;
        }
    }
    else
    {
        if ((ret = _write_fixlen(ctx, data, datalen)) != SOFAB_RET_OK)
        {
            return ret;
        }
    }
#else
    if ((ret = _write_fixlen(ctx, data, datalen)) != SOFAB_RET_OK)
    {
        return ret;
    }
#endif /* defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ */

    return SOFAB_RET_OK;
}
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
/*!
 * @brief Shared body for the unsigned/signed varint array writers.
 *
 * Emits the field header, the element count, then each element as a varint.
 * @p is_signed selects the wire type, the sign-extending element load and the
 * ZigZag transform; the two public writers are thin wrappers over this so the
 * header/count/loop machinery is emitted only once.
 *
 * @param ctx            Output stream context.
 * @param id             Field identifier.
 * @param data           Pointer to the element array.
 * @param element_count  Number of elements.
 * @param element_size   Size of each element in bytes.
 * @param is_signed      Non-zero for the signed (ZigZag) path.
 * @return SOFAB_RET_OK on success, or an sofab_ret_t error code.
 */
SOFAB_NOINLINE
static sofab_ret_t _write_varint_array (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data,
    int32_t element_count, int32_t element_size, int is_signed)
{
    sofab_ret_t ret;

    if ((ret = _write_id_varint(ctx, id,
            is_signed ? SOFAB_TYPE_VARINTARRAY_SIGNED
                      : SOFAB_TYPE_VARINTARRAY_UNSIGNED,
            (sofab_unsigned_t)element_count)) != SOFAB_RET_OK)
    {
        return ret;
    }

    const uint8_t *ptr = (const uint8_t*)data;
    for (int32_t i = 0; i < element_count; i++)
    {
        sofab_unsigned_t enc;

        if (is_signed)
        {
            sofab_signed_t value;
            if (element_size == 1)
                value = *(const int8_t *)ptr;
            else if (element_size == 2)
                value = *(const int16_t *)ptr;
            else if (element_size == 4)
                value = *(const int32_t *)ptr;
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
            else if (element_size == 8)
                value = *(const int64_t *)ptr;
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */
            else
                // unsupported element size (8 requires 64-bit value support)
                return SOFAB_RET_E_ARGUMENT;

            enc = _zigzag_encode(value);
        }
        else
        {
            if (element_size == 1)
                enc = *(const uint8_t *)ptr;
            else if (element_size == 2)
                enc = *(const uint16_t *)ptr;
            else if (element_size == 4)
                enc = *(const uint32_t *)ptr;
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
            else if (element_size == 8)
                enc = *(const uint64_t *)ptr;
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */
            else
                // unsupported element size (8 requires 64-bit value support)
                return SOFAB_RET_E_ARGUMENT;
        }

        if (_varint_encode(ctx, enc) < 0)
        {
            return SOFAB_RET_E_BUFFER_FULL;
        }

        ptr += element_size;
    }

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_ostream_write_array_of_unsigned (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data,
    int32_t element_count, int32_t element_size)
{
    assert(ctx != NULL);
    assert(element_count >= 0); /* zero-count arrays are legal */
    assert(data != NULL || element_count == 0); /* data unused when empty */
    assert(element_size > 0);

    return _write_varint_array(ctx, id, data, element_count, element_size, 0);
}

extern sofab_ret_t sofab_ostream_write_array_of_signed (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data,
    int32_t element_count, int32_t element_size)
{
    assert(ctx != NULL);
    assert(element_count >= 0); /* zero-count arrays are legal */
    assert(data != NULL || element_count == 0); /* data unused when empty */
    assert(element_size > 0);

    return _write_varint_array(ctx, id, data, element_count, element_size, 1);
}

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
extern sofab_ret_t sofab_ostream_write_array_of_fixlen (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data,
    int32_t element_count, int32_t element_size,
    sofab_fixlentype_t type)
{
    sofab_ret_t ret;

    assert(ctx != NULL);
    assert(element_count >= 0); /* zero-count arrays are legal */
    assert(data != NULL || element_count == 0); /* data unused when empty */
    assert(element_size > 0);

    // only FP32 and FP64 are supported for fixlen arrays
    assert(type <= SOFAB_FIXLENTYPE_FP64);

    if ((ret = _write_id_varint(ctx, id, SOFAB_TYPE_FIXLENARRAY,
            (sofab_unsigned_t)element_count)) != SOFAB_RET_OK)
    {
        return ret;
    }

    // The shared fixlen_word (element width + subtype) is ALWAYS written, even
    // for a zero-count array. Otherwise an empty fp32 and an empty fp64 array
    // would be wire-identical ([header][count=0]) and a decoder could not tell
    // them apart. A zero-count array is thus [header][count=0][fixlen_word], with
    // no payload.
    if (_varint_encode(ctx, _type_encode(element_size, type)) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

    const uint8_t *ptr = (const uint8_t*)data;
    for (int32_t i = 0; i < element_count; i++)
    {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        if (type == SOFAB_FIXLENTYPE_FP32 || type == SOFAB_FIXLENTYPE_FP64)
        {
            if ((ret = _write_fixlen_reverse(ctx, ptr, element_size)) != SOFAB_RET_OK)
            {
                return ret;
            }
        }
        else
        {
            if ((ret = _write_fixlen(ctx, ptr, element_size)) != SOFAB_RET_OK)
            {
                return ret;
            }
        }
#else
        if ((ret = _write_fixlen(ctx, ptr, element_size)) != SOFAB_RET_OK)
        {
            return ret;
        }
#endif /* defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ */
        ptr += element_size;
    }

    return SOFAB_RET_OK;
}
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
extern sofab_ret_t sofab_ostream_write_sequence_begin (sofab_ostream_t *ctx, sofab_id_t id)
{
    sofab_ret_t ret;

    assert(ctx != NULL);

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_SEQUENCE_START)) != SOFAB_RET_OK)
    {
        return ret;
    }

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_ostream_write_sequence_end (sofab_ostream_t *ctx)
{
    sofab_ret_t ret;

    assert(ctx != NULL);

    if ((ret = _write_id_type(ctx, 0, SOFAB_TYPE_SEQUENCE_END)) != SOFAB_RET_OK)
    {
        return ret;
    }

    return SOFAB_RET_OK;
}
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */
