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

/*!
 * @brief Widest scalar width (in bytes) this build can load, and that set of
 *        widths (1/2/4/8) as one bit per width.
 *
 * 8 only with 64-bit values; @ref SOFAB_DISABLE_INT64_SUPPORT narrows both the
 * cap and the set, so the width check in @ref sofab_object_encode and the
 * @ref _load_uint dispatch stay in agreement by construction.
 */
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
# define _SOFAB_WIDTH_MAX 8
#else
# define _SOFAB_WIDTH_MAX 4
#endif
#define _SOFAB_WIDTH_SET ((1u << 1) | (1u << 2) | (1u << 4) | (1u << _SOFAB_WIDTH_MAX))

/* The set test above shifts by a 4-bit descriptor field, so the mask must stay
 * wide enough that no in-range width shifts a set bit off the end. */
typedef char _sofab_check_width_set[(_SOFAB_WIDTH_MAX < 16) ? 1 : -1];

/*!
 * @brief Load a host-endian unsigned integer of @p width (1/2/4/8) bytes.
 *
 * Two callers share this one dispatch, so the binary carries a single 1/2/4/8
 * load: an integer field's raw value in @ref sofab_object_encode (whose signed
 * path sign-extends the result afterwards), and a sized blob's companion
 * used-length member, whose C type (and thus width) the caller chooses. @p width
 * comes from the descriptor's @c element_size or @c nested_idx respectively.
 * An unsupported width yields 0 (an empty blob; encode screens the width first).
 */
static sofab_unsigned_t _load_uint (const void *p, uint8_t width)
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

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
/*! @brief Store @p val as a host-endian unsigned integer of @p width bytes. */
static void _store_uint (void *p, uint8_t width, sofab_unsigned_t val)
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

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
        /* A nested object is seeded field by field, not as a byte image: its own
         * defaults live in its own descriptor. Without sequence support no
         * descriptor can carry a reachable SEQUENCE field — encode rejects one
         * and the field callback declines it — so the recursion compiles out
         * with the feature, as it already does in _field_is_default and encode. */
        if (field->type == SOFAB_OBJECT_FIELDTYPE_SEQUENCE)
        {
            const sofab_object_descr_t *nested_info = info->nested_list[field->nested_idx];
            void *nested_obj = CAST_TO(void *, obj, field->offset);

            sofab_object_init(nested_info, nested_obj);
        }
        else
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */
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
                sofab_unsigned_t dlen = info->default_values != NULL
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

/*!
 * @brief The istream read options each descriptor field type binds, indexed by
 *        the type tag itself.
 *
 * Every non-sequence field type binds the same three things — a destination
 * pointer, a width, and this option word — so the ten per-type arms this
 * replaces differed only in the constant below. Keeping it as data rather than
 * code is what collapses them into a single bind.
 *
 * The table is only as long as the enabled types reach, so a profile with
 * fixlen and arrays compiled out carries two bytes of it. A type that is a hole
 * in the enabled set (an fp64 entry in an fp32-only build) keeps its slot, to
 * hold the indices aligned with the type tags, and is marked
 * @ref _READ_OPT_NONE so the bind is declined exactly as an unknown type is.
 */
#define _READ_OPT_NONE 0xFFu

/*! @brief Whether the enabled set actually leaves a hole in the table.
 *
 * Only two configurations can: fp64 off (an fp32 build still needs the fp32
 * entries that follow), and fixlen off while arrays stay on (the array entries
 * sit past the fixlen ones). Anywhere else the table is dense to its end and the
 * @ref _READ_OPT_NONE test is dead code, so it is not compiled at all.
 */
#if defined(SOFAB_DISABLE_FP64_SUPPORT) || \
    (defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && !defined(SOFAB_DISABLE_ARRAY_SUPPORT))
# define _READ_OPT_HAS_HOLES 1
#else
# define _READ_OPT_HAS_HOLES 0
#endif

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
# define _READ_OPT_FP32 \
    (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) | \
     SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32))
# define _READ_OPT_STRING \
    (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) | \
     SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_STRING) | \
     SOFAB_ISTREAM_OPT_STRINGTERM)
# define _READ_OPT_BLOB \
    (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) | \
     SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_BLOB))
# define _READ_OPT_ARRAY_FP32 \
    (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY) | \
     SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32))
# if !defined(SOFAB_DISABLE_FP64_SUPPORT)
#  define _READ_OPT_FP64 \
    (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN) | \
     SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64))
#  define _READ_OPT_ARRAY_FP64 \
    (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY) | \
     SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64))
