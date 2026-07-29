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

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) || !defined(SOFAB_DISABLE_ARRAY_SUPPORT) \
    || !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
/*!
 * @brief Load a host-endian unsigned integer of @p width (1/2/4/8) bytes.
 *
 * Reads the companion used-length member of a sized blob, a sized array or a
 * sized wrapper-array holder, whose C type (and thus width) the caller chooses;
 * @p width comes from the descriptor's @c nested_idx (field) or @c fixed_seq
 * (holder). An unsupported width yields 0 (treated as empty).
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
#endif /* fixlen or array or sequence support */

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) || !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
/*!
 * @brief Byte width of a field's companion length member, or 0 when it has none.
 *
 * A BLOB or an ARRAY_* field carries its used length in a member declared
 * immediately before its buffer; the descriptor stores only that member's width,
 * in the @c nested_idx slot, which therefore doubles as the "is sized" flag
 * (@ref SOFAB_OBJECT_FIELD_BLOB_SIZED, @ref SOFAB_OBJECT_FIELD_ARRAY_SIZED). The
 * type test is what keeps a SEQUENCE out of it: there @c nested_idx is the nested
 * descriptor index, never a width.
 */
static uint8_t _sized_width (const sofab_object_descr_field_t *field)
{
    switch (field->type)
    {
        case SOFAB_OBJECT_FIELDTYPE_BLOB:
        case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
        case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
        case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
        case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
            return field->nested_idx;
        default:
            return 0;
    }
}
#endif /* fixlen or array support */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
/*!
 * @brief Byte width of a wrapper-array holder's element-count member (0 = none).
 *
 * The holder counterpart of @ref _sized_width: a holder descriptor packs the
 * "is a holder" flag in bit 0 of @c fixed_seq and the width of its companion
 * element-count member above @ref SOFAB_OBJECT_SEQ_LEN_SHIFT
 * (@ref SOFAB_OBJECT_DESCR_SEQ_SIZED). A plain object (@c fixed_seq @c == @c 0)
 * and an un-sized holder (@ref SOFAB_OBJECT_DESCR_SEQ, @c fixed_seq @c == @c 1)
 * both yield 0, which is what every "does it carry a length?" test below asks.
 */
static uint8_t _seq_len_width (const sofab_object_descr_t *info)
{
    return (uint8_t)(info->fixed_seq >> SOFAB_OBJECT_SEQ_LEN_SHIFT);
}

/*!
 * @def _SEQ_LEN_OFFSET
 * @brief Offset of that element-count member inside the holder object: zero.
 *
 * A holder's count sits at the START of the holder — @ref
 * SOFAB_OBJECT_DESCR_SEQ_SIZED asserts @c offsetof(obj,lfield) @c == @c 0 at
 * compile time — and NOT one width before the first element slot, the way a sized
 * blob's / sized array's length sits before its buffer.
 *
 * An object descriptor can afford that anchor, because it describes the whole
 * object; and it has to use it, because the byte before slot 0 is not free in
 * every holder. A blob element and a native inner-array row are themselves SIZED:
 * each slot BEGINS with its own used-length, so "one width before the slots"
 * addressed element 0's length instead of the holder's count, and the sized
 * holder worked for three of the five element kinds only. Offset 0 has no such
 * competition, and it needs no adjacency argument either — padding between a
 * narrow count and strictly-aligned slots is harmless, because nothing is
 * measured from the slots.
 *
 * The FIELD-level SIZED forms keep the old convention: a field descriptor knows
 * only the field's own offset inside its object, so "immediately before the
 * storage" is the only anchor available to it (@ref
 * SOFAB_OBJECT_ASSERT_LEN_ADJACENT).
 */
#define _SEQ_LEN_OFFSET ((size_t)0)

/*!
 * @brief Number of element slots a holder's value actually occupies.
 *
 * MESSAGE_SPEC §5.1: a wrapper array's length is *highest present id + 1*, and
 * @c count is only its capacity — so a sized holder reads the length from its
 * companion member (clamped to the capacity, mirroring @ref _array_count), while
 * an un-sized one has no length to read and its value occupies every slot.
 * The result drives both encode bounds: the slots @c [0, len) are walked and the
 * slot at @c len @c - @c 1 is the one that is always written.
 */
static size_t _seq_len (const sofab_object_descr_t *info, const void *obj)
{
    uint8_t width = _seq_len_width(info);
    size_t n = info->field_count;

    if (width != 0)
    {
        uint64_t used = _load_uint(
            CAST_TO(const void *, obj, _SEQ_LEN_OFFSET), width);
        if (used < (uint64_t)n) n = (size_t)used;
    }

    return n;
}

