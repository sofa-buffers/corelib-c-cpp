/*!
 * @file object.c
 * @brief SofaBuffers C - Object encoder and decoder.
 *
 * SPDX-License-Identifier: MIT
 */

#define SOFAB_OBJECT_C

/* includes *******************************************************************/
#include "sofab/object.h"

#include <assert.h>

/* constants ******************************************************************/

/* macros *********************************************************************/
/*! @brief Cast @p ptr advanced by @p offset bytes to @p type (field accessor). */
#define CAST_TO(type, ptr, offset) ((type)((const uint8_t *)(ptr) + (offset)))

/* types **********************************************************************/

/* prototypes *****************************************************************/

/* static vars ****************************************************************/
/*!
 * @brief In the minimal profile the §7.3 wire-type map degenerates to identity.
 *
 * When fixlen, array and sequence support are all compiled out, the only field
 * types that can reach @ref sofab_object_field_cb are @c UNSIGNED and @c SIGNED
 * (the istream rejects every other wire type as @c INVALID before the callback),
 * and for those the expected wire opt equals @c field->type (0x0 / 0x1). The
 * lookup table below and its bounds check are then dead weight, so the §7.3
 * check collapses to a direct @c field->type comparison and the table vanishes.
 */
#if defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && \
    defined(SOFAB_DISABLE_ARRAY_SUPPORT)  && \
    defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
#  define _WIRETYPE_MAP_IS_IDENTITY 1
#endif

#if !defined(_WIRETYPE_MAP_IS_IDENTITY)
/*!
 * @brief Expected wire opt (field type + fixlen subtype, low 6 bits) for each
 *        descriptor field type, indexed by @c SOFAB_OBJECT_FIELDTYPE_*.
 *
 * MESSAGE_SPEC §7.3: on decode a field whose header wire type does not match the
 * one its declared type maps to — for @c fixlen including the subtype — is
 * skipped exactly like an unknown id, and is @e not reported as a usage error or
 * as @c INVALID. @ref sofab_object_field_cb compares the actual wire opt (held in
 * @c ctx->target_opt on entry, subtype already merged for fixlen) against this
 * table before binding a target; a mismatch leaves the target unbound so the
 * istream skips the field. The @c STRINGTERM bit (0x40) is a read-side option,
 * absent from the wire, so the comparison masks it out (§7.3 depth is 0x3F).
 */
static const uint8_t _expected_wire_opt[] = {
    [SOFAB_OBJECT_FIELDTYPE_UNSIGNED]       = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED),
    [SOFAB_OBJECT_FIELDTYPE_SIGNED]         = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED),
    [SOFAB_OBJECT_FIELDTYPE_FP32]           = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32),
    [SOFAB_OBJECT_FIELDTYPE_FP64]           = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64),
    [SOFAB_OBJECT_FIELDTYPE_STRING]         = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_STRING),
    [SOFAB_OBJECT_FIELDTYPE_BLOB]           = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_BLOB),
    [SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED] = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED),
    [SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED]   = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED),
    [SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32]     = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
                                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32),
    [SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64]     = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
                                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64),
    [SOFAB_OBJECT_FIELDTYPE_SEQUENCE]       = SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_SEQUENCE_START),
};
#endif /* !defined(_WIRETYPE_MAP_IS_IDENTITY) */

/* functions ******************************************************************/
/*!
 * @brief Test whether a memory region is all zero bytes.
 *
 * @param ptr  Pointer to the region.
 * @param len  Number of bytes to examine.
 * @return 1 if every byte is zero, 0 otherwise.
 */