# else
#  define _READ_OPT_FP64       _READ_OPT_NONE
#  define _READ_OPT_ARRAY_FP64 _READ_OPT_NONE
# endif
#else
# define _READ_OPT_FP32        _READ_OPT_NONE
# define _READ_OPT_FP64        _READ_OPT_NONE
# define _READ_OPT_STRING      _READ_OPT_NONE
# define _READ_OPT_BLOB        _READ_OPT_NONE
# define _READ_OPT_ARRAY_FP32  _READ_OPT_NONE
# define _READ_OPT_ARRAY_FP64  _READ_OPT_NONE
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

/*! @brief The fixlen subtype a field type binds, as carried in @ref _read_opt. */
#define _SOFAB_READ_OPT_SUBTYPE(type) ((_read_opt[(type)] >> 3) & 0x07u)

static const uint8_t _read_opt[] = {
    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED),  /* UNSIGNED */
    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED),    /* SIGNED   */
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) || !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    _READ_OPT_FP32,                                           /* FP32     */
    _READ_OPT_FP64,                                           /* FP64     */
    _READ_OPT_STRING,                                         /* STRING   */
    _READ_OPT_BLOB,                                           /* BLOB     */
#endif
#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED), /* ARRAY_UNSIGNED */
    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED),   /* ARRAY_SIGNED   */
    _READ_OPT_ARRAY_FP32,                                     /* ARRAY_FP32 */
    _READ_OPT_ARRAY_FP64,                                     /* ARRAY_FP64 */