/*!
 * @brief Decode: raise a sized holder's length to cover element id @p id.
 *
 * The mirror of @ref _store_array_len one level up. A wrapper carries no length
 * field, so the decoder derives it from what arrived — *highest present id + 1*
 * (MESSAGE_SPEC §5.1) — and stores it back into the companion member, exactly as a
 * sized blob stores its received byte length. The maximum (rather than a plain
 * assignment) keeps the result independent of element order; an over-index id
 * never reaches here (it matches no slot, so it is rejected under §7/§7.1 or
 * skipped under §7.3, and either way returns first) and the clamp is belt and
 * braces.
 * An un-sized holder has nowhere to put it and this is a no-op.
 *
 * "Present" is an element that was actually **bound** as an element — the call
 * site decides that, and an element skipped under §7.3 never gets here.
 */
static void _seq_len_observe (const sofab_object_descr_t *info,
                              uint8_t *dst, sofab_id_t id)
{
    uint8_t width = _seq_len_width(info);
    size_t off, len;

    if (width == 0) return;

    off = _SEQ_LEN_OFFSET;
    len = (size_t)id + 1u;
    if (len > (size_t)info->field_count) len = (size_t)info->field_count;

    if ((uint64_t)len > _load_uint(dst + off, width))
    {
        _store_uint(dst + off, width, (uint64_t)len);
    }
}

/*!
 * @brief The wire opt a field of descriptor type @p type would install.
 *
 * The pair (wire opt, expected opt) is what settles MESSAGE_SPEC §7.3 — a header
 * whose wire type, or whose fixlen subtype, contradicts the declared type. For a
 * MATCHED id the expected opt arrives for free: the read that binds the slot
 * writes it into @c ctx->target_opt. An UNMATCHED id binds nothing, so the same
 * value has to be derived from the descriptor, which is what this does.
 *
 * The result is meant for the @c 0x3F mask (field type + fixlen subtype), the one
 * the matched-id test and the sized-blob guard already use: the string reader's
 * @ref SOFAB_ISTREAM_OPT_STRINGTERM bit (0x40) sits outside it, and a varint array
 * carries no subtype on either side, so the two agree there too.
 *
 * @param type  A @ref SOFAB_OBJECT_FIELDTYPE_UNSIGNED "SOFAB_OBJECT_FIELDTYPE_*" tag.
 * @return The expected opt (0..0x3F), or -1 for a descriptor type that has no wire
 *         expectation (an unsupported tag) — the caller must not claim a
 *         contradiction it cannot establish.
 */
static int _expected_opt (uint8_t type)
{
    switch (type)
    {
        case SOFAB_OBJECT_FIELDTYPE_UNSIGNED:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED);

        case SOFAB_OBJECT_FIELDTYPE_SIGNED:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED);

        case SOFAB_OBJECT_FIELDTYPE_FP32:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                 | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32);

        case SOFAB_OBJECT_FIELDTYPE_FP64:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                 | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64);

        case SOFAB_OBJECT_FIELDTYPE_STRING:
            /* the reader also sets STRINGTERM (0x40); it is outside the 0x3F mask */
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                 | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_STRING);

        case SOFAB_OBJECT_FIELDTYPE_BLOB:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                 | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_BLOB);

        case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED);

        case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED);

        case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
                 | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32);

        case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
                 | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64);

        case SOFAB_OBJECT_FIELDTYPE_SEQUENCE:
            return SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_SEQUENCE_START);

        default:
            return -1;
    }
}
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
/*!
 * @brief Element count to encode for a compact array field (MESSAGE_SPEC §3).
 *
 * A schema @c count:N is a @b capacity; the wire count @c M is the array's
 * @b length, and @b every element the field holds is written — a trailing element
 * equal to the element default included, because dropping it would shorten the
 * array (@c [1,2,3,0,0] and @c [1,2,3] are different values).
 *
 * Where that length comes from depends on the descriptor:
 * - @ref SOFAB_OBJECT_FIELD_ARRAY_SIZED (@c nested_idx @c != @c 0) reads the
 *   companion length member sitting immediately before the buffer, clamped to the
 *   capacity;
 * - a plain @ref SOFAB_OBJECT_FIELD_ARRAY has no length member, so its value
 *   occupies the whole array and the count is the capacity itself.
 *
 * (Until 0.8.x this function was @c _array_trim_count and elided the trailing
 * run of element defaults, refilled by a decoder to @c N. That pair was correct
 * only while @c count meant a fixed length; under the capacity reading it
 * silently shortens the value, so it is gone — MESSAGE_SPEC §3.)
 *
 * @param field  Field descriptor (array type).
 * @param src    Object being encoded.
 * @return The element count to write, in @c [0, N].
 */
static int32_t _array_count (const sofab_object_descr_field_t *field, const void *src)
{
    size_t n = field->size / field->element_size;   /* capacity N */
    uint8_t width = _sized_width(field);

    if (width != 0)
    {
        uint64_t used = _load_uint(
            CAST_TO(const void *, src, field->offset - width), width);
        if (used < (uint64_t)n) n = (size_t)used;
    }

    return (int32_t)n;
}

/*!
 * @brief Record the received element count of a sized array on decode.
 *
 * MESSAGE_SPEC §3: the wire count @c M @b is the array's length, so a
 * length-carrying descriptor (@ref SOFAB_OBJECT_FIELD_ARRAY_SIZED) stores it back
 * into the companion member — the mirror of the sized-blob flow, and what makes a
 * decode of @c M @c < @c N re-encode as @c M elements again instead of silently
 * growing back to the capacity. A plain (capacity-only) array descriptor has
 * nowhere to put it and this is a no-op; its trailing @c [M, N) slots are left at
 * the element default by the istream.
 *
 * @param field  Field descriptor (array type).
 * @param dst    Destination object.
 * @param count  Element count delivered to the field callback (the wire count).
 */
static void _store_array_len (const sofab_object_descr_field_t *field,
                              uint8_t *dst, size_t count)
{
    uint8_t width = _sized_width(field);
    size_t cap;

    if (width == 0) return;

    /* An over-count message is rejected by the istream; clamp so the stored
     * length can never exceed the buffer it describes in the meantime. */
    cap = field->size / field->element_size;
    _store_uint(dst + field->offset - width, width,
                (uint64_t)(count < cap ? count : cap));
}
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

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
 * defaults, and its padding must be ignored. It powers both omission decisions in
 * @ref sofab_object_encode: MESSAGE_SPEC §2 omission of a @e standalone SEQUENCE
 * field (an all-default one is dropped, not framed empty), and the §2/§5.1
 * omission of an @e interior wrapper-array element, which since the capacity
 * reading of §3 covers sequence-form elements too (they leave an id gap like any
 * other default element). The element at a holder's @e last index is never tested
 * against this — it always goes on the wire, because it is what carries the
 * array's length.
 *
 * A @b sized array (@ref SOFAB_OBJECT_FIELD_ARRAY_SIZED) compares by length
 * first, exactly like a sized blob: the storage past the used length is
 * indeterminate, and an all-zero storage image would otherwise make a
 * three-element @c [0,0,0] indistinguishable from the empty array and cost it its
 * length.
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

        if (_seq_len_width(ninfo) != 0)
        {
            /* Sized wrapper holder: the length IS the value (§5.1), so the test is
             * "is the array empty?" and nothing else -- exactly like a sized blob
             * or a sized array. Scanning the slots would be wrong twice over: their
             * content past the used length is indeterminate, and a non-empty
             * all-default array such as ["", ""] is NOT the empty array and must
             * keep its final element. The holder carries no default image
             * (SOFAB_OBJECT_DESCR_SEQ_SIZED passes NULL), so its declared default
             * is the empty array. */
            return _seq_len(ninfo, nsrc) == 0;
        }

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

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    {
        uint8_t width = _sized_width(field);
        if (width != 0 && field->type != SOFAB_OBJECT_FIELDTYPE_BLOB)
        {
            /* Sized array: length first (§3 -- the length is the value), then the
             * used prefix against the default image. Without a default image the
             * logical default is the empty array. */
            uint64_t used = _load_uint(
                CAST_TO(const void *, src, field->offset - width), width);
            size_t cap = field->size / field->element_size;
            if (used > (uint64_t)cap) used = (uint64_t)cap;

            if (defaults == NULL) return used == 0;

            if (_load_uint(CAST_TO(const void *, defaults, field->offset - width),
                           width) != used)
                return 0;
            return memcmp(CAST_TO(const void *, defaults, field->offset), val,
                          (size_t)used * field->element_size) == 0;
        }
    }
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

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

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    /* A sized wrapper holder's element-count member sits at offset 0 of the holder
     * and no field descriptor covers it (offset, size), so the loop below never
     * reaches it -- the same blind spot the sized blob had in issue #106. Clear it:
     * the holder carries no default image, so its declared default is the empty
     * array, i.e. length 0. This is also the §7.4 reset a re-opened wrapper runs,
     * which is what keeps a replaced array from reporting the previous length. */
    {
        uint8_t seq_width = _seq_len_width(info);
        if (seq_width != 0)
        {
            _store_uint(CAST_TO(void *, obj, _SEQ_LEN_OFFSET), seq_width, 0);
        }
    }
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

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

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) || !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            /* Sized blob / sized array: the used-length member sits width bytes
             * before the buffer and is not covered by (offset, size); reset it
             * too, mirroring _field_is_default / encode / decode which all address
             * it at offset - width. Without this a §7.4 wrapper re-open leaves a
             * stale length, so a dropped element survives as an all-zero value. */
            {
                uint8_t width = _sized_width(field);
                if (width != 0)
                {
                    uint64_t dlen = info->default_values != NULL
                        ? _load_uint(CAST_TO(const void *, info->default_values,
                                             field->offset - width), width)
                        : 0;
                    _store_uint(CAST_TO(void *, obj, field->offset - width),
                                width, dlen);
                }
            }
#endif /* fixlen or array support */
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
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    size_t count;
    size_t last;
#endif
    assert(ctx != NULL);
    assert(info != NULL);
    assert(src != NULL);

    /*
     * MESSAGE_SPEC §2/§5.1, positional element rule inside a wrapper-array holder
     * (info->fixed_seq): a wrapper carries no length field, so the decoded length
     * is *highest present id + 1* -- nothing that carries it may be elided, and
     * everything else may be. The element at the LAST index is therefore always
     * written (a leaf as its value, a sequence element as an empty frame), while
     * an interior element equal to its default is omitted whatever its kind.
     *
     * "Last index" is length - 1, and _seq_len says where the length comes from:
     * a SIZED holder (SOFAB_OBJECT_DESCR_SEQ_SIZED) reads its companion element-
     * count member, so slots at or past the length are not walked at all and all
     * of 0..N are expressible; an un-sized holder has no length member, so its
     * value occupies every slot and the last index is field_count - 1. Length 0
     * writes nothing: it is the empty array, which the FIELD-level ≠-default test
     * in the enclosing object omits whole (the canonical encoding, §2) and which a
     * re-decode reconstructs exactly.
     *
     * `last` stays SIZE_MAX for a plain object, where no field is at an element
     * position and the per-field skip applies unconditionally. Without sequence
     * support no holder can be reached at all, so the minimal profile compiles the
     * plain field walk it always had, byte for byte.
     *
     * (Until 0.8.x this spot elided the trailing run of all-default elements,
     * refilled to N by the decoder. §3 made `count` a capacity, so that trim
     * shortens the value instead of compacting it, and it is gone.)
     */
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    count = info->field_count;
    last  = (size_t)-1;
    if (info->fixed_seq)
    {
        count = _seq_len(info, src);
        last  = count - 1u;   /* count == 0 -> SIZE_MAX, and the loop never runs */
    }
#  define _SOFAB_FIELD_COUNT count
#  define _SOFAB_ELEMENT_HELD(i) ((i) == last)
#else
#  define _SOFAB_FIELD_COUNT (info->field_count)
#  define _SOFAB_ELEMENT_HELD(i) 0
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

    for (size_t i = 0; i < _SOFAB_FIELD_COUNT && ret == SOFAB_RET_OK; i++)
    {
        const sofab_object_descr_field_t *field = &info->field_list[i];

        /*
         * MESSAGE_SPEC §2: the ≠-default test is per field, and a SEQUENCE
         * (nested object) is no exception -- a sequence opens an id scope and
         * nothing more (CORELIB_PLAN §3), so it carries no value of its own and
         * an all-default one carries no information. _field_is_default compares a
         * SEQUENCE **per child field, recursively** against the nested
         * descriptor's declared-default image, never as a raw byte image, so
         * struct padding never enters the decision and a non-zero nested default
         * is handled by the same per-field test as everywhere else. Absence
         * reconstructs exactly that default (sofab_object_init), so the omission
         * is value-preserving by construction.
         *
         * Inside a wrapper holder the same test decides an ELEMENT, with one
         * positional exception: the slot at `last` is written whatever it holds
         * (the rule above). Everything before it is sparse -- a default leaf
         * element is skipped and a default sequence-form element is not framed
         * either, both leaving an id gap the decoder refills from the element
         * default.
         */
        if (!_SOFAB_ELEMENT_HELD(i) && _field_is_default(info, field, src))
        {
            // Field value matches its default, skip serialization
            continue;
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
                    return SOFAB_RET_E_ARGUMENT; // Unsupported size (8 requires 64-bit values)

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
                    return SOFAB_RET_E_ARGUMENT; // Unsupported size (8 requires 64-bit values)

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
                    _array_count(field, src),
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
                    _array_count(field, src),
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
                return SOFAB_RET_E_ARGUMENT;
        }
    }
#undef _SOFAB_FIELD_COUNT
#undef _SOFAB_ELEMENT_HELD

    return ret;
}

extern void sofab_object_field_cb (sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    sofab_object_decoder_t *decoder = (sofab_object_decoder_t *)usrptr;
    const sofab_object_descr_t *info = decoder->info;

#if defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
    (void)size;   /* consumed only by the sized-blob branch (fixlen) below */
#endif
#if defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    (void)count;  /* consumed only by the sized-array branches (array) below */
#endif

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    /* The wire field type as the callback finds it: sofab_istream_* set
     * ctx->target_opt to the type read from the header (plus the fixlen subtype
     * once it is known) and reset target_ptr, both immediately before calling
     * here. Every read below overwrites target_opt with the type it EXPECTS, so
     * the pair (wire type, expected type) is only available if the incoming one
     * is captured first. It is needed to tell a bound element from a skipped one
     * -- see the element-count update after the switch. */
    const uint8_t wire_opt = ctx->target_opt;
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

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
                /* A sized blob writes used_len whether or not the bind survives,
                 * so a contradicting field would zero the length of the value
                 * already there. The bind alone would be safe; this is not, so
                 * settle the wire type (and the fixlen subtype) first. */
                if (field->nested_idx != 0
                    && (ctx->target_opt & 0x3F)
                        != (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_BLOB)))
                {
                    break;
                }

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
            /* A SIZED array writes its length member whether or not the bind
             * survives, so — exactly like the sized blob above — the wire type is
             * settled first: a contradicting field (§7.3) would otherwise reset the
             * length of the value already there. A plain array only binds, which
             * the istream rolls back on its own. */
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
                // unsigned and signed arrays differ only in the wire type tag
                if (_sized_width(field) != 0
                    && (ctx->target_opt & 0x07)
                        != SOFAB_ISTREAM_OPT_FIELDTYPE(
                            field->type == SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED
                                ? SOFAB_TYPE_VARINTARRAY_SIGNED : SOFAB_TYPE_VARINTARRAY_UNSIGNED))
                {
                    break;
                }

                sofab_istream_read_array(ctx,
                    decoder->dst + field->offset,
                    field->size / field->element_size, field->element_size,
                    SOFAB_ISTREAM_OPT_FIELDTYPE(
                        field->type == SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED
                            ? SOFAB_TYPE_VARINTARRAY_SIGNED : SOFAB_TYPE_VARINTARRAY_UNSIGNED));
                _store_array_len(field, decoder->dst, count);
                break;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
                if (_sized_width(field) != 0
                    && (ctx->target_opt & 0x3F)
                        != (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32)))
                {
                    break;
                }

                sofab_istream_read_array_of_fp32(ctx,
                    (float *)(decoder->dst + field->offset),
                    field->size / sizeof(float));
                _store_array_len(field, decoder->dst, count);
                break;

#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
                if (_sized_width(field) != 0
                    && (ctx->target_opt & 0x3F)
                        != (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
                            | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64)))
                {
                    break;
                }

                sofab_istream_read_array_of_fp64(ctx,
                    (double *)(decoder->dst + field->offset),
                    field->size / sizeof(double));
                _store_array_len(field, decoder->dst, count);
                break;
#endif /* !defined(SOFAB_DISABLE_FP64_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

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
                //
                // Scope: this fires when a wrapper is OPENED on the wire, which
                // is all §7.4 is about (occurrences *within* one message). It is
                // not a per-decode reset of the destination: a wrapper field the
                // message omits entirely — the canonical form of an all-default
                // one since §2 — opens nothing here, so whatever the destination
                // held stays. Re-using a destination across messages therefore
                // requires sofab_object_init() between decodes, exactly as it
                // always has for an omitted leaf field (see object.h).
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

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
        /* MESSAGE_SPEC §5.1: inside a SIZED wrapper holder the array's length is
         * *highest present id + 1*, so record it -- the mirror of the sized blob's
         * stored byte length, and what lets a received [{k:1}] re-encode as one
         * element instead of growing back to the capacity. A §7.4 re-open resets
         * the member first (sofab_object_init), so each occurrence reports its own
         * length.
         *
         * Only an element that was actually BOUND counts. §7.3 says a field whose
         * header wire type contradicts the declared one "MUST be skipped, exactly
         * as a field with an unknown id is skipped" -- and an unknown id leaves
         * nothing behind: no value, no id occupied, no container mutation. So the
         * ids §5.1 counts are the ids that were consumed as elements, not the ids
         * that merely appeared on the wire. A mistyped child is reconstructed from
         * the element default like any absent one, and the array is byte-for-byte
         * what it would have been had the child never arrived.
         *
         * Two conditions say "bound": the branch above bound a destination at all
         * (a branch that declined -- a settled §7.3 pre-check, an unsupported
         * descriptor type, an exhausted sequence depth -- leaves target_ptr NULL),
         * and the type it bound agrees with the wire. The second test is the same
         * one the istream applies after this callback returns to unbind a
         * contradicting read (_call_field_callback_masked); running it here too is
         * what lets the decision be made before the count is touched. Mask 0x3F =
         * field type + fixlen subtype, so a blob-for-string is a mismatch; the
         * string reader's STRINGTERM bit (0x40) sits outside it, as does the
         * subtype-less form the istream matches with 0x07 (an empty varint array
         * carries no subtype on either side, so 0x3F agrees with it).
         *
         * A well-typed but EMPTY element -- an empty frame, an empty string -- is
         * bound and therefore present: it counts, and must. That is the control
         * this test must not swallow. */
        if (ctx->target_ptr != NULL
            && ((unsigned)(ctx->target_opt ^ wire_opt) & 0x3Fu) == 0u)
        {
            _seq_len_observe(info, decoder->dst, id);
        }
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

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
    //
    // §7.3 is decided FIRST, though (issue #117, Crucible F-0041). A header whose
    // wire type -- or, for a fixlen element type, whose fixlen subtype --
    // contradicts the declared type "MUST be skipped, exactly as a field with an
    // unknown id is skipped", and "against a schema bound, this clause wins". A
    // skipped field is not an element, so its id is not an array index and there is
    // no index for the count to bound: CORELIB_PLAN §4.8, "the field was never this
    // array's value"; §7.4, "an occurrence skipped under §7.3 is not an occurrence
    // for this clause". The bound applies only to a field that SURVIVES the test --
    // which is the very test the matched-id path runs before _seq_len_observe
    // above, with the same 0x3F mask (field type + fixlen subtype). Reading it the
    // other way round is what split the roster: 11 implementations skip here, this
    // one rejected.
    //
    // An over-index element has no declared slot of its own, so "the declared type"
    // §7.3 tests against is the ARRAY's element type. Wrapper elements are
    // homogeneous (§5.1), so slot 0 carries it and no descriptor member is needed.
    //
    // The ordering this needs is already in place upstream: sofab_istream_feed ORs
    // the fixlen subtype into target_opt before it calls this callback, so the
    // subtype is known here, and a message that ends between an element header and
    // its fixlen word never reaches the callback at all -- it stays INCOMPLETE
    // (§5.2), as it already did. Nor is the reject deferred any further: from the
    // fixlen word on it fires without waiting for a single payload byte.
    //
    // Format-level rejects are untouched. An over-wide varint, an id past ID_MAX,
    // ARRAY_MAX, a reserved fixlen subtype, a bad fp width and MAX_DEPTH all fire
    // in the istream, before or independently of this callback. §7.3 subordinates
    // the SCHEMA bound only (CORELIB_PLAN §4.8: "the format ceiling still fires on
    // the count word whatever the subtype turns out to be").
    if (info->fixed_seq && info->field_count != 0)
    {
        /* field_count == 0 above: a holder with no slot has no element type to
         * contradict, so §7.3 cannot be settled -- skip, never reject.
         * expected < 0 below: an unsupported descriptor tag is likewise no
         * expectation, so no contradiction can be claimed and the bound stands. */
        const int expected = _expected_opt(info->field_list[0].type);

        if (expected < 0
            || ((unsigned)(wire_opt ^ (uint8_t)expected) & 0x3Fu) == 0u)
        {
            sofab_istream_invalidate(ctx);
        }
    }
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */
}