static int _iszero (const void *ptr, size_t len)
{
    const uint8_t *p = (const uint8_t *)ptr;

    for (size_t i = 0; i < len; i++)
    {
        if (p[i] != 0) return 0;
    }

    return 1;
}

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
/*!
 * @brief Canonical element count for a fixed-length array (MESSAGE_SPEC §3).
 *
 * A @c count:N array carries exactly @p n logical elements, but its trailing run
 * of element defaults MUST NOT be emitted: the encoder writes @c M = one past the
 * index of the last element that differs from the element default (zero), and a
 * decoder refills @c [M, N) from the element default. The default is compared on
 * the raw @b byte image (via @ref _iszero), so a trailing @c -0.0 (sign bit set)
 * or any NaN is correctly @e not a default and stays on the wire.
 *
 * This trim lives only here on the C-only descriptor path — never in the
 * @c sofab_ostream_write_array_of_* writers, whose C++ callers pass dynamic
 * arrays with no @c N to refill from, so their trailing defaults are significant.
 *
 * @param base          Pointer to element 0.
 * @param n             Structural element count N (@c size / element_size).
 * @param element_size  Byte width of one element.
 * @return M, the canonical (trimmed) element count in @c [0, n].
 */
static int32_t _array_trim_count (const void *base, size_t n, size_t element_size)
{
    const uint8_t *p = (const uint8_t *)base;

    while (n > 0 && _iszero(p + (n - 1) * element_size, element_size))
    {
        n--;
    }

    return (int32_t)n;
}
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
/*!
 * @brief Load a host-endian unsigned integer of @p width (1/2/4/8) bytes.
 *
 * Reads a sized blob's companion used-length member, whose C type (and thus
 * width) the caller chooses; @p width comes from the descriptor's @c nested_idx.
 * An unsupported width yields 0 (treated as an empty blob).
 */
static uint64_t _load_uint (const void *p, uint8_t width)
{
    switch (width)
    {
        case 1: return *(const uint8_t *)p;
        case 2: return *(const uint16_t *)p;
        case 4: return *(const uint32_t *)p;
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
        case 8: return *(const uint64_t *)p;
#endif
        default: return 0;
    }
}

/*! @brief Store @p val as a host-endian unsigned integer of @p width bytes. */
static void _store_uint (void *p, uint8_t width, uint64_t val)
{
    switch (width)
    {
        case 1: *(uint8_t *)p  = (uint8_t)val;  break;
        case 2: *(uint16_t *)p = (uint16_t)val; break;
        case 4: *(uint32_t *)p = (uint32_t)val; break;
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
        case 8: *(uint64_t *)p = val;           break;
#endif
        default: break;
    }
}
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

/*!
 * @brief Test whether a field currently holds its default value (so it is
 *        omitted from the sparse encoding).
 *
 * Fixed-width fields (integers, floats, blobs, native arrays) compare their raw
 * storage: against the descriptor's default image when it carries one, else
 * against all-zero. A STRING is instead compared by its logical, null-terminated
 * content bounded by the field size: the buffer bytes past the terminator are
 * indeterminate (e.g. a shorter string overwriting a longer one) and must not
 * affect the decision, so it matches exactly what @ref sofab_ostream_write_string
 * serialises.
 *
 * A SEQUENCE field recurses: it is default iff its whole sub-object is default
 * (every child field default), i.e. iff encoding it would emit an empty frame.
 * This is the encode-faithful notion a raw @ref _iszero byte scan cannot express —
 * an all-default nested struct is @e not all-zero when it carries non-zero
 * defaults, and its padding must be ignored. It powers MESSAGE_SPEC §5.1 trailing
 * elision of sequence-form wrapper elements (see @ref sofab_object_encode); the
 * per-field skip there must still @e not call this on a @e standalone SEQUENCE
 * field, which §2 keeps framed regardless.
 *
 * @param info  Descriptor owning @p field (source of the default image and, for a
 *              SEQUENCE, the nested descriptor).
 * @param field Field descriptor.
 * @param src   Object being encoded.
 * @return 1 when the field equals its default, 0 otherwise.
 */
static int _field_is_default (
    const sofab_object_descr_t *info,
    const sofab_object_descr_field_t *field,
    const void *src)
{
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    if (field->type == SOFAB_OBJECT_FIELDTYPE_SEQUENCE)
    {
        const sofab_object_descr_t *ninfo = info->nested_list[field->nested_idx];
        const void *nsrc = CAST_TO(const void *, src, field->offset);
        for (size_t i = 0; i < ninfo->field_count; i++)
        {
            if (!_field_is_default(ninfo, &ninfo->field_list[i], nsrc))
                return 0;
        }
        return 1;
    }
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

    const void *defaults = info->default_values;
    const void *val = CAST_TO(const void *, src, field->offset);

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
    if (field->type == SOFAB_OBJECT_FIELDTYPE_STRING)
    {
        const char *s = (const char *)val;
        if (defaults != NULL)
        {
            return strncmp(s, CAST_TO(const char *, defaults, field->offset),
                           field->size) == 0;
        }
        /* No default image: the implicit default is the empty string. */
        return field->size == 0 || s[0] == '\0';
    }

    if (field->type == SOFAB_OBJECT_FIELDTYPE_BLOB && field->nested_idx != 0)
    {
        /* Sized blob: the logical default is an empty blob (used_len == 0),
         * mirroring the empty-string rule above. The buffer bytes are
         * indeterminate and must not influence the decision. used_len sits
         * immediately before the buffer (nested_idx bytes wide). */
        return _load_uint(CAST_TO(const void *, src, field->offset - field->nested_idx),
                          field->nested_idx) == 0;
    }
#endif

    if (defaults != NULL)
    {
        return memcmp(CAST_TO(const void *, defaults, field->offset),
                      val, field->size) == 0;
    }
    return _iszero(val, field->size);
}

extern sofab_ret_t sofab_object_init (
    const sofab_object_descr_t *info,
    void *obj)
{
    assert(info != NULL);
    assert(obj != NULL);

    for (size_t i = 0; i < info->field_count; i++)
    {
        const sofab_object_descr_field_t *field = &info->field_list[i];

        if (field->type == SOFAB_OBJECT_FIELDTYPE_SEQUENCE)
        {
            const sofab_object_descr_t *nested_info = info->nested_list[field->nested_idx];
            void *nested_obj = CAST_TO(void *, obj, field->offset);

            sofab_object_init(nested_info, nested_obj);
        }
        else
        {
            if (info->default_values != NULL)
            {
                memcpy(
                    CAST_TO(void *, obj, field->offset),
                    CAST_TO(const void *, info->default_values, field->offset),
                    field->size);
            }
            else
            {
                memset(CAST_TO(void *, obj, field->offset), 0, field->size);
            }

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            /* Sized blob: the used-length member sits nested_idx bytes before the
             * buffer and is not covered by (offset, size); reset it too, mirroring
             * _field_is_default / encode / decode which all address it at
             * offset - nested_idx. Without this a §7.4 wrapper re-open leaves a
             * stale length, so a dropped element survives as an all-zero blob. */
            if (field->type == SOFAB_OBJECT_FIELDTYPE_BLOB && field->nested_idx != 0)
            {
                uint64_t dlen = info->default_values != NULL
                    ? _load_uint(CAST_TO(const void *, info->default_values,
                                         field->offset - field->nested_idx),
                                 field->nested_idx)
                    : 0;
                _store_uint(CAST_TO(void *, obj, field->offset - field->nested_idx),
                            field->nested_idx, dlen);
            }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
        }
    }

    return SOFAB_RET_OK;
}

