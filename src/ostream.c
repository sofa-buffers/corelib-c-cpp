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
 * @brief Hand the buffered bytes to the flush callback.
 *
 * The cursor is reset **before** the callback runs, not after. CORELIB_PLAN
 * §5.1 states the start offset over the *installation* rather than the buffer:
 * a sofab_ostream_buffer_set() the callback makes begins a new installation
 * whose cursor starts at *that call's* offset, and that is how a sink reserves
 * framing-header room in every flushed unit. Resetting after the return would
 * silently discard the offset the callback just installed -- the buffer swap
 * would survive and the reservation would not, so every unit but the first
 * would have its header room overwritten.
 *
 * Resetting first also states the other half of the same rule: a sink that
 * copied and returns without installing anything leaves this reset standing
 * and the encoder resumes at offset 0.
 *
 * Caller checks @c ctx->flush; @p ctx->buffer and the byte count are read
 * before the call, so the callback is free to install anything it likes.
 *
 * @param ctx   Output stream context.
 */
static void _drain (sofab_ostream_t *ctx)
{
    uint8_t *data = ctx->buffer;
    size_t used = (size_t)(ctx->offset - data);

    ctx->offset = data;
    ctx->flush(ctx, data, used, ctx->usrptr);
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
            _drain(ctx);
        }
        else
        {
            // no flush callback, return buffer overflow
            return -1;
        }

        /* The callback may have installed a buffer that starts out full -- an
         * offset at its end, or no length to speak of. There is no room to
         * write into and nothing further to flush, so report it rather than
         * run past the end. Before the cursor honoured the installed offset
         * this could not happen: the post-flush reset forced the cursor to the
         * buffer start, which guaranteed room. */
        if (ctx->offset >= ctx->bufend)
        {
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

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) && !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
/*!
 * @brief Write out the held-back sequence headers, outermost first.
 *
 * Runs at most once per non-default sequence, never per field, so it is kept out
 * of line: the hot writers only pay the npending test.
 *
 * On SOFAB_RET_E_BUFFER_FULL the ids that did NOT reach the buffer stay pending:
 * they are shifted down so ctx->pending still holds exactly the still-unwritten
 * (innermost) run, and it still is a suffix of the open sequences — the ones
 * dropped from it are on the wire, framed. Without that, a partial commit would
 * forget the rest of the run, and the matching sofab_ostream_write_sequence_end()
 * calls would emit end markers for headers that were never written, leaving an
 * unbalanced (INVALID) stream even for a caller that installs a fresh buffer via
 * sofab_ostream_buffer_set() and retries. Retained is not the same as atomic: the
 * header that hit the wall may already have pushed the low bytes of its varint,
 * exactly as every other writer here can (see _push_byte) — the retained run is
 * what keeps the *bookkeeping* consistent, not a rollback of the buffer.
 *
 * @param ctx  Output stream context.
 * @return SOFAB_RET_OK on success, otherwise an sofab_ret_t error code.
 */
SOFAB_NOINLINE
static sofab_ret_t _commit_pending (sofab_ostream_t *ctx)
{
    sofab_ret_t ret = SOFAB_RET_OK;
    uint8_t n = ctx->npending;
    uint8_t i = 0;

    for (; i < n; i++)
    {
        if (_varint_encode(ctx, _type_encode(ctx->pending[i],
                                            SOFAB_TYPE_SEQUENCE_START)) < 0)
        {
            ret = SOFAB_RET_E_BUFFER_FULL;
            break;
        }
    }

    /* Keep the ids from i on -- everything before it is framed already. On the
     * success path i == n, so this is "npending = 0" plus an empty loop. */
    ctx->npending = (uint8_t)(n - i);
    for (uint8_t k = 0; k < ctx->npending; k++)
    {
        ctx->pending[k] = ctx->pending[i + k];
    }

    return ret;
}
#endif /* SEQUENCE && LAZY_SEQ */

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

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) && !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
    /* Every writer funnels through here, so this is where a held-back sequence
     * run is committed: the field about to be written is content, which means
     * every enclosing sequence is non-default and must be framed after all
     * (MESSAGE_SPEC §2, see sofab_ostream_write_sequence_begin_lazy). */
    if (ctx->npending != 0
        && type != SOFAB_TYPE_SEQUENCE_START && type != SOFAB_TYPE_SEQUENCE_END)
    {
        sofab_ret_t cret = _commit_pending(ctx);
        if (cret != SOFAB_RET_OK)
        {
            return cret;
        }
    }
