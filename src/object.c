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

/*!
 * @brief The scalar widths this build can load, as one bit per width.
 *
 * 1/2/4/8, narrowing to 1/2/4 when @ref SOFAB_DISABLE_INT64_SUPPORT removes the
 * 64-bit arm — so the set and the @ref _load_uint dispatch stay in agreement by
 * construction. @c element_size is a 4-bit descriptor field, so it never shifts
 * the mask past its width and the test needs no range guard of its own.
 */
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
# define _SOFAB_WIDTH_SET ((1u << 1) | (1u << 2) | (1u << 4) | (1u << 8))
#else
# define _SOFAB_WIDTH_SET ((1u << 1) | (1u << 2) | (1u << 4))
#endif

/*!
 * @brief Load a host-endian unsigned integer of @p width (1/2/4/8) bytes.
 *
 * Two kinds of caller share this one dispatch, which is why it is not gated with
 * the sized-length machinery: an integer field's raw value in
 * @ref sofab_object_encode (whose signed path re-signs the result afterwards),
 * and the companion used-length member of a sized blob, a sized array or a sized
 * wrapper-array holder, whose C type (and thus width) the caller chooses. @p
 * width comes from the descriptor's @c element_size, @c nested_idx (field) or
 * @c fixed_seq (holder). An unsupported width yields 0 (treated as empty; encode
 * screens the width against @ref _SOFAB_WIDTH_SET first).
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

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) || !defined(SOFAB_DISABLE_ARRAY_SUPPORT) \
    || !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
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
        case SOFAB_OBJECT_FIELDTYPE_ARRAY_BOOLEAN:
            return field->nested_idx;
        default:
            return 0;
    }
}
#endif /* fixlen or array support */

/*!
 * @name Tagged union (@ref SOFAB_OBJECT_DESCR_UNION)
 *
 * A union descriptor is a plain struct descriptor whose option fields all
 * overlay the same storage. The tag at offset 0 of the object holds the id of
 * the option received LAST (MESSAGE_SPEC §7.4.1: the last correctly-typed option
 * wins), the way a sized holder's count at offset 0 holds its length. Every walk
 * treats an option that is not the held one as absent: init seeds only the held
 * option, the ≠-default test and encode look at nothing else, and decode records
 * the tag at the same "was bound" point where a holder records its length -- so
 * an option skipped under §7.3 never switches the union.
 *
 * The default image is a PREFIX (object.h): the tag, then the `default_id`
 * option's own bytes when that option is a leaf. A NULL image means tag 0. A held
 * option other than `default_id` is FORCED (MESSAGE_SPEC §2/§4.2): it is never
 * default (@ref _field_is_default), so it is written even at its own default and
 * never compared against the image, which does not carry it -- and a union
 * holding one is never default either, so its parent frames it. A tag that names
 * no option (a caller's stray write) holds nothing: no option is seeded, compared
 * or written, and the union reads as default -- omitted, which a receiver decodes
 * as `default_id` at its default.
 *
 * Shaped for the footprint profile, where flash outranks cycles: the walks test
 * the tag against the field id per field (init, the ≠-default test, the decode
 * re-init) instead of narrowing each walk to the held option up front. That is
 * the least code; a union-free schema pays it as one flag test per field in
 * cycles, and in flash only this code.
 * @{
 */
/* A union needs sequence support -- it IS a sequence on the wire -- so the union
 * walk rides that one condition and needs no switch of its own: without
 * sequences SOFAB_OBJECT_DESCR_UNION sets no bit (object.h), no descriptor is a
 * union, and a non-zero fixed_seq is a holder. */
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
#  define _SOFAB_WITH_UNION 1
/*! A union descriptor: bit 7 of @c fixed_seq. */
#  define _IS_UNION(info) ((info)->fixed_seq & SOFAB_OBJECT_UNION)
/*! A wrapper-array holder: bit 0 of @c fixed_seq (a union sets bit 7 only). */
#  define _IS_HOLDER(info) ((info)->fixed_seq & SOFAB_OBJECT_SEQ_HOLDER)
#else
#  define _IS_UNION(info) 0
#  define _IS_HOLDER(info) ((info)->fixed_seq)
#endif
/*! The tag, READ: the id of the held option, typed like a descriptor field id.
 *  Split from the writing form so a const source stays const -- @ref
 *  _field_is_default and @ref _NOT_HELD only ever read it, and the compiler is
 *  what should say so if that ever stops being true. */