#endif
};

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
            case SOFAB_OBJECT_FIELDTYPE_SIGNED:
            {
                // Both types read the same bytes and differ only in how they are
                // re-signed, so they share one width dispatch (_load_uint) instead
                // of carrying a 1/2/4/8 load chain each.
                // element_size is a 4-bit descriptor field, so it never shifts
                // the mask past its width and the set test needs no range guard
                // of its own.
                const uint8_t width = field->element_size;
                if (((_SOFAB_WIDTH_SET >> width) & 1u) == 0)
                {
                    // Unsupported size (8 requires 64-bit values)
                    return SOFAB_RET_E_ARGUMENT;
                }

                sofab_unsigned_t val =
                    _load_uint(CAST_TO(const void *, src, field->offset), width);

                if (field->type == SOFAB_OBJECT_FIELDTYPE_SIGNED)
                {
                    // Re-sign the loaded low bytes. A cast per width, not a
                    // shift by a computed amount: the widths are a fixed set, so
                    // each arm is a single sign-extend instruction, while a
                    // variable shift on a 64-bit value is a multi-instruction
                    // sequence on every 32-bit target.
                    sofab_signed_t sval;
                    switch (width)
                    {
                        case 1:  sval = (int8_t)val;  break;
                        case 2:  sval = (int16_t)val; break;
                        case 4:  sval = (int32_t)val; break;
                        default: sval = (sofab_signed_t)val; break; /* full width */
                    }
                    ret = sofab_ostream_write_signed(ctx, field->id, sval);
                }
                else
                {
                    ret = sofab_ostream_write_unsigned(ctx, field->id, val);
                }
                break;
            }

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP32:
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP64:
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
            case SOFAB_OBJECT_FIELDTYPE_STRING:
            case SOFAB_OBJECT_FIELDTYPE_BLOB:
            {
                /* Every fixlen scalar is the same wire shape — a subtype and a
                 * byte range — so the four types share one write and differ only
                 * in how the length is found. The subtype is the one already
                 * tabulated for the decode side (bits 3..5 of _read_opt), so no
                 * second table exists to drift from it. Writing the field in
                 * place also spares the fp cases the stack copy the by-value
                 * sofab_ostream_write_fp32/64 wrappers would need. */
                const uint8_t *bytes = CAST_TO(const uint8_t *, src, field->offset);
                size_t fixlen;

                if (field->type == SOFAB_OBJECT_FIELDTYPE_STRING)
                {
                    /* A string's length is its content, not its buffer. */
                    fixlen = strlen((const char *)bytes);
                }
                else if (field->type == SOFAB_OBJECT_FIELDTYPE_BLOB)
                {
                    fixlen = field->size;
                    if (field->nested_idx != 0)
                    {
                        /* Sized blob: emit only used_len bytes (clamped to
                         * capacity). used_len sits immediately before the
                         * buffer. */
                        sofab_unsigned_t used = _load_uint(
                            CAST_TO(const uint8_t *, src,
                                    field->offset - field->nested_idx),
                            field->nested_idx);
                        if (used < fixlen) fixlen = (size_t)used;
                    }
                }
                else
                {
                    /* fp32 / fp64: the value is exactly one element wide. */
                    fixlen = field->element_size;
                }

                ret = sofab_ostream_write_fixlen(ctx, field->id, bytes,
                    (int32_t)fixlen,
                    (sofab_fixlentype_t)_SOFAB_READ_OPT_SUBTYPE(field->type));
                break;
            }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
            {
                // Every array kind starts the same way — base pointer, element
                // width, trimmed element count — so that part is computed once
                // and only the writer differs below.
                const void *base = CAST_TO(const void *, src, field->offset);
                const size_t element_size = field->element_size;
                const int32_t n = element_size != 0
                    ? _array_trim_count(base, field->size / element_size, element_size)
                    : 0;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                if (field->type >= SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32)
                {
                    // A float array carries its subtype in the shared fixlen
                    // word; the subtype is the one _read_opt already records.
                    ret = sofab_ostream_write_array_of_fixlen(ctx, field->id,
                        base, n, (int32_t)element_size,
                        (sofab_fixlentype_t)_SOFAB_READ_OPT_SUBTYPE(field->type));
                    break;
                }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

                // The two integer-array writers share a signature and differ
                // only in the element interpretation; select via pointer so the
                // call itself is emitted once.
                sofab_ret_t (*const write_array)(
                    sofab_ostream_t *, sofab_id_t, const void *, int32_t, int32_t) =
                    (field->type == SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED)
                        ? sofab_ostream_write_array_of_signed
                        : sofab_ostream_write_array_of_unsigned;
                ret = write_array(ctx, field->id, base, n, (int32_t)element_size);
                break;
            }
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
                return SOFAB_RET_E_ARGUMENT;
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

        /* MESSAGE_SPEC §7.3 (a header wire type that contradicts the declared
         * type is skipped like an unknown id) needs no check for a branch that
         * only binds: the istream unbinds a contradicting read and skips the
         * field on its own. Two branches below do more than bind, and those
         * check first -- see the comments there. */
        switch (field->type)
        {
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_SEQUENCE:
            {
                if (decoder->depth == 0) break; // Sequence depth exceeded

                /* The wrapper reset below (MESSAGE_SPEC §7.4) is a side effect on
                 * the destination, so unlike a plain bind it cannot be left to
                 * the istream to undo: a field whose wire type contradicts would
                 * have emptied the array before being skipped. Settle §7.3 here.
                 * It also spares the istream a decoder push it would only have to
                 * pop again. */
                if ((ctx->target_opt & 0x07)
                    != SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_SEQUENCE_START))
                {
                    break;
                }

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
            {
                /* Every remaining field type binds a destination and nothing
                 * else, so one bind serves them all: the option word comes from
                 * _read_opt, the width from the descriptor. A type outside the
                 * table -- past its end, or a hole this configuration does not
                 * implement -- binds nothing, and the field is skipped exactly as
                 * an unknown id is. */
                if (field->type >= (uint8_t)(sizeof(_read_opt)))
                {
                    // Unsupported field type in descriptor
                    break;
                }

                const uint8_t opt = _read_opt[field->type];
#if _READ_OPT_HAS_HOLES
                if (opt == _READ_OPT_NONE)
                {
                    break;
                }
#endif /* _READ_OPT_HAS_HOLES */

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                /* A sized blob is the one type here that does more than bind: it
                 * writes used_len whether or not the bind survives, so a
                 * contradicting field would zero the length of the value already
                 * there. The bind alone would be safe; this is not, so settle the
                 * wire type (and the fixlen subtype) before either happens. */
                const bool sized_blob =
                    (field->type == SOFAB_OBJECT_FIELDTYPE_BLOB &&
                     field->nested_idx != 0);
                if (sized_blob && (ctx->target_opt & 0x3F) != opt)
                {
                    break;
                }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

                /* A string or blob is bound over its whole buffer (its length is
                 * on the wire); every other type is bound one element wide. */
                size_t width = field->element_size;
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                if (field->type == SOFAB_OBJECT_FIELDTYPE_STRING ||
                    field->type == SOFAB_OBJECT_FIELDTYPE_BLOB)
                {
                    width = field->size;
                }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
                if (field->type >= SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED)
                {
                    sofab_istream_read_array(ctx, decoder->dst + field->offset,
                        width != 0 ? field->size / width : 0, width, opt);
                    break;
                }
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

                sofab_istream_read_field(ctx, decoder->dst + field->offset,
                                         width, opt);

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                if (sized_blob)
                {
                    /* Sized blob: record the actual received length in used_len,
                     * which sits immediately before the buffer. */
                    size_t used = size < field->size ? size : field->size;
                    _store_uint(decoder->dst + field->offset - field->nested_idx,
                                field->nested_idx, (sofab_unsigned_t)used);
                }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
                break;
            }
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