#endif /* SEQUENCE && LAZY_SEQ */

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
static sofab_ret_t _write_fixlen (sofab_ostream_t *ctx, const void *data, size_t datalen)
{
    const uint8_t *bytes = (const uint8_t *)data;

    for (size_t i = 0; i < datalen; i++)
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
    /* CORELIB_PLAN §5.1 binds SOFAB_MIN_OUTPUT_BUFFER to a buffer installed
     * *with* a sink, "and on no other" (§13). A buffer without one can be
     * arbitrarily small -- no flush can occur, so nothing can be split, and it
     * either holds the message or reports SOFAB_RET_E_BUFFER_FULL. That case
     * has to stay exact: the all-default message is the empty byte string
     * (MESSAGE_SPEC §2), so a zero-length buffer is a legitimate installation
     * and offset == buflen is a legitimate (immediately full) cursor. */
    assert(offset <= buflen);
    assert(flush == NULL || buflen - offset >= SOFAB_MIN_OUTPUT_BUFFER);

    ctx->buffer = buffer;
    ctx->offset = buffer + offset;
    ctx->bufend = buffer + buflen;
    ctx->flush = flush;
    ctx->usrptr = usrptr;
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) && !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
    /* No sequence is open yet, so nothing is held back. The pending[] slots
     * themselves stay untouched -- npending bounds every read of them. */
    ctx->npending = 0;
#endif /* SEQUENCE && LAZY_SEQ */
}