#define _TAG_R(obj) (*(const sofab_object_descr_id_t *)(const void *)(obj))
/*! The tag, WRITTEN: init seeds it, @ref _seq_len_observe switches it. */
#define _TAG_W(obj) (*(sofab_object_descr_id_t *)(void *)(obj))
/*! A field of a union that is not the held option: skipped, as if absent. */
#define _NOT_HELD(info, obj, field) (_IS_UNION(info) && _TAG_R(obj) != (field)->id)
/*! @} */

#if defined(_SOFAB_WITH_UNION)
/*!
 * @brief A union's `default_id`: the default image's leading tag, or 0 without
 *        an image.
 *
 * Out of line rather than a macro: its two callers (init and the ≠-default
 * test) share one body, which is the smaller image on the footprint targets.
 */
static sofab_object_descr_id_t _default_tag(const sofab_object_descr_t *info)
{
    return info->default_values
        ? *(const sofab_object_descr_id_t *)info->default_values : 0u;
}
#endif

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
 *
 * A union (bit 7 only) yields 0x40 here, which is no width: @ref _store_uint and
 * @ref _load_uint ignore it (their default arms), so the two callers that only
 * store or raise a length -- init and @ref _seq_len_observe -- do nothing for a
 * union and need no union test of their own. The one caller that DECIDES on the
 * width, the ≠-default test, masks it (@ref _seq_len_width_masked).
 */
static uint8_t _seq_len_width (const sofab_object_descr_t *info)
{
    return (uint8_t)(info->fixed_seq >> SOFAB_OBJECT_SEQ_LEN_SHIFT);
}

/*! @ref _seq_len_width with the union bit masked off: 0 for a union. */
#define _seq_len_width_masked(info) (_seq_len_width(info) & 0x0Fu)

/* The contract the two no-op callers above rely on, made a compile error rather
 * than a comment: the value a union's fixed_seq yields through _seq_len_width
 * (SOFAB_OBJECT_UNION >> SOFAB_OBJECT_SEQ_LEN_SHIFT) must NOT be a width
 * _load_uint / _store_uint recognise, or init and _seq_len_observe would read and
 * write a length where a union has none. Those two dispatch 1/2/4/8 and ignore
 * everything else, so the contract is that the union bit lands above 8. */
typedef struct {
    int union_bit_must_not_be_a_valid_length_width
        : ((SOFAB_OBJECT_UNION >> SOFAB_OBJECT_SEQ_LEN_SHIFT) > 8u) ? 1 : -1;
} _sofab_union_bit_width_check;

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

#if defined(_SOFAB_WITH_UNION)
    /* A union records the option it now holds instead (MESSAGE_SPEC §7.4.1: the
     * last correctly-typed option wins); the length steps above are no-ops for
     * it (its "width" is the union bit, see _seq_len_width). This is the point
     * where the option is known to be bound, so a child skipped under §7.3 or an
     * unknown id never switches it; a leaf option switched to is overwritten
     * whole by its payload, a sequence option was re-initialised when its frame
     * opened. Tested on the width already in hand: the smallest code. */
    if (width & (SOFAB_OBJECT_UNION >> SOFAB_OBJECT_SEQ_LEN_SHIFT))
        _TAG_W(dst) = (sofab_object_descr_id_t)id;
#endif
}

