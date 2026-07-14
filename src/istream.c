/*!
 * @file istream.c
 * @brief SofaBuffers C - Input stream decoder for Sofab messages.
 *
 * SPDX-License-Identifier: MIT
 */

#define SOFAB_C

/* includes *******************************************************************/
#include "sofab/istream.h"

#include <assert.h>

/* constants ******************************************************************/

/* macros *********************************************************************/
#define _OPT_FIELDTYPE(type)    ((type) & 0x07)
#define _OPT_FIXLENTYPE(type)   (((type) >> 3) & 0x07)
#define _OPT_STRINGTERM(type)   ((type) & 0x40)

#if !defined(SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK)
# define _FITS_UNSIGNED_CHECK(val, bits) \
    if (!_fits_unsigned_n((val), (bits))) return SOFAB_RET_E_INVALID_MSG;

# define _FITS_SIGNED_CHECK(val, bits) \
    if (!_fits_signed_n((val), (bits))) return SOFAB_RET_E_INVALID_MSG;
#else
# define _FITS_UNSIGNED_CHECK(val, bits)   do {} while (0)
# define _FITS_SIGNED_CHECK(val, bits)     do {} while (0)
#endif /* !defined(SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK) */

/*!
 * @brief Keep a shared helper out of line so its one copy is reused.
 *
 * Forcing the shared decode helper out of line makes the toolchain emit a
 * single copy instead of inlining (and thus duplicating) it into every caller
 * state — a net size win on the small targets this corelib targets. Falls back
 * to nothing on compilers without the attribute (still correct).
 */
#if defined(__GNUC__)
# define SOFAB_NOINLINE __attribute__((noinline))
#else
# define SOFAB_NOINLINE
#endif

/* types **********************************************************************/
typedef enum
{
    /* Varint-decoding states come first and stay contiguous: each one begins by
     * pulling a single LEB128 varint from the stream, so they share one decode
     * preamble in sofab_istream_feed (state <= _DECODER_STATE_ARRAY_COUNT). The
     * raw-byte states, which consume payload bytes directly, follow after. */
    _DECODER_STATE_IDLE,
    _DECODER_STATE_VARINT_UNSIGNED,
    _DECODER_STATE_VARINT_SIGNED,
    _DECODER_STATE_FIXLEN_LEN,
    _DECODER_STATE_ARRAY_COUNT,
    /* raw-byte (non-varint-decoding) states follow _DECODER_STATE_ARRAY_COUNT */
    _DECODER_STATE_FIXLEN_VAL,
    _DECODER_STATE_FIXLEN_RAW,
} _decoder_state_t;

/* prototypes *****************************************************************/

/* static vars ****************************************************************/

/* functions ******************************************************************/

/*!
 * @brief ZigZag-decode an unsigned value back to signed.
 *
 * Inverse of the encoder's ZigZag transform.
 *
 * @param u  ZigZag-encoded unsigned value.
 * @return The decoded signed value.
 */
static sofab_signed_t _zigzag_decode (sofab_unsigned_t u)
{
    return (sofab_signed_t)((u >> 1) ^ (-(sofab_signed_t)(u & 1)));
}

/*!
 * @brief Feed one byte into the in-progress varint accumulator.
 *
 * Accumulates 7-bit groups across calls; rejects values too wide for
 * @ref sofab_unsigned_t (checked at the type boundary to avoid dropping bits).
 *
 * @param ctx        Input stream context (holds the accumulator state).
 * @param byte       Next varint byte.
 * @param out_value  Receives the decoded value when the varint completes.
 * @return 0 when a value completed (stored in @p out_value), -1 if more bytes
 *         are needed, -2 on overflow (value wider than the value type).
 */
static int _varint_decode (sofab_istream_t *ctx, uint8_t byte, sofab_unsigned_t *out_value)
{
    const int bits = sizeof(sofab_unsigned_t) * 8;

    // already consumed a full value type's worth of bits => value too wide
    if (ctx->varint_shift >= bits)
    {
        return -2;
    }

    // bits still available before this 7-bit chunk; if the chunk carries bits
    // beyond the value width, the value does not fit. Checking here (rather than
    // after the shift) avoids silently dropping the high bits, which matters at
    // the type boundary — e.g. a 33-bit value reaching a 32-bit build.
    const int room = bits - ctx->varint_shift;
    if (room < 7 && ((byte & 0x7F) >> room) != 0)
    {
        return -2;
    }

    ctx->varint_value |= ((sofab_unsigned_t)(byte & 0x7F)) << ctx->varint_shift;
    ctx->varint_shift += 7;

    if ((byte & 0x80) == 0)
    {
        *out_value = ctx->varint_value;
        ctx->varint_value = 0;
        ctx->varint_shift = 0;

        return 0;
    }

    // need more data
    return -1;
}

#if !defined(SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK)
/*!
 * @brief Test whether an unsigned value fits in @p n bits.
 *
 * @param x  Value to check.
 * @param n  Target width in bits.
 * @return 1 if @p x fits in @p n bits, 0 otherwise.
 */
static int _fits_unsigned_n (sofab_unsigned_t x, int n)
{
    const int bits = sizeof(sofab_unsigned_t) * 8;
    /* A target wider than the value type always fits; using >= (not ==) also
     * avoids an undefined x >> n shift when the value is narrower than the
     * target (e.g. a 32-bit value read into a 64-bit slot). */
    if (n >= bits) return 1;
    return (x >> n) == 0;
}

/*!
 * @brief Test whether a signed value fits in @p n bits (two's complement).
 *
 * @param x  Value to check.
 * @param n  Target width in bits.
 * @return 1 if @p x fits in @p n bits, 0 otherwise.
 */
static int _fits_signed_n (sofab_signed_t x, int n)
{
    const int bits = sizeof(sofab_signed_t) * 8;
    if (n >= bits) return 1;
    return (x >> (n - 1)) == (x >> (bits - 1));
}
#endif /* !defined(SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK) */

/*!
 * @brief Store a decoded scalar's low @p len bytes into the target buffer.
 *
 * The 1/2/4/8-byte store truncates to the width's low bytes, whose bit pattern
 * is identical for the signed and unsigned varint paths — so a single helper
 * serves both (the signed path passes its value reinterpreted as unsigned).
 *
 * @param p      Destination buffer.
 * @param len    Target width in bytes (1, 2, 4 or 8).
 * @param value  Value whose low @p len bytes are written.
 * @return 0 on success, -1 if @p len is not a supported width.
 */
SOFAB_NOINLINE
static int _store_scalar (uint8_t *p, size_t len, sofab_unsigned_t value)
{
    if (len == 1)
        *((uint8_t *)p) = (uint8_t)(value);
    else if (len == 2)
        *((uint16_t *)p) = (uint16_t)(value);
    else if (len == 4)
        *((uint32_t *)p) = (uint32_t)(value);
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
    else if (len == 8)
        *((uint64_t *)p) = (uint64_t)(value);
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */
    else
        return -1;

    return 0;
}

/*!
 * @brief Split a 3-bit type tag off a decoded header word.
 *
 * Reads the low 3 bits as the type and shifts @p value right by 3 in place,
 * leaving the remaining id/length.
 *
 * @param value  In/out: header word; on return holds the value without the type bits.
 * @return The extracted 3-bit type tag.
 */
static uint8_t _type_decode (sofab_unsigned_t *value)
{
    uint8_t type = (uint8_t)(*value & 0x07);
    *value >>= 3;

    return type;
}

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
/*!
 * @brief Consume one byte of a fixed-length field, storing it if a target is bound.
 *
 * @param ctx   Input stream context.
 * @param byte  Next payload byte.
 * @return Number of payload bytes still remaining (0 when the field is complete).
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
/*!
 * @brief Consume one byte of a fixed-length field, storing it in reversed order.
 *
 * Big-endian counterpart of @ref _read_fixlen for little-endian float payloads;
 * advances the target pointer once the field completes.
 *
 * @param ctx   Input stream context.
 * @param byte  Next payload byte.
 * @return Number of payload bytes still remaining (0 when the field is complete).
 */
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
    if (ctx->target_ptr && ctx->fixlen_remaining == 0)
    {
        // move target pointer forward
        ctx->target_ptr += ctx->target_len;
    }

    return ctx->fixlen_remaining;
}
#endif /* defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ */
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

/*!
 * @brief Invoke the active decoder's field callback for the current field.
 *
 * Skips the callback while inside an ignored sequence. After the callback runs,
 * verifies that the field/fixlen type bound by any read matches the actual
 * field on the wire.
 *
 * @param ctx  Input stream context.
 * @return SOFAB_RET_OK on success, or SOFAB_RET_E_USAGE if the bound read type
 *         does not match the decoded field type.
 */