extern size_t sofab_ostream_flush (sofab_ostream_t *ctx)
{
    size_t used;

    assert(ctx != NULL);

    used = (size_t)(ctx->offset - ctx->buffer);
    if (ctx->flush && used)
    {
        _drain(ctx);
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
    /* Same split as in sofab_ostream_init(): the minimum binds this
     * installation only if the stream drains through a sink (§5.1, §13). */
    assert(offset <= buflen);
    assert(ctx->flush == NULL || buflen - offset >= SOFAB_MIN_OUTPUT_BUFFER);

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
        if ((ret = _write_fixlen(ctx, data, (size_t)datalen)) != SOFAB_RET_OK)
        {
            return ret;
        }
    }
#else
    if ((ret = _write_fixlen(ctx, data, (size_t)datalen)) != SOFAB_RET_OK)
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
        // Both signednesses read the same bytes and differ only in how they are
        // re-signed afterwards, so one width dispatch serves them and the
        // element loop carries a single 1/2/4/8 load chain.
        sofab_unsigned_t enc;

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

        if (is_signed)
        {
            // Re-sign the loaded low bytes, then ZigZag them. A cast per width,
            // not a shift by a computed amount: the widths are a fixed set, so
            // each arm is a single sign-extend instruction, while a variable
            // shift of a 64-bit value costs a multi-instruction sequence on a
            // 32-bit target (measured: 36 bytes on Cortex-M3/M7/M55).
            sofab_signed_t value;
            switch (element_size)
            {
                case 1:  value = (int8_t)enc;  break;
                case 2:  value = (int16_t)enc; break;
                case 4:  value = (int32_t)enc; break;
                default: value = (sofab_signed_t)enc; break; /* full width */
            }
            enc = _zigzag_encode(value);
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

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    const uint8_t *ptr = (const uint8_t*)data;
    for (int32_t i = 0; i < element_count; i++)
    {
        // Big-endian host: each float element is byte-reversed on its way out,
        // so the payload has to be walked one element at a time.
        if (type == SOFAB_FIXLENTYPE_FP32 || type == SOFAB_FIXLENTYPE_FP64)
        {
            if ((ret = _write_fixlen_reverse(ctx, ptr, element_size)) != SOFAB_RET_OK)
            {
                return ret;
            }
        }
        else
        {
            if ((ret = _write_fixlen(ctx, ptr, (size_t)element_size)) != SOFAB_RET_OK)
            {
                return ret;
            }
        }
        ptr += element_size;
    }
#else
    // Little-endian host: the elements are already in wire order and lie
    // contiguously, so the whole payload is one flat byte range — the
    // per-element loop would emit exactly these bytes, one call at a time.
    if ((ret = _write_fixlen(ctx, data,
             (size_t)element_count * (size_t)element_size)) != SOFAB_RET_OK)
    {
        return ret;
    }
#endif /* defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ */

    return SOFAB_RET_OK;
}
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
extern sofab_ret_t sofab_ostream_write_sequence_begin (sofab_ostream_t *ctx, sofab_id_t id)
{
    sofab_ret_t ret;

    assert(ctx != NULL);

    /* A framed sequence is content for everything enclosing it. Committing here
     * also keeps ctx->pending a suffix of the open sequences, which
     * sofab_ostream_write_sequence_end() relies on. */
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) && !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
    if (ctx->npending != 0 && (ret = _commit_pending(ctx)) != SOFAB_RET_OK)
    {
        return ret;
    }
#endif /* SEQUENCE && LAZY_SEQ */

    if ((ret = _write_id_type(ctx, id, SOFAB_TYPE_SEQUENCE_START)) != SOFAB_RET_OK)
    {
        return ret;
    }

    return SOFAB_RET_OK;
}

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) && !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
extern sofab_ret_t sofab_ostream_write_sequence_begin_lazy (sofab_ostream_t *ctx, sofab_id_t id)
{
    sofab_ret_t ret;

    assert(ctx != NULL);

    if (id > SOFAB_ID_MAX)
    {
        return SOFAB_RET_E_ARGUMENT;
    }

    if (ctx->npending < SOFAB_LAZY_SEQ_DEPTH)
    {
        ctx->pending[ctx->npending++] = id;
        return SOFAB_RET_OK;
    }

    /* Deeper than the hold-back window: commit the run and frame eagerly, which
     * keeps the suffix invariant above. Valid, just not canonical if this
     * sequence turns out to be all-default. */
    if ((ret = _commit_pending(ctx)) != SOFAB_RET_OK)
    {
        return ret;
    }

    return _write_id_type(ctx, id, SOFAB_TYPE_SEQUENCE_START);
}

extern sofab_ret_t sofab_ostream_write_sequence_end_keep (sofab_ostream_t *ctx)
{
    sofab_ret_t ret;

    assert(ctx != NULL);

    /* Like a write: emit the held-back headers first, then the end marker, so a
     * sequence that never got content still reaches the wire as begin + end
     * (MESSAGE_SPEC §5.1 -- element presence carries the array length). */
    if (ctx->npending != 0 && (ret = _commit_pending(ctx)) != SOFAB_RET_OK)
    {
        return ret;
    }

    return _write_id_type(ctx, 0, SOFAB_TYPE_SEQUENCE_END);
}
#endif /* SEQUENCE && LAZY_SEQ */

extern sofab_ret_t sofab_ostream_write_sequence_end (sofab_ostream_t *ctx)
{
    sofab_ret_t ret;

    assert(ctx != NULL);

    /* The innermost open sequence is the last held-back one, if any: it had no
     * content, so the whole field is omitted -- header and end marker both
     * (MESSAGE_SPEC §2). */
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) && !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
    if (ctx->npending != 0)
    {
        ctx->npending--;
        return SOFAB_RET_OK;
    }
#endif /* SEQUENCE && LAZY_SEQ */

    if ((ret = _write_id_type(ctx, 0, SOFAB_TYPE_SEQUENCE_END)) != SOFAB_RET_OK)
    {
        return ret;
    }

    return SOFAB_RET_OK;
}
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */
