/*!
 * @file istream.c
 * @brief SofaBuffers C - Input stream decoder for Sofab messages.
 *
 * SPDX-License-Identifier: MIT
 */

#define _SOFAB_C

/* includes *******************************************************************/
#include "sofab/istream.h"

#include <assert.h>

/* constants ******************************************************************/
#define _CHECK_INT_OVERFLOW 1

/* macros *********************************************************************/
#define _OPT_FIELDTYPE(type)    ((type) & 0x07)
#define _OPT_FIXLENTYPE(type)   (((type) >> 3) & 0x07)
#define _OPT_STRINGTERM(type)   ((type) & 0x40)

#if _CHECK_INT_OVERFLOW == 1
# define _FITS_UNSIGNED_CHECK(val, bits) \
    if (!_fits_unsigned_n((val), (bits))) return SOFAB_RET_E_INVALID_MSG;

# define _FITS_SIGNED_CHECK(val, bits) \
    if (!_fits_signed_n((val), (bits))) return SOFAB_RET_E_INVALID_MSG;
#else
# define _FITS_UNSIGNED_CHECK(val, bits)   do {} while (0)
# define _FITS_SIGNED_CHECK(val, bits)     do {} while (0)
#endif

/* types **********************************************************************/
typedef enum
{
    _DECODER_STATE_IDLE,
    _DECODER_STATE_VARINT_UNSIGNED,
    _DECODER_STATE_VARINT_SIGNED,
    _DECODER_STATE_FIXLEN_LEN,
    _DECODER_STATE_FIXLEN_VAL,
    _DECODER_STATE_FIXLEN_RAW,
    _DECODER_STATE_ARRAY_COUNT,
    _DECODER_STATE_ARRAY_FIXLEN,
} _decoder_state_t;

// compile-time checks for float and double sizes
typedef char float_size_check[(sizeof(float) == 4) ? 1 : -1];
typedef char double_size_check[(sizeof(double) == 8) ? 1 : -1];

/* prototypes *****************************************************************/

/* static vars ****************************************************************/

/* functions ******************************************************************/

/* ZigZag decode (unsigned -> signed) */
static sofab_signed_t _zigzag_decode (sofab_unsigned_t u)
{
    return (sofab_signed_t)((u >> 1) ^ (-(sofab_signed_t)(u & 1)));
}

/* Read unsigned varlen integer value.
 * Returns 0 on success, -1 need more data, -2 on overflow.
 */
static int _varint_decode (sofab_istream_t *ctx, uint8_t byte, sofab_unsigned_t *out_value)
{
    ctx->varint_value |= ((sofab_unsigned_t)(byte & 0x7F)) << ctx->varint_shift;
    ctx->varint_shift += 7;

    if ((byte & 0x80) == 0)
    {
        *out_value = ctx->varint_value;
        ctx->varint_value = 0;
        ctx->varint_shift = 0;

        return 0;
    }

    // overflow or need more data
    const int bits = sizeof(sofab_unsigned_t) * 8;
    return (ctx->varint_shift >= bits) ? -2 : -1;
}

#if _CHECK_INT_OVERFLOW == 1
static int _fits_unsigned_n (sofab_unsigned_t x, int n)
{
    if (n == sizeof(sofab_unsigned_t) * 8) return 1;
    return (x >> n) == 0;
}

static int _fits_signed_n (sofab_signed_t x, int n)
{
    if (n == sizeof(sofab_unsigned_t) * 8) return 1;
    return (x >> (n - 1)) == (x >> 63);
}
#endif

static uint8_t _type_decode (sofab_unsigned_t *value)
{
    uint8_t type = (uint8_t)(*value & 0x07);
    *value >>= 3;

    return type;
}

/* Read fixed-length data from the stream.
 * Returns 0 on success, >0 need more data.
 */