extern sofab_ret_t sofab_object_encode (
    sofab_ostream_t *ctx,
    const sofab_object_descr_t *info,
    const void *src)
{
    sofab_ret_t ret = SOFAB_RET_OK;
    assert(ctx != NULL);
    assert(info != NULL);
    assert(src != NULL);

    /*
     * MESSAGE_SPEC §5.1: a fixed-count wrapper holder elides its trailing run of
     * all-default elements — sequence-form elements included. The element slots
     * are ids 0..field_count-1; drop the maximal trailing run whose elements each
     * equal their default so an all-default array-of-struct re-encodes as an empty
     * wrapper (M = 0), matching the §3 trim already applied to scalar arrays by
     * _array_trim_count. _field_is_default handles both element kinds uniformly
     * (a SEQUENCE element recurses into its sub-object); leaf-element holders would
     * also have their trailing run elided by the per-field skip below, so here the
     * trim is only load-bearing for the sequence-form elements that skip never
     * reaches. Confined to a fixed_seq holder, so a standalone struct field stays
     * framed per §2.
     */
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    size_t n_emit = info->field_count;
    if (info->fixed_seq)
    {
        while (n_emit > 0)
        {
            if (!_field_is_default(info, &info->field_list[n_emit - 1], src))
                break;
            n_emit--;
        }
    }
#  define _SOFAB_ENCODE_LIMIT n_emit
#else
    /* Without sequence support no fixed-count wrapper holder can be reached, so
     * there is nothing to trim: the loop reads info->field_count directly and
     * this path stays byte-identical to the pre-elision encoder (minimal profile). */
#  define _SOFAB_ENCODE_LIMIT info->field_count
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

    for (size_t i = 0; i < _SOFAB_ENCODE_LIMIT && ret == SOFAB_RET_OK; i++)
    {
        const sofab_object_descr_field_t *field = &info->field_list[i];

        /*
         * A SEQUENCE (nested object) is always framed and recursed into; whether
         * its children appear is decided per inner field below. It is never
         * omitted by a whole-object memcmp/_iszero over its raw storage, which
         * would also compare struct padding and mishandle non-zero nested
         * defaults (a logically-default child is not all-zero). Only leaf fields
         * are skipped when they equal their default. When SEQUENCE support is
         * compiled out this guard vanishes, leaving the original code unchanged.
         */
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
        if (field->type != SOFAB_OBJECT_FIELDTYPE_SEQUENCE)
#endif
        {
            if (_field_is_default(info, field, src))
            {
                // Field value matches its default, skip serialization
                continue;
            }
        }

        switch (field->type)
        {
            case SOFAB_OBJECT_FIELDTYPE_UNSIGNED:
            {
                sofab_unsigned_t val;
                if (field->element_size == sizeof(uint8_t))
                    val = *CAST_TO(uint8_t *, src, field->offset);
                else if (field->element_size == sizeof(uint16_t))
                    val = *CAST_TO(uint16_t *, src, field->offset);
                else if (field->element_size == sizeof(uint32_t))
                    val = *CAST_TO(uint32_t *, src, field->offset);
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
                else if (field->element_size == sizeof(uint64_t))
                    val = *CAST_TO(uint64_t *, src, field->offset);
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */
                else
                    return SOFAB_RET_E_USAGE; // Unsupported size (8 requires 64-bit values)

                ret = sofab_ostream_write_unsigned(ctx, field->id, val);
                break;
            }

            case SOFAB_OBJECT_FIELDTYPE_SIGNED:
            {
                sofab_signed_t sval;
                if (field->element_size == sizeof(int8_t))
                    sval = *CAST_TO(int8_t *, src, field->offset);
                else if (field->element_size == sizeof(int16_t))
                    sval = *CAST_TO(int16_t *, src, field->offset);
                else if (field->element_size == sizeof(int32_t))
                    sval = *CAST_TO(int32_t *, src, field->offset);
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
                else if (field->element_size == sizeof(int64_t))
                    sval = *CAST_TO(int64_t *, src, field->offset);
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */
                else
                    return SOFAB_RET_E_USAGE; // Unsupported size (8 requires 64-bit values)

                ret = sofab_ostream_write_signed(ctx, field->id, sval);
                break;
            }

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP32:
                ret = sofab_ostream_write_fp32(ctx, field->id, *CAST_TO(float *, src, field->offset));
                break;

#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP64:
                ret = sofab_ostream_write_fp64(ctx, field->id, *CAST_TO(double *, src, field->offset));
                break;
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */

            case SOFAB_OBJECT_FIELDTYPE_STRING:
                ret = sofab_ostream_write_string(ctx, field->id, CAST_TO(char *, src, field->offset));
                break;

            case SOFAB_OBJECT_FIELDTYPE_BLOB:
            {
                size_t blob_len = field->size;
                if (field->nested_idx != 0)
                {
                    /* Sized blob: emit only used_len bytes (clamped to capacity).
                     * used_len sits immediately before the buffer. */
                    uint64_t used = _load_uint(
                        CAST_TO(const uint8_t *, src, field->offset - field->nested_idx),
                        field->nested_idx);
                    blob_len = used < field->size ? (size_t)used : field->size;
                }
                ret = sofab_ostream_write_blob(ctx, field->id,
                    CAST_TO(uint8_t *, src, field->offset), blob_len);
                break;
            }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
            {
                // Both writers share a signature and differ only in the element
                // interpretation; select via pointer so the element-count math
                // and the call are emitted once.
                sofab_ret_t (*const write_array)(
                    sofab_ostream_t *, sofab_id_t, const void *, int32_t, int32_t) =
                    (field->type == SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED)
                        ? sofab_ostream_write_array_of_signed
                        : sofab_ostream_write_array_of_unsigned;
                ret = write_array(ctx, field->id,
                    CAST_TO(const void *, src, field->offset),
                    _array_trim_count(CAST_TO(const void *, src, field->offset),
                        field->size / field->element_size, field->element_size),
                    field->element_size);
                break;
            }

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
            {
                // FP32/FP64 arrays share the fixlen-array writer; only the
                // element width and subtype tag differ.
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
                int is_fp64 = (field->type == SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64);
#else
                const int is_fp64 = 0;
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
                size_t element_size = is_fp64 ? sizeof(double) : sizeof(float);
                ret = sofab_ostream_write_array_of_fixlen(ctx, field->id,
                    CAST_TO(const void *, src, field->offset),
                    _array_trim_count(CAST_TO(const void *, src, field->offset),
                        field->size / element_size, element_size),
                    element_size,
                    is_fp64 ? SOFAB_FIXLENTYPE_FP64 : SOFAB_FIXLENTYPE_FP32);
                break;
            }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_SEQUENCE:
                ret = sofab_ostream_write_sequence_begin(ctx, field->id);
                ret |= sofab_object_encode(ctx,
                    info->nested_list[field->nested_idx],
                    CAST_TO(const uint8_t *, src, field->offset));
                ret |= sofab_ostream_write_sequence_end(ctx);
                break;
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

            default:
                // Unsupported field type in descriptor
                return SOFAB_RET_E_USAGE;
        }
    }
#undef _SOFAB_ENCODE_LIMIT

    return ret;
}

extern void sofab_object_field_cb (sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    sofab_object_decoder_t *decoder = (sofab_object_decoder_t *)usrptr;
    const sofab_object_descr_t *info = decoder->info;

#if defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
    (void)size;   /* consumed only by the sized-blob branch (fixlen) below */
#endif
    (void)count;

    for (size_t i = 0; i < info->field_count; i++)
    {
        const sofab_object_descr_field_t *field = &info->field_list[i];
        if (field->id != id)
        {
            continue;
        }

        /* MESSAGE_SPEC §7.3: the id matches, but if the header wire type
         * contradicts the declared type (wire type + fixlen subtype, mask
         * 0x3F) the field is skipped like an unknown id — not rejected. Bind no
         * target and return, leaving target_ptr NULL so the istream consumes
         * (or, for a sequence, skips) the field. A descriptor type outside the
         * table (0x0..0xA) falls through to the switch's default (also a skip). */
#if defined(_WIRETYPE_MAP_IS_IDENTITY)
        if ((ctx->target_opt ^ field->type) & 0x07)
        {
            return;
        }
#else
        if (field->type < (sizeof _expected_wire_opt / sizeof _expected_wire_opt[0])
            && ((ctx->target_opt ^ _expected_wire_opt[field->type]) & 0x3F))
        {
            return;
        }
#endif

        switch (field->type)
        {
            case SOFAB_OBJECT_FIELDTYPE_UNSIGNED:
            case SOFAB_OBJECT_FIELDTYPE_SIGNED:
                // unsigned and signed differ only in the wire type tag
                sofab_istream_read_field(ctx, decoder->dst + field->offset, field->element_size,
                    SOFAB_ISTREAM_OPT_FIELDTYPE(
                        field->type == SOFAB_OBJECT_FIELDTYPE_SIGNED
                            ? SOFAB_TYPE_VARINT_SIGNED : SOFAB_TYPE_VARINT_UNSIGNED));
                break;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP32:
                sofab_istream_read_fp32(ctx, (float *)(decoder->dst + field->offset));
                break;

#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP64:
                sofab_istream_read_fp64(ctx, (double *)(decoder->dst + field->offset));
                break;
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */

            case SOFAB_OBJECT_FIELDTYPE_STRING:
                sofab_istream_read_string(ctx, (char *)(decoder->dst + field->offset), field->size);
                break;

            case SOFAB_OBJECT_FIELDTYPE_BLOB:
                sofab_istream_read_blob(ctx, decoder->dst + field->offset, field->size);
                if (field->nested_idx != 0)
                {
                    /* Sized blob: record the actual received length in used_len,
                     * which sits immediately before the buffer. */
                    size_t used = size < field->size ? size : field->size;
                    _store_uint(decoder->dst + field->offset - field->nested_idx,
                                field->nested_idx, (uint64_t)used);
                }
                break;
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
                // unsigned and signed arrays differ only in the wire type tag
                sofab_istream_read_array(ctx,
                    decoder->dst + field->offset,
                    field->size / field->element_size, field->element_size,
                    SOFAB_ISTREAM_OPT_FIELDTYPE(
                        field->type == SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED
                            ? SOFAB_TYPE_VARINTARRAY_SIGNED : SOFAB_TYPE_VARINTARRAY_UNSIGNED));
                break;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
                sofab_istream_read_array_of_fp32(ctx,
                    (float *)(decoder->dst + field->offset),
                    field->size / sizeof(float));
                break;

#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
                sofab_istream_read_array_of_fp64(ctx,
                    (double *)(decoder->dst + field->offset),
                    field->size / sizeof(double));
                break;
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_SEQUENCE:
            {
                if (decoder->depth == 0) break; // Sequence depth exceeded

                // pointer arithmetic to get next decoder handle in array
                // (bounds are checked via depth above)
                sofab_object_decoder_t *nested = decoder + 1;

                // use descriptor from nested list
                nested->info = info->nested_list[field->nested_idx];
                // destination pointer for nested object
                nested->dst = decoder->dst + field->offset;
                // decrement available amount of decoder handles
                nested->depth = decoder->depth - 1;

                // MESSAGE_SPEC §7.4: a re-opened array wrapper *replaces* the
                // array value whole, whereas a struct/union *merges* (last
                // occurrence wins per field id). A wrapper holder is flagged
                // fixed_seq (SOFAB_OBJECT_DESCR_SEQ) — the same marker used to
                // reject over-index elements. Reset its slots to their defaults
                // on open so a later occurrence overwrites rather than merges;
                // structs and unions (fixed_seq == 0) keep merging untouched.
                if (nested->info->fixed_seq)
                {
                    sofab_object_init(nested->info, nested->dst);
                }

                sofab_istream_read_sequence(ctx,
                    &nested->decoder,
                    sofab_object_field_cb,
                    nested);
                break;
            }
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

            default:
                // Unsupported field type in descriptor
                break;
        }

        // field handled — done (return, so the over-index reject below only
        // runs when no descriptor field matched this id)
        return;
    }

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    // No descriptor field matched this id. A message treats an unknown id as a
    // forward-compatible field and skips it. But a fixed-count sequence holder's
    // fields ARE the element slots 0..field_count-1, so an unmatched id is an
    // over-index element (id >= N): reject the message per MESSAGE_SPEC §7/§7.1
    // instead of silently dropping it. This is the object-API counterpart of the
    // streaming abort channel from #92/#93 (issue #94); the holder loop above
    // never bound a target, so the invalidate is set synchronously here.
    if (info->fixed_seq)
    {
        sofab_istream_invalidate(ctx);
    }
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */
}