static sofab_ret_t _call_field_callback_masked (
    sofab_istream_t *ctx, uint8_t type_mask)
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
        // so verify that it matches the actual field type.
        //
        // type_mask selects which bits must match:
        // 0x3F = field type [0..2] + fixlen subtype [3..5];
        // 0x07 = field type only
        if ((ctx->target_opt ^ field_opt) & type_mask)
        {
            // target type mismatch
            return SOFAB_RET_E_USAGE;
        }
    }

    return SOFAB_RET_OK;
}

static sofab_ret_t _call_field_callback (
    sofab_istream_t *ctx)
{
    // verify both the wire field type and the fixlen subtype
    return _call_field_callback_masked(ctx, 0x3F);
}

/*!
 * @brief Test whether the decoder is parked exactly on a field boundary.
 *
 * The decoder is a resumable state machine: after a feed consumes all its input
 * it is either sitting on a clean field boundary (a complete message so far) or
 * suspended mid-field / inside an open sequence, waiting for more bytes. This
 * predicate distinguishes the two so @ref sofab_istream_feed can surface a
 * three-valued outcome (COMPLETE vs INCOMPLETE) instead of silently accepting a
 * truncated tail. It is a pure inspection — nothing is finalized or rejected.
 *
 * Not-at-boundary covers every "ends inside a field" case:
 *   - a partial varint (continuation bit was set, terminating byte not yet seen)
 *     leaves @c varint_shift non-zero, even while the active state is IDLE
 *     (a lone 0x80 header byte);
 *   - a fixlen/array payload shorter than declared, or an array with elements
 *     still pending, leaves the active decoder in a non-IDLE state;
 *   - an open (unclosed) sequence leaves either a pushed child decoder
 *     (@c parent != NULL) or, for an ignored sequence, a non-zero @c skip_depth
 *     on the top-level decoder.
 *
 * @param ctx  Input stream context.
 * @return true if consumed bytes end exactly at a field boundary (COMPLETE),
 *         false if they end mid-field or with an open sequence (INCOMPLETE).
 */
static bool _at_message_boundary (const sofab_istream_t *ctx)
{
    // A field boundary means all four positional state fields are at rest.
    // _DECODER_STATE_IDLE is 0, so the three uint8 counters (varint_shift,
    // state, skip_depth) OR-reduce to a single zero test; parent stays a
    // plain (portable) null-pointer check — equivalent to four separate
    // guards, one branch instead of four.
    return (ctx->varint_shift | ctx->decoder->state | ctx->decoder->skip_depth) == 0
        && ctx->decoder->parent == NULL;
}

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
/*!
 * @brief Reconcile the wire element count with the destination the callback bound.
 *
 * On the wire an array carries its @b actual length (@c 0..N per MESSAGE_SPEC §3
 * / CORELIB_PLAN §4.7); a bound destination declares its @b capacity N in
 * @c target_count. A wire count within capacity is accepted and becomes the
 * number of elements the decoder reads — any remaining destination slots keep
 * their init/default values (MESSAGE_SPEC §5.1's pre-sized-destination model). A
 * wire count that would overflow the destination is rejected. When the callback
 * bound no destination (@c target_ptr is NULL) the field is skipped and the wire
 * count already sitting in @c target_count drives the read, so nothing changes.
 *
 * @param ctx         Input stream context (@c target_count holds the capacity).
 * @param wire_count  Element count decoded from the wire.
 * @return SOFAB_RET_OK, or SOFAB_RET_E_INVALID_MSG if @p wire_count exceeds the
 *         destination capacity.
 */