static size_t _read_fixlen (sofab_istream_t *ctx, uint8_t byte)
{
    // if interested in field ...
    if (ctx->target_ptr && ctx->fixlen_remaining)
    {
        // store byte in target buffer
        *(ctx->target_ptr) = byte;
        ctx->target_ptr++;
    }

    // decrease remaining source length
    ctx->fixlen_remaining--;

    return ctx->fixlen_remaining;
}

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static size_t _read_fixlen_reverse (sofab_istream_t *ctx, uint8_t byte)
{
    // if interested in field ...
    if (ctx->target_ptr)
    {
        // store byte in target buffer in reverse order
        uint8_t *ptr = ctx->target_ptr + (ctx->fixlen_remaining - 1);
        *ptr = byte;
    }

    // decrease remaining source length
    ctx->fixlen_remaining--;

    // if done reading all bytes in reverse order ...
    if (ctx->fixlen_remaining == 0)
    {
        // move target pointer forward
        ctx->target_ptr += ctx->target_len;
    }

    return ctx->fixlen_remaining;
}
#endif

static sofab_ret_t _call_field_callback (
    sofab_istream_t *ctx)
{
    uint8_t field_opt = ctx->target_opt;

    // field is ignored, so let's skip all children
    if (ctx->decoder->skip_depth > 0)
    {
        return SOFAB_RET_OK;
    }

    // call field callback to notify about new field with size
    ctx->decoder->field_callback(
        ctx, ctx->id, ctx->target_len, ctx->target_count, ctx->decoder->usrptr);

    // after call:
    //   target_ptr can be NULL (not interested in field)
    // otherwise:
    //   target_ptr    points to the buffer to store the value
    //   target_len    is the size of the target buffer or size of one array element
    //   target_count  is the number of elements in the target array
    //   target_opts   is the expected field and fixlen type

    // if interested in field ...
    if (ctx->target_ptr)
    {
        // ctx->target_opts can be changed by the callback via read functions,
        // so verify that it matches the actual field type
        if ((ctx->target_opt ^ field_opt) & 0x3F)
        {
            // target type mismatch
            return SOFAB_RET_E_USAGE;
        }
    }

    return SOFAB_RET_OK;
}

//

extern void sofab_istream_init (
    sofab_istream_t *ctx, sofab_istream_field_cb_t field_callback, void *usrptr)
{
    assert(ctx != NULL);
    assert(field_callback != NULL);

    memset(ctx, 0, sizeof(*ctx));

    // setup default decoder
    ctx->decoder = &ctx->default_decoder;
    ctx->decoder->field_callback = field_callback;
    ctx->decoder->usrptr = usrptr;
}

extern sofab_ret_t sofab_istream_feed (sofab_istream_t *ctx, const void *data, size_t datalen)
{
    int dec;

    assert(ctx != NULL);
    assert(data != NULL);
    assert(datalen > 0);

    const uint8_t *p;
    for (p = (const uint8_t *)data; datalen > 0; p++, datalen--)
    {
        switch (ctx->decoder->state)
        {
            case _DECODER_STATE_IDLE:
            {
                sofab_unsigned_t id;

                // read field id + type
                if ((dec = _varint_decode(ctx, *p, &id)) == -1)
                {
                    // need more data
                    continue;
                }
                else if (dec < 0)
                {
                    // varint overflow
                    return SOFAB_RET_E_INVALID_MSG;
                }

                // extract type from id
                uint8_t type = _type_decode(&id);
                if (id > SOFAB_ID_MAX)
                {
                    // invalid field id
                    return SOFAB_RET_E_INVALID_MSG;
                }

                // store current field id and type
                ctx->id = (sofab_id_t)(id);
                ctx->target_opt = type;

                // reset target infos used in field callback
                ctx->target_ptr = NULL;
                ctx->target_len = 0;
                ctx->target_count = 0;

                if (type != SOFAB_TYPE_FIXLEN &&
                    type != SOFAB_TYPE_VARINTARRAY_UNSIGNED &&
                    type != SOFAB_TYPE_VARINTARRAY_SIGNED &&
                    type != SOFAB_TYPE_FIXLENARRAY &&
                    type != SOFAB_TYPE_SEQUENCE_END)
                {
                    sofab_ret_t ret;

                    if ((ret = _call_field_callback(ctx)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }
                }

                // no default case, since all 3bit are handled
                switch (type)
                {
                    case SOFAB_TYPE_VARINT_UNSIGNED:
                        ctx->decoder->state = _DECODER_STATE_VARINT_UNSIGNED;
                        break;

                    case SOFAB_TYPE_VARINT_SIGNED:
                        ctx->decoder->state = _DECODER_STATE_VARINT_SIGNED;
                        break;

                    case SOFAB_TYPE_FIXLEN:
                        ctx->decoder->state = _DECODER_STATE_FIXLEN_LEN;
                        break;

                    case SOFAB_TYPE_VARINTARRAY_UNSIGNED:
                    case SOFAB_TYPE_VARINTARRAY_SIGNED:
                    case SOFAB_TYPE_FIXLENARRAY:
                        ctx->decoder->state = _DECODER_STATE_ARRAY_COUNT;
                        break;

                    case SOFAB_TYPE_SEQUENCE_START:
                        // if not interested in sequence ...
                        if (!ctx->target_ptr)
                        {
                            if (ctx->decoder->skip_depth == UINT8_MAX)
                            {
                                // skip depth overflow
                                return SOFAB_RET_E_INVALID_MSG;
                            }

                            // increase skip_depth counter to skip all fields until sequence end
                            ctx->decoder->skip_depth++;
                        }

                        ctx->decoder->state = _DECODER_STATE_IDLE;
                        break;

                    case SOFAB_TYPE_SEQUENCE_END:
                        // if decoder is skipping fields ...
                        if (ctx->decoder->skip_depth > 0)
                        {
                            // decrease skip_depth counter
                            ctx->decoder->skip_depth--;
                        }
                        else
                        {
                            if (ctx->decoder->parent == NULL)
                            {
                                // no parent decoder to return to,
                                // due to invalid sequence end
                                return SOFAB_RET_E_INVALID_MSG;
                            }

                            // pop decoder
                            ctx->decoder = ctx->decoder->parent;
                        }

                        ctx->decoder->state = _DECODER_STATE_IDLE;
                        break;
                }

                break;
            }

            case _DECODER_STATE_VARINT_UNSIGNED:
            {
                sofab_unsigned_t unsigned_value;

                // read varint value
                if ((dec = _varint_decode(ctx, *p, &unsigned_value)) == -1)
                {
                    // need more data
                    continue;
                }
                else if (dec < 0)
                {
                    // varint overflow
                    return SOFAB_RET_E_INVALID_MSG;
                }

                if (ctx->target_ptr)
                {
                    // store unsigned value in target buffer
                    if (ctx->target_len == 1)
                        *((uint8_t *)ctx->target_ptr) = (uint8_t)(unsigned_value);
                    else if (ctx->target_len == 2)
                        *((uint16_t *)ctx->target_ptr) = (uint16_t)(unsigned_value);
                    else if (ctx->target_len == 4)
                        *((uint32_t *)ctx->target_ptr) = (uint32_t)(unsigned_value);
                    else if (ctx->target_len == 8)
                        *((uint64_t *)ctx->target_ptr) = (uint64_t)(unsigned_value);
                    else
                    {
                        // invalid target length
                        return SOFAB_RET_E_USAGE;
                    }

                    // optional: check integer overflow
                    _FITS_UNSIGNED_CHECK(unsigned_value, ctx->target_len * 8);
                }

                // if decoding an array of unsigned values ...
                if (_OPT_FIELDTYPE(ctx->target_opt) == SOFAB_TYPE_VARINTARRAY_UNSIGNED)
                {
                    // decode next array element
                    if (ctx->target_ptr) ctx->target_ptr += ctx->target_len;

                    ctx->target_count--;
                    if (ctx->target_count > 0)
                    {
                        // more array elements to read
                        continue;
                    }
                }

                // go back to idle
                ctx->decoder->state = _DECODER_STATE_IDLE;
                break;
            }

            case _DECODER_STATE_VARINT_SIGNED:
            {
                sofab_unsigned_t zigzag_value;

                if ((dec = _varint_decode(ctx, *p, &zigzag_value)) == -1)
                {
                    // need more data
                    continue;
                }
                else if (dec < 0)
                {
                    // varint overflow
                    return SOFAB_RET_E_INVALID_MSG;
                }

                if (ctx->target_ptr)
                {
                    // zigzag decode
                    sofab_signed_t signed_value = _zigzag_decode(zigzag_value);

                    // store signed value in target buffer
                    if (ctx->target_len == 1)
                        *((int8_t *)ctx->target_ptr) = (int8_t)(signed_value);
                    else if (ctx->target_len == 2)
                        *((int16_t *)ctx->target_ptr) = (int16_t)(signed_value);
                    else if (ctx->target_len == 4)
                        *((int32_t *)ctx->target_ptr) = (int32_t)(signed_value);
                    else if (ctx->target_len == 8)
                        *((int64_t *)ctx->target_ptr) = (int64_t)(signed_value);
                    else
                    {
                        // invalid target length
                        return SOFAB_RET_E_USAGE;
                    }

                    // optional: check integer overflow
                    _FITS_SIGNED_CHECK(signed_value, ctx->target_len * 8);
                }

                // if decoding an array of signed values ...
                if (_OPT_FIELDTYPE(ctx->target_opt) == SOFAB_TYPE_VARINTARRAY_SIGNED)
                {
                    // decode next array element
                    if (ctx->target_ptr) ctx->target_ptr += ctx->target_len;

                    ctx->target_count--;
                    if (ctx->target_count > 0)
                    {
                        // more array elements to read
                        continue;
                    }
                }

                // go back to idle
                ctx->decoder->state = _DECODER_STATE_IDLE;
                break;
            }

            case _DECODER_STATE_FIXLEN_LEN:
            {
                sofab_ret_t ret;
                sofab_unsigned_t fixlen_length;

                // read fixlen length + type
                if ((dec = _varint_decode(ctx, *p, &fixlen_length)) == -1)
                {
                    // need more data
                    continue;
                }
                else if (dec < 0)
                {
                    // varint overflow
                    return SOFAB_RET_E_INVALID_MSG;
                }

                // extract type from length
                uint8_t fixlen_type = _type_decode(&fixlen_length);
                if (fixlen_type > SOFAB_FIXLENTYPE_BLOB)
                {
                    // invalid fixlen type
                    return SOFAB_RET_E_INVALID_MSG;
                }

                if (fixlen_length > SOFAB_FIXLEN_MAX)
                {
                    // invalid fixlen length in message
                    return SOFAB_RET_E_INVALID_MSG;
                }

                // store fixlen type and length
                ctx->target_opt |= fixlen_type << 3;
                size_t length = (size_t)(fixlen_length);
                ctx->target_len = length;

                // fixlen filed with zero length are allowed
                // e.g., empty strings or blobs

                if ((ret = _call_field_callback(ctx)) != SOFAB_RET_OK)
                {
                    return ret;
                }

                if (ctx->target_ptr)
                {
                    if (_OPT_STRINGTERM(ctx->target_opt))
                    {
                        if (length > ctx->target_len - 1)
                        {
                            // message too long to be stored as null-terminated string
                            return SOFAB_RET_E_INVALID_MSG;
                        }

                        // add null-terminator after string
                        ctx->target_ptr[length] = '\0';
                    }
                    else
                    {
                        if (length > ctx->target_len)
                        {
                            // message too long to be stored in target buffer
                            return SOFAB_RET_E_INVALID_MSG;
                        }
                    }
                }

                if (length)
                {
                    // store source length to know how many bytes to consume
                    ctx->fixlen_remaining = length;

                    switch (fixlen_type)
                    {
                        case SOFAB_FIXLENTYPE_FP32:
                        case SOFAB_FIXLENTYPE_FP64:
                            ctx->decoder->state = _DECODER_STATE_FIXLEN_VAL;
                            break;

                        case SOFAB_FIXLENTYPE_STRING:
                        case SOFAB_FIXLENTYPE_BLOB:
                            ctx->decoder->state = _DECODER_STATE_FIXLEN_RAW;
                            break;
                    }
                }
                else
                {
                    // go back to idle
                    ctx->decoder->state = _DECODER_STATE_IDLE;
                    break;
                }

                break;
            }

            case _DECODER_STATE_FIXLEN_VAL:
            {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                if (_read_fixlen_reverse(ctx, *p) > 0)
                {
                    // need more data
                    continue;
                }
#else
                if (_read_fixlen(ctx, *p) > 0)
                {
                    // need more data
                    continue;
                }
#endif

                if (_OPT_FIELDTYPE(ctx->target_opt) == SOFAB_TYPE_FIXLENARRAY)
                {
                    ctx->target_count--;
                    if (ctx->target_count > 0)
                    {
                        // restore source length to read next element
                        ctx->fixlen_remaining = ctx->target_len;

                        // more array elements to read
                        continue;
                    }
                }

                // go back to idle
                ctx->decoder->state = _DECODER_STATE_IDLE;
                break;
            }

            case _DECODER_STATE_FIXLEN_RAW:
            {
                if (_read_fixlen(ctx, *p) > 0)
                {
                    // need more data
                    continue;
                }

                // go back to idle
                ctx->decoder->state = _DECODER_STATE_IDLE;
                break;
            }

            case _DECODER_STATE_ARRAY_COUNT:
            {
                sofab_ret_t ret;
                sofab_unsigned_t array_count;

                // read array element count
                if ((dec = _varint_decode(ctx, *p, &array_count)) == -1)
                {
                    // need more data
                    continue;
                }
                else if (dec < 0)
                {
                    // varint overflow
                    return SOFAB_RET_E_INVALID_MSG;
                }

                if (array_count > SOFAB_ARRAY_MAX)
                {
                    // invalid array count in message
                    return SOFAB_RET_E_INVALID_MSG;
                }

                // store array element count
                size_t count = (size_t)(array_count);
                ctx->target_count = count;

                if (count == 0)
                {
                    // arrays with zero elements are not allowed
                    return SOFAB_RET_E_INVALID_MSG;
                }

                if (_OPT_FIELDTYPE(ctx->target_opt) != SOFAB_TYPE_FIXLENARRAY)
                {
                    if ((ret = _call_field_callback(ctx)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }
                }

                if (ctx->target_ptr)
                {
                    if (count != ctx->target_count)
                    {
                        // array size missmatch
                        return SOFAB_RET_E_INVALID_MSG;
                    }
                }

                // read array elements (see IDLE state for type check)
                uint8_t type = _OPT_FIELDTYPE(ctx->target_opt);
                if (type == SOFAB_TYPE_VARINTARRAY_UNSIGNED)
                    ctx->decoder->state = _DECODER_STATE_VARINT_UNSIGNED;
                else if (type == SOFAB_TYPE_VARINTARRAY_SIGNED)
                    ctx->decoder->state = _DECODER_STATE_VARINT_SIGNED;
                else if (type == SOFAB_TYPE_FIXLENARRAY)
                    ctx->decoder->state = _DECODER_STATE_FIXLEN_LEN;

                break;
            }
        }
    }

    return SOFAB_RET_OK;
}

extern void sofab_istream_read_field (
    sofab_istream_t *ctx, void *var, size_t varlen, uint8_t opt)
{
    assert(ctx != NULL);
    assert(var != NULL);
    assert(varlen > 0);

    ctx->target_ptr = (uint8_t *)var;
    ctx->target_len = varlen;
    ctx->target_opt = opt;
}

extern void sofab_istream_read_array (
    sofab_istream_t *ctx, void *var,
    size_t element_count, size_t element_size, uint8_t opt)
{
    assert(ctx != NULL);
    assert(var != NULL);
    assert(element_count > 0);
    assert(element_size > 0);

    ctx->target_ptr = (uint8_t *)var;
    ctx->target_count = element_count;
    ctx->target_len = element_size;
    ctx->target_opt = opt;
}

extern void sofab_istream_read_sequence (
    sofab_istream_t *ctx, sofab_istream_decoder_t *decoder,
    sofab_istream_field_cb_t field_callback, void *usrptr)
{
    assert(ctx != NULL);
    assert(decoder != NULL);
    assert(field_callback != NULL);

    memset(decoder, 0, sizeof(*decoder));

    // setup decoder for sequence
    decoder->field_callback = field_callback;
    decoder->usrptr = usrptr;
    decoder->parent = ctx->decoder;

    ctx->decoder = decoder;
    ctx->target_ptr = (uint8_t *)decoder; // just to have a non-NULL value
    ctx->target_opt = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_SEQUENCE_START);
}