#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

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
    /* A table, not a switch. The thirteen arms are all constant returns, so the
     * compiler builds a lookup table for them anyway once there are enough --
     * and which side of "enough" a build lands on shifts with every tag added
     * and with every SOFAB_DISABLE_* that removes one. Writing the table out
     * makes the cost the same 16 bytes in every profile instead of a compare
     * chain here and a compiler-invented table there.
     *
     * Each entry holds its option word @b plus @b one, so the entry a disabled
     * or unassigned tag leaves at 0 reads as "no such type in this build" -- no
     * sentinel value to pick, and no risk of it colliding with a real option
     * word (VARINT_UNSIGNED is 0). */
    static const uint8_t opt_plus_one[16] =
    {
        [SOFAB_OBJECT_FIELDTYPE_UNSIGNED] = 1 +
            SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED),
        [SOFAB_OBJECT_FIELDTYPE_SIGNED] = 1 +
            SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED),

        /* A boolean is an unsigned varint on the wire (§4.4), so the §7.3 tag
         * test must see exactly that; the BOOLEAN flag sits outside the 0x3F
         * mask and only tells the istream how to store what it decoded. */
        [SOFAB_OBJECT_FIELDTYPE_BOOLEAN] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED)
             | SOFAB_ISTREAM_OPT_BOOLEAN),

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
        [SOFAB_OBJECT_FIELDTYPE_FP32] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
             | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32)),
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
        [SOFAB_OBJECT_FIELDTYPE_FP64] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
             | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64)),
#endif
        /* the reader also sets STRINGTERM (0x40); it is outside the 0x3F mask */
        [SOFAB_OBJECT_FIELDTYPE_STRING] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
             | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_STRING)),
        [SOFAB_OBJECT_FIELDTYPE_BLOB] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLEN)
             | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_BLOB)),
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
        [SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED] = 1 +
            SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED),
        [SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED] = 1 +
            SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED),
        [SOFAB_OBJECT_FIELDTYPE_ARRAY_BOOLEAN] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED)
             | SOFAB_ISTREAM_OPT_BOOLEAN),
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
        [SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
             | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP32)),
#if !defined(SOFAB_DISABLE_FP64_SUPPORT)
        [SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64] = 1 +
            (SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_FIXLENARRAY)
             | SOFAB_ISTREAM_OPT_FIXLENTYPE(SOFAB_FIXLENTYPE_FP64)),
#endif
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
        [SOFAB_OBJECT_FIELDTYPE_SEQUENCE] = 1 +
            SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_SEQUENCE_START),
#endif
    };

    /* One entry per value a four-bit field can hold, so the index cannot leave
     * the table and there is no bounds check to pay for. That is why the table
     * is 16 long and not 13: the three unassigned tags cost three bytes and buy
     * the branch away. */
    return (int)opt_plus_one[type] - 1;
}


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
#if defined(_SOFAB_WITH_UNION)
    /* The union rule, decided here for both callers -- encode, and the SEQUENCE
     * branch below one level up -- so neither loop tests the tag itself:
     *  - an option the union does not hold is absent, hence default: encode
     *    skips it, and it never makes the union non-default;
     *  - the held option, when it is not `default_id`, is never default: it is
     *    FORCED (MESSAGE_SPEC §4.2) -- written even at its own default, a scalar
     *    as its value, a string/blob empty, an array as count 0, a sequence
     *    option as a present frame -- because a receiver's fresh union holds
     *    `default_id` and would read the omission back as that. The same answer
     *    one level up makes the union holding it not default, so its parent
     *    frames it;
     *  - `default_id` held is compared against its default like any field. */
    if (_IS_UNION(info))
    {
        if (field->id != _TAG_R(src)) return 1;
        if (field->id != _default_tag(info)) return 0;
    }
#endif

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    if (field->type == SOFAB_OBJECT_FIELDTYPE_SEQUENCE)
    {
        const sofab_object_descr_t *ninfo = info->nested_list[field->nested_idx];
        const void *nsrc = CAST_TO(const void *, src, field->offset);

        if (_seq_len_width_masked(ninfo) != 0)
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
#endif

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) || !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    {
        uint8_t width = _sized_width(field);
        if (width != 0)
        {
            /* Sized blob / sized array: length first (§3 -- the length is the
             * value), then the used prefix against the default image. A blob is
             * an array of one-byte elements here. Without a default image the
             * logical default is the empty value. The buffer past the used length
             * is indeterminate and never influences the decision.
             *
             * The 1 a blob keeps is load-bearing, not shorthand: for a BLOB the
             * descriptor macros record sizeof(buffer) & 0xF in element_size -- the
             * CAPACITY, not an element width -- which is 0 at a capacity of 16, so
             * reading it here would divide by zero. An #if rather than a ternary
             * because it also pays: with arrays disabled the blob is the only kind
             * that reaches this block, esz folds to a constant, and neither the
             * division (a libgcc call on a core without one) nor the multiply is
             * emitted at all. */
            size_t esz = 1;
#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            if (field->type != SOFAB_OBJECT_FIELDTYPE_BLOB)
                esz = field->element_size;
#endif
            uint64_t used = _load_uint(
                CAST_TO(const void *, src, field->offset - width), width);
            size_t cap = field->size / esz;
            if (used > (uint64_t)cap) used = (uint64_t)cap;

            if (defaults == NULL) return used == 0;

            if (_load_uint(CAST_TO(const void *, defaults, field->offset - width),
                           width) != used)
                return 0;
            return memcmp(CAST_TO(const void *, defaults, field->offset), val,
                          (size_t)used * esz) == 0;
        }
    }
#endif /* fixlen or array support */

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

#if defined(_SOFAB_WITH_UNION)
    /* A union holds `default_id` at that option's default: the tag first, then
     * the loop below seeds that one option only. */
    if (_IS_UNION(info))
        _TAG_W(obj) = _default_tag(info);
#endif

    for (size_t i = 0; i < info->field_count; i++)
    {
        const sofab_object_descr_field_t *field = &info->field_list[i];

#if defined(_SOFAB_WITH_UNION) && !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
        /* A union's BLOB option must carry its length
         * (SOFAB_OBJECT_FIELD_BLOB_SIZED, see object.h). A capacity-only blob
         * cannot represent the M < N a peer may send, so the bytes the wire does
         * not carry would keep whatever the object held -- in a union, another
         * option's payload, which would then be re-encoded as part of this one.
         * A descriptor that does it is wrong by construction, so this is an
         * assert -- this port's mechanism for an out-of-range argument -- and
         * costs nothing under NDEBUG. Checked for every option, not just the held
         * one, so one init of a fresh object screens the whole descriptor. */
        assert(!(_IS_UNION(info)
                 && field->type == SOFAB_OBJECT_FIELDTYPE_BLOB
                 && _sized_width(field) == 0));
#endif

        if (_NOT_HELD(info, obj, field)) continue;

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
    if (_IS_HOLDER(info))
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
            case SOFAB_OBJECT_FIELDTYPE_SIGNED:
            /* A boolean joins the unsigned arm unchanged, with no normalizing
             * step of its own: §4.4 is canonical on encode, and the source of an
             * encode is the caller's own `bool`, which holds 0 or 1 and is that
             * canonical form already. Only a decode sees foreign bytes, which is
             * why only the decode side normalizes. A runtime `!= 0` here would
             * buy nothing and cost a compare on every unsigned field. */
            case SOFAB_OBJECT_FIELDTYPE_BOOLEAN:
            {
                // Both types read the same bytes and differ only in how they are
                // re-signed, so they share one width dispatch (_load_uint, which
                // the sized blob/array paths already carry) instead of a 1/2/4/8
                // load chain each. _load_uint's own default arm is the width
                // check: it yields 0 for an unsupported size, which the set test
                // below rejects first.
                const uint8_t width = field->element_size;
                if (((_SOFAB_WIDTH_SET >> width) & 1u) == 0)
                {
                    return SOFAB_RET_E_ARGUMENT; // Unsupported size (8 requires 64-bit values)
                }

                sofab_unsigned_t val = (sofab_unsigned_t)_load_uint(
                    CAST_TO(const void *, src, field->offset), width);

                if (field->type == SOFAB_OBJECT_FIELDTYPE_SIGNED)
                {
                    // Re-sign the loaded low bytes. A cast per width, not a shift
                    // by a computed amount: the widths are a fixed set, so each
                    // arm is a single sign-extend instruction, while a variable
                    // shift of a 64-bit value costs a multi-instruction sequence
                    // on a 32-bit target.
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
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_BOOLEAN:
            {
                // Both writers share a signature and differ only in the element
                // interpretation; select via pointer so the element-count math
                // and the call are emitted once.
                sofab_ret_t (*const write_array)(
                    sofab_ostream_t *, sofab_id_t, const void *, int32_t, int32_t) =
                    (field->type == SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED)
                        ? sofab_ostream_write_array_of_signed
                        : sofab_ostream_write_array_of_unsigned;
                /* A boolean array takes the unsigned writer unchanged: its
                 * elements are `bool` objects, which hold 0 or 1 and are already
                 * the canonical form §4.4 asks for. Only the decode side needs
                 * the normalizing store, because only it sees foreign bytes. */
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
            default:
            {
                /* Every field type but SEQUENCE does the same three things here:
                 * settle §7.3 if the destination is sized, bind, and write the
                 * companion length. Only the option word, the width and
                 * field-vs-array bind differ, so one body serves them all --
                 * eleven near-identical arms otherwise, each repeating the option
                 * word _expected_opt already tabulates for the §7.3 checks.
                 * Sharing it is also what keeps the expected type and the bound
                 * type from drifting apart. */
                const int expected = _expected_opt(field->type);
                if (expected < 0)
                {
                    // Unsupported field type in descriptor
                    break;
                }
                const uint8_t opt = (uint8_t)expected;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) || !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
                /* A SIZED blob or array writes its length member whether or not
                 * the bind survives, so the wire type is settled first: a
                 * contradicting field (§7.3) would otherwise reset the length of
                 * the value already there. A plain bind only binds, which the
                 * istream rolls back on its own. */
                const uint8_t sized = _sized_width(field);
                if (sized != 0 && (ctx->target_opt & 0x3F) != (opt & 0x3F))
                {
                    break;
                }
#endif /* fixlen or array support */

                /* A string or blob is bound over its whole buffer (its length is
                 * on the wire); everything else is bound one element wide, which
                 * for fp32/fp64 is the 4/8 the descriptor macros record. */
                size_t width = field->element_size;
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                if (field->type == SOFAB_OBJECT_FIELDTYPE_STRING ||
                    field->type == SOFAB_OBJECT_FIELDTYPE_BLOB)
                {
                    width = field->size;
                }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
                /* Read the array-ness off the wire form _expected_opt already
                 * tabulated, not off the tag's numeric value: the three array
                 * wire types are the top of that enum, and SEQUENCE has its own
                 * case above and never arrives here. A test on the tag number
                 * instead would silently mis-route every scalar tag added after
                 * the array tags -- BOOLEAN was the first. */
                if (SOFAB_ISTREAM_OPT_FIELDTYPE(opt) >= SOFAB_TYPE_VARINTARRAY_UNSIGNED)
                {
                    sofab_istream_read_array(ctx, decoder->dst + field->offset,
                        width != 0 ? field->size / width : 0, width, opt);
                    _store_array_len(field, decoder->dst, count);
                    break;
                }
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

                sofab_istream_read_field(ctx, decoder->dst + field->offset, width,
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                    field->type == SOFAB_OBJECT_FIELDTYPE_STRING
                        ? (uint8_t)(opt | SOFAB_ISTREAM_OPT_STRINGTERM) :
#endif
                    opt);

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                if (sized != 0)
                {
                    /* Sized blob: record the actual received length in used_len,
                     * which sits immediately before the buffer. */
                    size_t used = size < field->size ? size : field->size;
                    _store_uint(decoder->dst + field->offset - sized, sized,
                                (uint64_t)used);
                }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
                break;
            }

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
                // structs (fixed_seq == 0) and unions (bit 7 only) keep merging
                // untouched.
                //
                // Scope: this fires when a wrapper is OPENED on the wire, which
                // is all §7.4 is about (occurrences *within* one message). It is
                // not a per-decode reset of the destination: a wrapper field the
                // message omits entirely — the canonical form of an all-default
                // one since §2 — opens nothing here, so whatever the destination
                // held stays. Re-using a destination across messages therefore
                // requires sofab_object_init() between decodes, exactly as it
                // always has for an omitted leaf field (see object.h).
                //
                // A union option switched to starts from its default
                // (MESSAGE_SPEC §7.4.1), before any of its children arrive. Past
                // the wire-type test above the frame is bound (a sequence read
                // always binds), so this is the "was bound" point; the tag
                // follows below. The held option received again merges (§7.4).
                if (_IS_HOLDER(nested->info) || _NOT_HELD(info, decoder->dst, field))
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
    if (_IS_HOLDER(info) && info->field_count != 0)
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