static sofab_ret_t _bind_array_count (sofab_istream_t *ctx, size_t wire_count)
{
    if (ctx->target_ptr)
    {
        if (wire_count > ctx->target_count)
        {
            // wire array longer than the destination buffer capacity
            return SOFAB_RET_E_INVALID_MSG;
        }

        // read exactly what the wire carries; trailing slots keep their defaults
        ctx->target_count = wire_count;
    }

    return SOFAB_RET_OK;
}
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

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
        // The varint-decoding states (state <= _DECODER_STATE_ARRAY_COUNT) all
        // start by pulling one LEB128 varint and reject an over-wide value the
        // same way, so that shared preamble lives here once instead of in each
        // state. The raw-byte states (FIXLEN_VAL/RAW) skip it and read *p
        // directly. `decoded` holds the completed varint for the state below.
        sofab_unsigned_t decoded = 0;
        if (ctx->decoder->state <= _DECODER_STATE_ARRAY_COUNT)
        {
            if ((dec = _varint_decode(ctx, *p, &decoded)) == -1)
            {
                // need more data
                continue;
            }
            if (dec < 0)
            {
                // varint overflow
                return SOFAB_RET_E_INVALID_MSG;
            }
        }

        switch (ctx->decoder->state)
        {
            case _DECODER_STATE_IDLE:
            {
                sofab_unsigned_t id = decoded;

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

                uint8_t callback = 0;
                switch (type)
                {
                    case SOFAB_TYPE_VARINT_UNSIGNED:
                        callback = 1;
                        ctx->decoder->state = _DECODER_STATE_VARINT_UNSIGNED;
                        break;

                    case SOFAB_TYPE_VARINT_SIGNED:
                        callback = 1;
                        ctx->decoder->state = _DECODER_STATE_VARINT_SIGNED;
                        break;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                    case SOFAB_TYPE_FIXLEN:
                        ctx->decoder->state = _DECODER_STATE_FIXLEN_LEN;
                        break;
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
                    case SOFAB_TYPE_VARINTARRAY_UNSIGNED:
                    case SOFAB_TYPE_VARINTARRAY_SIGNED:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                    case SOFAB_TYPE_FIXLENARRAY:
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
                        ctx->decoder->state = _DECODER_STATE_ARRAY_COUNT;
                        break;
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
                    case SOFAB_TYPE_SEQUENCE_START:
                        callback = 1;
                        break;

                    case SOFAB_TYPE_SEQUENCE_END:
                        break;
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

#if defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) || \
    defined(SOFAB_DISABLE_ARRAY_SUPPORT) || \
    defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                    default:
                        // unsupported field type
                        return SOFAB_RET_E_INVALID_MSG;
#endif
                }

                if (callback)
                {
                    sofab_ret_t ret;

                    if ((ret = _call_field_callback(ctx)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }
                }

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
                if (type == SOFAB_TYPE_SEQUENCE_START)
                {
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
                }
                else if (type == SOFAB_TYPE_SEQUENCE_END)
                {
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
                }
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

                break;
            }

            case _DECODER_STATE_VARINT_UNSIGNED:
            {
                sofab_unsigned_t unsigned_value = decoded;

                if (ctx->target_ptr)
                {
                    // store unsigned value in target buffer
                    if (_store_scalar(ctx->target_ptr, ctx->target_len, unsigned_value) != 0)
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
                sofab_unsigned_t zigzag_value = decoded;

                if (ctx->target_ptr)
                {
                    // zigzag decode
                    sofab_signed_t signed_value = _zigzag_decode(zigzag_value);

                    // store signed value in target buffer (low bytes are shared
                    // with the unsigned path once reinterpreted as unsigned)
                    if (_store_scalar(ctx->target_ptr, ctx->target_len,
                            (sofab_unsigned_t)signed_value) != 0)
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

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case _DECODER_STATE_FIXLEN_LEN:
            {
                sofab_ret_t ret;
                sofab_unsigned_t fixlen_length = decoded;

                // extract type from length
                uint8_t fixlen_type = _type_decode(&fixlen_length);
                switch (fixlen_type)
                {
                    case SOFAB_FIXLENTYPE_FP32:
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
                    case SOFAB_FIXLENTYPE_FP64:
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
                        ctx->decoder->state = _DECODER_STATE_FIXLEN_VAL;
                        break;

                    case SOFAB_FIXLENTYPE_STRING:
                    case SOFAB_FIXLENTYPE_BLOB:
                        ctx->decoder->state = _DECODER_STATE_FIXLEN_RAW;
                        break;

                    default:
                        // unsupported fixlen type
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

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
                // A fixlen array binds its destination only here (its element
                // subtype is unknown until the fixlen_word above is read).
                // Reconcile the wire count preserved in _DECODER_STATE_ARRAY_COUNT
                // with the destination capacity: this sets target_count to the
                // number of elements to read and lets the empty-array early-out
                // below see the true (0) wire count instead of the capacity.
                if (_OPT_FIELDTYPE(ctx->target_opt) == SOFAB_TYPE_FIXLENARRAY)
                {
                    if ((ret = _bind_array_count(ctx, ctx->array_wire_count)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }
                }
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

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

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
                // An empty fixlen ARRAY has just read its fixlen_word (the callback
                // above fired with the subtype) but carries no elements - finish.
                if (_OPT_FIELDTYPE(ctx->target_opt) == SOFAB_TYPE_FIXLENARRAY &&
                    ctx->target_count == 0)
                {
                    ctx->decoder->state = _DECODER_STATE_IDLE;
                    break;
                }
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

                if (length)
                {
                    // store source length to know how many bytes to consume
                    ctx->fixlen_remaining = length;
                }
                else
                {
                    // go back to idle
                    ctx->decoder->state = _DECODER_STATE_IDLE;
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
#endif /* defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ */

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
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            case _DECODER_STATE_ARRAY_COUNT:
            {
                sofab_ret_t ret;
                sofab_unsigned_t array_count = decoded;

                if (array_count > SOFAB_ARRAY_MAX)
                {
                    // invalid array count in message
                    return SOFAB_RET_E_INVALID_MSG;
                }

                // The wire carries the ACTUAL element count (0..N per
                // MESSAGE_SPEC §3); a bound destination declares its capacity N.
                // Seed target_count with the wire count, and preserve that count
                // in array_wire_count so it survives both the field callback
                // (which overwrites target_count with the destination capacity)
                // and, for fixlen arrays, the hop to _DECODER_STATE_FIXLEN_LEN
                // where the destination is actually bound.
                size_t count = (size_t)(array_count);
                ctx->target_count = count;
                ctx->array_wire_count = count;

                // read array elements (see IDLE state for type check)
                uint8_t type = _OPT_FIELDTYPE(ctx->target_opt);

                if (count == 0)
                {
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                    if (type == SOFAB_TYPE_FIXLENARRAY)
                    {
                        // A zero-count fixlen array still carries its fixlen_word,
                        // so the element subtype IS on the wire: read it (the field
                        // callback fires there with the full subtype). No payload
                        // follows for an empty array.
                        ctx->decoder->state = _DECODER_STATE_FIXLEN_LEN;
                        break;
                    }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

                    // A zero-count varint (integer) array carries no fixlen_word;
                    // its element width is API-only, so match the field type
                    // (0x07) without a subtype and finish.
                    if ((ret = _call_field_callback_masked(ctx, 0x07)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }

                    // An empty array is valid against any capacity >= 0.
                    if ((ret = _bind_array_count(ctx, count)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }

                    ctx->decoder->state = _DECODER_STATE_IDLE;
                    break;
                }

                if (type != SOFAB_TYPE_FIXLENARRAY)
                {
                    if ((ret = _call_field_callback(ctx)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }

                    // A varint array is fully bound now: accept a wire count up to
                    // the destination capacity, reading exactly that many. (Fixlen
                    // arrays bind later, in _DECODER_STATE_FIXLEN_LEN.)
                    if ((ret = _bind_array_count(ctx, count)) != SOFAB_RET_OK)
                    {
                        return ret;
                    }
                }

                if (type == SOFAB_TYPE_VARINTARRAY_UNSIGNED)
                    ctx->decoder->state = _DECODER_STATE_VARINT_UNSIGNED;
                else if (type == SOFAB_TYPE_VARINTARRAY_SIGNED)
                    ctx->decoder->state = _DECODER_STATE_VARINT_SIGNED;
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                else if (type == SOFAB_TYPE_FIXLENARRAY)
                    ctx->decoder->state = _DECODER_STATE_FIXLEN_LEN;
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

                break;
            }
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */
        }
    }

    // All input consumed without an error. Distinguish a complete message
    // (consumed bytes end exactly on a field boundary) from a partial one
    // (consumed bytes end mid-field or with an open sequence). INCOMPLETE is a
    // valid but partial decode, NOT a rejection: the caller owns end-of-input
    // and may resume by feeding more bytes. There is no finalize step.
    return _at_message_boundary(ctx) ? SOFAB_RET_OK : SOFAB_RET_INCOMPLETE;
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

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
extern void sofab_istream_read_array (
    sofab_istream_t *ctx, void *var,
    size_t element_count, size_t element_size, uint8_t opt)
{
    assert(ctx != NULL);
    assert(var != NULL);
    assert(element_size > 0);

    ctx->target_ptr = (uint8_t *)var;
    ctx->target_count = element_count;
    ctx->target_len = element_size;
    ctx->target_opt = opt;
}
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
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
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */
