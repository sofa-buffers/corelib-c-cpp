/*!
 * @file ostream.c
 * @brief SofaBuffers C - Output stream encoder for Sofab messages.
 *
 * SPDX-License-Identifier: MIT
 */

#define _SOFAB_OSTREAM_C

/* includes *******************************************************************/
#include "sofab/ostream.h"

#include <assert.h>

/* constants ******************************************************************/

/* macros *********************************************************************/

/* types **********************************************************************/

/* prototypes *****************************************************************/

/* static vars ****************************************************************/

/* functions ******************************************************************/

/* ZigZag encode (signed -> unsigned) */
static inline sofab_unsigned_t _zigzag_encode (sofab_signed_t v)
{
    const int bits = sizeof(v) * 8;
    return ((sofab_unsigned_t)(v << 1)) ^ (sofab_unsigned_t)(v >> (bits - 1));
}

/* Pushs a single byte to the buffer, flush if needed.
 * Returns 0 on success, -1 on overflow.
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

/* Write unsigned variable length integer to buffer.
 * Returns 0 on success, negative on overflow.
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

/* Encode type into a variable */
static sofab_unsigned_t _type_encode (sofab_unsigned_t var, int type)
{
    return ((var << 3) | (type & 0x07));
}

/* Write ID and type to buffer as varint */
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

/* Write fixed-length data to buffer */
static sofab_ret_t _write_fixlen (sofab_ostream_t *ctx, const uint8_t *data, int32_t datalen)
{
    for (int32_t i = 0; i < datalen; i++)
    {
        if (_push_byte(ctx, data[i]) != 0)
        {
            return SOFAB_RET_E_BUFFER_FULL;
        }
    }

    return SOFAB_RET_OK;
}

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
/* Write fixed-length data to buffer in reverse byte order */
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
#endif

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
    sofab_ret_t ret;

    assert(ctx != NULL);

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_VARINT_UNSIGNED)) != SOFAB_RET_OK)
    {
        return ret;
    }

    if (_varint_encode(ctx, value) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_ostream_write_signed (
    sofab_ostream_t *ctx, sofab_id_t id, sofab_signed_t value)
{
    sofab_ret_t ret;

    assert(ctx != NULL);

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_VARINT_SIGNED)) != SOFAB_RET_OK)
    {
        return ret;
    }

    if (_varint_encode(ctx, _zigzag_encode(value)) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_ostream_write_fixlen (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data, int32_t datalen,
    sofab_fixlentype_t type)
{
    sofab_ret_t ret;

    assert(ctx != NULL);
    assert(datalen == 0 || data != NULL);

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_FIXLEN)) != SOFAB_RET_OK)
    {
        return ret;
    }

    if (_varint_encode(ctx, _type_encode(datalen, type)) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
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
#endif

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_ostream_write_array_of_unsigned (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data,
    int32_t element_count, int32_t element_size)
{
    sofab_ret_t ret;

    assert(ctx != NULL);
    assert(data != NULL);
    assert(element_count > 0);
    assert(element_size > 0);

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_VARINTARRAY_UNSIGNED)) != SOFAB_RET_OK)
    {
        return ret;
    }

    if (_varint_encode(ctx, element_count) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

    const uint8_t *ptr = (const uint8_t*)data;
    for (int32_t i = 0; i < element_count; i++)
    {
        uint64_t value;

        if (element_size == 1)
            value = *(uint8_t *)ptr;
        else if (element_size == 2)
            value = *(uint16_t *)ptr;
        else if (element_size == 4)
            value = *(uint32_t *)ptr;
        else if (element_size == 8)
            value = *(uint64_t *)ptr;
        else
        {
            // unsupported element size
            return SOFAB_RET_E_ARGUMENT;
        }

        if (_varint_encode(ctx, value) < 0)
        {
            return SOFAB_RET_E_BUFFER_FULL;
        }

        ptr += element_size;
    }

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_ostream_write_array_of_signed (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data,
    int32_t element_count, int32_t element_size)
{
    sofab_ret_t ret;

    assert(ctx != NULL);
    assert(data != NULL);
    assert(element_count > 0);
    assert(element_size > 0);

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_VARINTARRAY_SIGNED)) != SOFAB_RET_OK)
    {
        return ret;
    }

    if (_varint_encode(ctx, element_count) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

    const uint8_t *ptr = (const uint8_t*)data;
    for (int32_t i = 0; i < element_count; i++)
    {
        int64_t value;

        if (element_size == 1)
            value = *(int8_t *)ptr;
        else if (element_size == 2)
            value = *(int16_t *)ptr;
        else if (element_size == 4)
            value = *(int32_t *)ptr;
        else if (element_size == 8)
            value = *(int64_t *)ptr;
        else
        {
            // unsupported element size
            return SOFAB_RET_E_ARGUMENT;
        }

        if (_varint_encode(ctx, _zigzag_encode(value)) < 0)
        {
            return SOFAB_RET_E_BUFFER_FULL;
        }

        ptr += element_size;
    }

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_ostream_write_array_of_fixlen (
    sofab_ostream_t *ctx, sofab_id_t id, const void *data,
    int32_t element_count, int32_t element_size,
    sofab_fixlentype_t type)
{
    sofab_ret_t ret;

    assert(ctx != NULL);
    assert(data != NULL);
    assert(element_count > 0);
    assert(element_size > 0);

    // only FP32 and FP64 are supported for fixlen arrays
    assert(type <= SOFAB_FIXLENTYPE_FP64);

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_FIXLENARRAY)) != SOFAB_RET_OK)
    {
        return ret;
    }

    if (_varint_encode(ctx, element_count) < 0)
    {
        return SOFAB_RET_E_BUFFER_FULL;
    }

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
#endif
        ptr += element_size;
    }

    return SOFAB_RET_OK;
}

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
