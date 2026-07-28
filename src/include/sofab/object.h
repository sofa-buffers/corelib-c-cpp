/*!
 * @file object.h
 * @brief SofaBuffers C - Object encoder and decoder.
 *
 * This module implements a generic message encoder and decoder.
 * Since programmatic transcoding of messages using the corelib API can
 * generate a lot of program code under certain circumstances,
 * this generic transcoder helps to keep the footprint small.
 * Instead of API calls, a constant message description is used to
 * serialize and deserialize messages.
 * For large messages, these descriptions are smaller than the code
 * required for API calls. Even in projects with many messages,
 * program code can be saved by reusing the transcoder multiple times.
 * For small embedded projects with few small messages, however,
 * it may make sense to use the corelib without this transcoder.
 *
 * Typically, this module is used from generated code that defines
 * the message structures and their descriptors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_OBJECT_H
#define SOFAB_OBJECT_H

/**
 * @defgroup c_api C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SOFAB_OBJECT_C
# define SOFAB_OBJECT_EXTERN extern
#else
# define SOFAB_OBJECT_EXTERN
#endif

/* includes *******************************************************************/
#include <stddef.h>

#include "sofab/istream.h"
#include "sofab/ostream.h"

/* constants ******************************************************************/
/*!
 * @name Object field types
 * @brief Field type tags stored in @ref sofab_object_descr_field_t::type.
 *
 * Each descriptor field carries one of these tags; it selects which
 * encode/decode path the transcoder takes for that struct member. Pass the tag
 * as the @c type argument of the @ref SOFAB_OBJECT_FIELD family of macros.
 * @{
 */
#define SOFAB_OBJECT_FIELDTYPE_UNSIGNED 		0x0 /*!< Unsigned integer (uint8/16/32/64). */
#define SOFAB_OBJECT_FIELDTYPE_SIGNED   		0x1 /*!< Signed integer (int8/16/32/64). */
#define SOFAB_OBJECT_FIELDTYPE_FP32     		0x2 /*!< 32-bit floating point (float). */
#define SOFAB_OBJECT_FIELDTYPE_FP64     		0x3 /*!< 64-bit floating point (double). */
#define SOFAB_OBJECT_FIELDTYPE_STRING   		0x4 /*!< Null-terminated string. */
#define SOFAB_OBJECT_FIELDTYPE_BLOB     		0x5 /*!< Raw binary blob. Fixed full-capacity by default; a variable used-length variant is built with @ref SOFAB_OBJECT_FIELD_BLOB_SIZED (flagged via a non-zero @c nested_idx). */
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED	0x6 /*!< Array of unsigned integers. Fixed full-capacity by default; a length-carrying variant is built with @ref SOFAB_OBJECT_FIELD_ARRAY_SIZED (flagged via a non-zero @c nested_idx). */
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED   	0x7 /*!< Array of signed integers (sized variant as above). */
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32     	0x8 /*!< Array of 32-bit floats (sized variant as above). */
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64     	0x9 /*!< Array of 64-bit doubles (sized variant as above). */
#define SOFAB_OBJECT_FIELDTYPE_SEQUENCE       	0xA /*!< Nested object (encoded as a sequence). */
/*! @} */

/* macros *********************************************************************/
/*!
 * @brief Build a scalar field descriptor (@ref sofab_object_descr_field_t).
 *
 * Derives the field offset and size from the struct type, so the descriptor
 * stays in sync with the C declaration. The element size is taken as the size
 * of the whole field (scalars are a single element).
 *
 * @param id     Field ID on the wire.
 * @param obj    Enclosing struct type (e.g. @c struct my_msg).
 * @param field  Member name within @p obj.
 * @param type   Field type tag (one of the scalar @ref SOFAB_OBJECT_FIELDTYPE_UNSIGNED "SOFAB_OBJECT_FIELDTYPE_*").
 */
#define SOFAB_OBJECT_FIELD(id, obj, field, type) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), 0, type, (sizeof(((obj *)0)->field) & 0xF) }

/*!
 * @brief Build a variable-length blob field descriptor (reuses the BLOB type).
 *
 * A sized blob pairs a fixed-capacity buffer @p dfield (@c sizeof(dfield) bytes)
 * with a companion length member @p lfield holding how many bytes are actually
 * used (@c 0..sizeof(dfield)). On encode only @p lfield bytes reach the wire; on
 * decode the received length is stored back into @p lfield. The buffer capacity
 * never reaches the wire. This is the C counterpart of C++ @c sofab::FixedBytes,
 * and it produces byte-identical wire to a plain blob of the same actual length.
 *
 * @warning @p lfield @b must immediately precede @p dfield in @p obj, i.e.
 * @c offsetof(obj,dfield) @c == @c offsetof(obj,lfield)+sizeof(lfield); declare
 * them adjacently as @c { uintX lfield; uint8_t dfield[N]; }. Placing the length
 * @b before the buffer is alignment-robust: a byte buffer (alignment 1) always
 * abuts the length with no padding, for any @p lfield width and any @c N (a
 * length placed after the buffer could be padded away from @c offset+size). The
 * companion width @c sizeof(lfield) (one of 1/2/4/8) is stored in the
 * descriptor's @c nested_idx slot, which also flags the blob as sized: a plain
 * @ref SOFAB_OBJECT_FIELD blob keeps @c nested_idx @c == @c 0 and its original
 * fixed full-capacity behaviour.
 *
 * @param id      Field ID on the wire.
 * @param obj     Enclosing struct type.
 * @param dfield  Blob buffer member within @p obj (its size is the capacity).
 * @param lfield  Used-length member, declared immediately before @p dfield.
 */
#define SOFAB_OBJECT_FIELD_BLOB_SIZED(id, obj, dfield, lfield) \
    { id, offsetof(obj, dfield), sizeof(((obj *)0)->dfield), \
      (uint8_t)(sizeof(((obj *)0)->lfield) \
                + SOFAB_OBJECT_ASSERT_LEN_ADJACENT(obj, dfield, lfield)), \
      SOFAB_OBJECT_FIELDTYPE_BLOB, \
      (sizeof(((obj *)0)->dfield) & 0xF) }

/*!
 * @brief Compile-time check that @p lfield immediately precedes @p dfield.
 *
 * Evaluates to 0, or fails to compile (negative array bound) when the two
 * members are not adjacent — i.e. when the compiler inserted padding between
 * them. Used by the @c *_SIZED field macros, whose descriptor records only the
 * length's @e width and locates it at @c offset @c - @c width.
 */
#define SOFAB_OBJECT_ASSERT_LEN_ADJACENT(obj, dfield, lfield) \
    (0 * sizeof(char[(offsetof(obj, dfield) \
                      == offsetof(obj, lfield) + sizeof(((obj *)0)->lfield)) ? 1 : -1]))

/*!
 * @brief Build a nested-object (sequence) field descriptor.
 *
 * Like @ref SOFAB_OBJECT_FIELD but for a nested struct serialized as a
 * sequence. @p idx selects the child descriptor from the enclosing object's
 * @c nested_list.
 *
 * @param id     Field ID on the wire.
 * @param obj    Enclosing struct type.
 * @param field  Nested struct member within @p obj.
 * @param type   Field type tag (typically @ref SOFAB_OBJECT_FIELDTYPE_SEQUENCE).
 * @param idx    Index of the nested descriptor in the @c nested_list.
 */
#define SOFAB_OBJECT_FIELD_SEQUENCE(id, obj, field, type, idx) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), idx, type, (sizeof(((obj *)0)->field) & 0xF) }

/*!
 * @brief Build an array field descriptor.
 *
 * Like @ref SOFAB_OBJECT_FIELD but the element size is taken from a single
 * array element (@c field[0]); the element count is derived at run time from
 * the total field size divided by the element size.
 *
 * @param id     Field ID on the wire.
 * @param obj    Enclosing struct type.
 * @param field  Array member within @p obj.
 * @param type   Field type tag (one of the array @ref SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED "SOFAB_OBJECT_FIELDTYPE_ARRAY_*").
 */
#define SOFAB_OBJECT_FIELD_ARRAY(id, obj, field, type) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), 0, type, (sizeof(((obj *)0)->field[0]) & 0xF) }

/*!
 * @brief Build a length-carrying (sized) array field descriptor.
 *
 * A plain @ref SOFAB_OBJECT_FIELD_ARRAY has nowhere to keep a length: it derives
 * the element count from @c sizeof(field)/sizeof(field[0]), which is the array's
 * @b capacity. MESSAGE_SPEC §3 makes the schema @c count a capacity too and the
 * wire count @c M the array's @b length, with no element elided — so a capacity-
 * only object can never hold an array shorter than @c N, and a decode of
 * @c M @c < @c N followed by a re-encode silently lengthens the value back to
 * @c N. A sized array fixes that the same way @ref SOFAB_OBJECT_FIELD_BLOB_SIZED
 * fixes it for a blob: it pairs the fixed-capacity array @p dfield with a
 * companion length member @p lfield holding how many elements are actually used
 * (@c 0..N). On encode exactly @p lfield elements reach the wire — a trailing
 * element equal to the element default included, because @c M is the length; on
 * decode the received count is stored back into @p lfield. The capacity never
 * reaches the wire.
 *
 * @warning @p lfield @b must immediately precede @p dfield in @p obj, and the
 * two must be @b adjacent — @c offsetof(obj,dfield) @c == @c
 * offsetof(obj,lfield)+sizeof(lfield). For a sized @e blob that is automatic: a
 * byte buffer has alignment 1, so it abuts any length width. It is @b not
 * automatic here. An element wider than a byte carries a stricter alignment, so a
 * @e narrower length in front of it is padded away — @c { @c uint8_t @c len; @c
 * uint32_t @c vals[4]; @c } places @c vals at offset 4, three bytes past the
 * length, and the descriptor (which stores only the length's @e width and reads
 * it at @c offset @c - @c width) would address the padding instead. This macro
 * therefore does not assume the invariant, it @b asserts it: the adjacency is
 * checked at compile time (@ref SOFAB_OBJECT_ASSERT_LEN_ADJACENT), so a padded
 * pair is a build error rather than a silent misread. Declare the length at least
 * as wide as one element — @c { @c uint32_t @c len; @c uint32_t @c vals[4]; @c }
 * — or otherwise arrange the two members to leave no gap.
 *
 * The companion width @c sizeof(lfield) (one of 1/2/4/8) is stored in the
 * descriptor's @c nested_idx slot, which also flags the array as sized: a plain
 * @ref SOFAB_OBJECT_FIELD_ARRAY keeps @c nested_idx @c == @c 0 and encodes its
 * full capacity.
 *
 * @param id      Field ID on the wire.
 * @param obj     Enclosing struct type.
 * @param dfield  Array member within @p obj (its size is the capacity).
 * @param lfield  Used-length member (in elements), declared immediately before
 *                @p dfield.
 * @param type    Field type tag (one of the array @ref SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED "SOFAB_OBJECT_FIELDTYPE_ARRAY_*").
 */
#define SOFAB_OBJECT_FIELD_ARRAY_SIZED(id, obj, dfield, lfield, type) \
    { id, offsetof(obj, dfield), sizeof(((obj *)0)->dfield), \
      (uint8_t)(sizeof(((obj *)0)->lfield) \
                + SOFAB_OBJECT_ASSERT_LEN_ADJACENT(obj, dfield, lfield)), \
      type, (sizeof(((obj *)0)->dfield[0]) & 0xF) }

/*!
 * @brief Build an object descriptor (@ref sofab_object_descr_t) without defaults.
 *
 * Fields whose value is all-zero are omitted from the encoding (see
 * @ref sofab_object_encode).
 *
 * @param field_list    Array of @ref sofab_object_descr_field_t for this object.
 * @param field_count   Number of entries in @p field_list.
 * @param nested_list   Array of pointers to nested @ref sofab_object_descr_t (may be NULL).
 * @param nested_count  Number of entries in @p nested_list.
 */
#define SOFAB_OBJECT_DESCR(field_list, field_count, nested_list, nested_count) \
    { (field_list), (nested_list), NULL, (field_count), (nested_count), 0 }

/*!
 * @brief Build an object descriptor with a default-values reference.
 *
 * As @ref SOFAB_OBJECT_DESCR, but fields equal to the corresponding member in
 * @p default_struct are omitted from the encoding (instead of comparing against
 * zero), and @ref sofab_object_init seeds objects from it.
 *
 * @param field_list     Array of @ref sofab_object_descr_field_t for this object.
 * @param field_count    Number of entries in @p field_list.
 * @param nested_list    Array of pointers to nested @ref sofab_object_descr_t (may be NULL).
 * @param nested_count   Number of entries in @p nested_list.
 * @param default_struct Pointer to a fully-populated object holding the field defaults.
 */
#define SOFAB_OBJECT_DESCR_WITH_DEFAULTS(field_list, field_count,nested_list, nested_count, default_struct) \
    { (field_list), (nested_list), (default_struct), (field_count), (nested_count), 0 }

/*!
 * @name Wrapper-array holder marker (@ref sofab_object_descr_t::fixed_seq)
 *
 * The @c fixed_seq slot packs two things: bit 0 flags the descriptor as a
 * wrapper-array holder, and the remaining bits carry the byte width of the
 * holder's companion element-count member (0 when it has none). It is the
 * holder-level counterpart of the way @c nested_idx doubles as the "is sized"
 * flag and the length width for @ref SOFAB_OBJECT_FIELD_BLOB_SIZED /
 * @ref SOFAB_OBJECT_FIELD_ARRAY_SIZED — with the flag kept in bit 0 so that every
 * plain "is this a holder?" test stays a truth test.
 * @{
 */
#define SOFAB_OBJECT_SEQ_HOLDER    0x01u /*!< Bit 0: this descriptor is a wrapper-array holder. */
#define SOFAB_OBJECT_SEQ_LEN_SHIFT 1     /*!< Shift of the length member's byte width (0 = unsized). */
/*! @} */

/*!
 * @brief Build a fixed-capacity sequence-holder object descriptor.
 *
 * Like @ref SOFAB_OBJECT_DESCR, but marks the descriptor as a wrapper-array
 * holder: one whose fields are exactly the element slots
 * @c 0 … @p field_count - 1 of a bounded @c string / @c blob / @c struct /
 * @c union array (which lowers to a wrapper sequence). For such a descriptor a
 * wire element id outside that range is an over-index element and is rejected as
 * @ref SOFAB_RET_E_INVALID_MSG on decode (MESSAGE_SPEC §7/§7.1), rather than
 * silently skipped the way a message ignores an unknown forward-compatible id.
 *
 * The flag also selects the **positional** sparse rule of MESSAGE_SPEC §2/§5.1
 * on encode: inside a holder an element at an @e interior index equal to its
 * default is omitted (leaving an id gap) whatever its kind, and the element at
 * the @e last index is always written, because a wrapper carries no length and
 * *highest present id + 1* is what recovers it.
 *
 * This macro builds the **un-sized** holder: it has no length member, so its
 * @p field_count slots are all materialized, the last index is
 * @c field_count @c - @c 1 unconditionally, and the only two lengths it can
 * express are @c 0 (every slot default — the enclosing object omits the whole
 * field) and @c field_count. Use @ref SOFAB_OBJECT_DESCR_SEQ_SIZED to express
 * @c 0 … @c N like every other target.
 *
 * @param field_list    Array of @ref sofab_object_descr_field_t (the element slots).
 * @param field_count   Number of element slots (the array capacity N).
 * @param nested_list   Array of pointers to nested @ref sofab_object_descr_t (may be NULL).
 * @param nested_count  Number of entries in @p nested_list.
 */
#define SOFAB_OBJECT_DESCR_SEQ(field_list, field_count, nested_list, nested_count) \
    { (field_list), (nested_list), NULL, (field_count), (nested_count), \
      SOFAB_OBJECT_SEQ_HOLDER }

/*!
 * @brief Build a length-carrying (sized) sequence-holder object descriptor.
 *
 * The wrapper-array counterpart of @ref SOFAB_OBJECT_FIELD_ARRAY_SIZED, and it
 * closes the same gap one level up. An un-sized holder
 * (@ref SOFAB_OBJECT_DESCR_SEQ) materializes all @p field_count element slots and
 * carries no length, so it can express only two lengths — @c 0 and @c N — while
 * MESSAGE_SPEC §5.1 gives a wrapper array the length *highest present id + 1*,
 * i.e. any of @c 0 … @c N. A sized holder adds the missing member: @p lfield holds
 * how many elements are actually used, exactly as a sized blob's companion holds
 * how many bytes are.
 *
 * What it buys, per MESSAGE_SPEC §2/§5.1:
 * - **encode** walks the slots @c [0, @p lfield) only — the last index is
 *   @c lfield @c - @c 1, not @c field_count @c - @c 1. An interior slot equal to
 *   its element default is omitted (id gap) whatever its kind; the slot at
 *   @c lfield @c - @c 1 is always written (a leaf as its value, a sequence element
 *   as an empty frame), because that is what carries the length. @c lfield @c ==
 *   @c 0 is the empty array and the enclosing object omits the field entirely.
 * - **decode** stores the received length — *highest present element id + 1* —
 *   back into @p lfield, so a received @c [{k:1}] re-encodes as one element
 *   instead of silently growing back to @c N. "Present" is an element that was
 *   actually bound: one skipped because its wire type contradicts the declared
 *   element type is skipped like an unknown id (§7.3) and counts for nothing, an
 *   empty one (empty frame, empty string) is present and counts.
 *
 * @warning @p lfield @b must immediately precede the @b first element slot
 * @p efield (the member @c field_list[0] describes) and the two must be
 * @b adjacent — @c offsetof(obj,efield) @c == @c
 * offsetof(obj,lfield)+sizeof(lfield). The descriptor stores only the length's
 * @e width and reads it at <em>first slot offset − width</em>, so a padding gap
 * would address the padding instead. The "a length placed immediately before the
 * buffer is never padded" argument holds only for a @b byte-aligned buffer (a
 * sized blob): an element slot is generally wider and more strictly aligned, so a
 * narrower length in front of it is padded away. This macro therefore does not
 * assume the invariant, it @b asserts it at compile time
 * (@ref SOFAB_OBJECT_ASSERT_LEN_ADJACENT) — a padded pair is a build error rather
 * than a silent misread. Declare the length at least as wide as the slot's
 * alignment, e.g. @c { @c uint32_t @c len; @c struct @c kv @c e[5]; @c }.
 *
 * The width @c sizeof(lfield) (one of 1/2/4/8) is stored in the descriptor's
 * @c fixed_seq slot above the holder flag (@ref SOFAB_OBJECT_SEQ_LEN_SHIFT); a
 * plain @ref SOFAB_OBJECT_DESCR_SEQ keeps that width @c 0 and its original
 * full-capacity behaviour. Like @ref SOFAB_OBJECT_DESCR_SEQ this descriptor
 * carries no default image, so the holder's declared default is the @b empty
 * array: @ref sofab_object_init clears @p lfield to 0, and the enclosing
 * ≠-default test reads "is default" as @c lfield @c == @c 0 (never a scan of the
 * slots, whose content past the used length is indeterminate — the same rule a
 * sized blob and a sized array follow).
 *
 * @param field_list    Array of @ref sofab_object_descr_field_t (the element slots).
 * @param field_count   Number of element slots (the array capacity N).
 * @param nested_list   Array of pointers to nested @ref sofab_object_descr_t (may be NULL).
 * @param nested_count  Number of entries in @p nested_list.
 * @param obj           The holder struct type.
 * @param efield        First element-slot member (the one @c field_list[0] describes).
 * @param lfield        Used-length member (in elements), declared immediately
 *                      before @p efield.
 */
#define SOFAB_OBJECT_DESCR_SEQ_SIZED(field_list, field_count, nested_list, nested_count, obj, efield, lfield) \
    { (field_list), (nested_list), NULL, (field_count), (nested_count), \
      (uint8_t)(SOFAB_OBJECT_SEQ_HOLDER \
                | (sizeof(((obj *)0)->lfield) << SOFAB_OBJECT_SEQ_LEN_SHIFT) \
                | SOFAB_OBJECT_ASSERT_LEN_ADJACENT(obj, efield, lfield)) }

/* types **********************************************************************/
/*!
 * @brief Description of a single field within a SofaBuffer object.
 */
typedef struct
{
    const sofab_object_descr_id_t id;		/*!< Field ID (width per SOFAB_OBJECT_DESCR_PROFILE) */
    const sofab_object_descr_offset_t offset;	/*!< Offset within the object structure (width per profile) */
    const sofab_object_descr_size_t size;		/*!< Size of the field in bytes (width per profile) */
    const uint8_t nested_idx;		/*!< SEQUENCE: index into the nested object descriptor list. BLOB/ARRAY: byte width of the companion length member (0 = not sized), see @ref SOFAB_OBJECT_FIELD_BLOB_SIZED / @ref SOFAB_OBJECT_FIELD_ARRAY_SIZED */
    const uint8_t type : 4;			/*!< Field type (4bit for types: 0x0..0xA) */
    const uint8_t element_size : 4;	/*!< Size of individual elements for arrays (4bit for type length: 1..8)*/
} sofab_object_descr_field_t;

/*!
 * @brief Description of a SofaBuffer object structure.
 */
typedef struct sofab_object_descr
{
    const sofab_object_descr_field_t *const field_list;     /*!< Pointer to list of field descriptors */
    const struct sofab_object_descr *const *nested_list;    /*!< Pointer to list of nested object descriptors */
    const void *const default_values;                       /*!< Pointer to default values for fields (optional, may be NULL) */
    const uint16_t field_count;                             /*!< Number of fields in the object */
    const uint8_t nested_count;                             /*!< Number of nested objects */
    const uint8_t fixed_seq;                                /*!< Wrapper-array holder marker: bit 0 (@ref SOFAB_OBJECT_SEQ_HOLDER) flags the holder — reject an unmatched (over-index) element id instead of skipping it; the bits above @ref SOFAB_OBJECT_SEQ_LEN_SHIFT carry the byte width of the companion element-count member (0 = un-sized, see @ref SOFAB_OBJECT_DESCR_SEQ_SIZED) */
} sofab_object_descr_t;

/*!
 * @brief Decoder state for a SofaBuffer object.
 */
typedef struct
{
    const sofab_object_descr_t *info;      /*!< Pointer to object descriptor */
    uint8_t *dst;                          /*!< Destination buffer for decoded data */
    sofab_istream_decoder_t decoder;       /*!< Decoder state */
    uint8_t depth;                         /*!< Decoder depth */
} sofab_object_decoder_t;

/* prototypes *****************************************************************/

/*!
 * @brief Initializes an object structure with default values.
 *
 * This function populates the provided object structure with default values
 * as specified in the object descriptor. If no default values are provided,
 * the object is zero-initialized.
 *
 * @param info      Pointer to the object descriptor.
 * @param obj       Pointer to the object structure to initialize.
 * @return          SOFAB_RET_OK on success, or an error code on failure.
 */
extern sofab_ret_t sofab_object_init (
    const sofab_object_descr_t *info,
    void *obj);

/*!
 * @brief Encodes an object with the given descriptor into the output stream.
 *
 * The output stream context must be initialized prior to calling this function.
 * Fields equal to their default (the matching member of the descriptor's
 * default-values object, or zero when none is set) are skipped. Nested objects
 * are written as sequences.
 *
 * The skip rule covers a nested-object **field** too (MESSAGE_SPEC §2): one whose
 * every child equals its default is omitted entirely rather than framed as an
 * empty sequence, so an all-default object encodes to zero bytes. The comparison
 * is per child field, recursively, against the nested descriptor's declared
 * defaults — never a raw byte image — so struct padding cannot influence it and a
 * non-zero nested default is handled like any other. Absence reconstructs exactly
 * that default (@ref sofab_object_init), so the omission is value-preserving.
 *
 * Inside a **wrapper-array holder** (@ref SOFAB_OBJECT_DESCR_SEQ) the fields are
 * the array's element slots and the rule becomes **positional** (MESSAGE_SPEC
 * §2/§5.1): a wrapper carries no length, so the decoded length is *highest present
 * id + 1* and nothing that carries it may be elided.
 * - An element at an **interior** index equal to its default is **omitted**,
 *   leaving an id gap — a leaf is not written and a sequence-form element is not
 *   framed either. Both kinds obey one rule; there is no trailing-run elision and
 *   no fill-to-N.
 * - The element at the **last** index is **always written**: a leaf as its
 *   (default) value, a sequence element as an empty frame.
 *
 * **What "last index" means here.** It is the array's `length - 1`, and where the
 * length comes from is the holder descriptor's business:
 * - @ref SOFAB_OBJECT_DESCR_SEQ_SIZED carries an element-count member, so the
 *   length is that member (clamped to the capacity). Slots at or past it are not
 *   walked at all; the slot at `length - 1` is the one that is always written; a
 *   length of 0 writes nothing, and the field-level ≠-default test above omits the
 *   whole wrapper (the canonical encoding of the empty array, §2). All of
 *   @c 0 … @c N are expressible, as on every other target.
 * - a plain @ref SOFAB_OBJECT_DESCR_SEQ has no length member, so the value it
 *   holds occupies every slot and the length is @c field_count: the last index is
 *   @c field_count @c - @c 1 unconditionally. An all-default holder is the one
 *   exception — indistinguishable from the empty array there — and is omitted
 *   whole by the same field-level test. Lengths @c 1 … @c N-1 are not
 *   representable in that form.
 *
 * A compact scalar array encodes **every** element it holds (§3): its element
 * count is the length member of @ref SOFAB_OBJECT_FIELD_ARRAY_SIZED, or the full
 * capacity for a plain @ref SOFAB_OBJECT_FIELD_ARRAY. A trailing element equal to
 * the element default is *not* dropped — `[1,2,3,0,0]` and `[1,2,3]` are different
 * values.
 *
 * This descriptor-driven encoder uses none of the output stream's hold-back
 * framing (@c sofab_ostream_write_sequence_begin_lazy): it tests a field against
 * its default *before* opening anything, so it is canonical at every nesting
 * depth and no @c SOFAB_LAZY_SEQ_DEPTH window applies to it.
 *
 * @param ctx       Pointer to the output stream context.
 * @param info      Pointer to the object descriptor.
 * @param src       Pointer to the source object to serialize.
 *
 * @return SOFAB_RET_OK on success, otherwise an sofab_ret_t error code
 *         (e.g. SOFAB_RET_E_ARGUMENT for an unsupported descriptor field type,
 *         or a write error propagated from the output stream).
 */
extern sofab_ret_t sofab_object_encode (
    sofab_ostream_t *ctx,
    const sofab_object_descr_t *info,
    const void *src);

 /*!
 * @brief Field callback invoked during object decoding.
 *
 * Use this function as the field callback when initializing an input stream
 * for decoding objects with @ref sofab_istream_init. The @p usrptr must point
 * to a @ref sofab_object_decoder_t whose @c info and @c dst describe the target
 * object; for nested objects the decoder must be the first element of an array
 * with one slot per supported nesting level (see @c depth). The callback binds
 * the appropriate read for each known field ID. An unknown field ID is ignored
 * (skipped) for a normal message descriptor, but for a fixed-capacity
 * sequence-holder descriptor (see @ref SOFAB_OBJECT_DESCR_SEQ) it is an
 * over-index element and rejects the message via @ref sofab_istream_invalidate.
 *
 * Inside a **sized** holder (@ref SOFAB_OBJECT_DESCR_SEQ_SIZED) a matched element
 * id also raises the companion element-count member to @c id @c + @c 1, so that
 * when the wrapper closes it holds *highest present id + 1* — the array's length
 * per MESSAGE_SPEC §5.1, stored back exactly as a sized blob stores its received
 * byte length. The §7.4 reset below re-zeroes the member whenever the wrapper
 * re-opens, so a replaced array reports its own length and not the previous one's.
 *
 * **Only a bound element counts.** An element whose header wire type contradicts
 * the declared one is skipped "exactly as a field with an unknown id is skipped"
 * (§7.3) — and an unknown id leaves nothing behind, so it does **not** occupy its
 * id and does **not** count toward the length: the slot is reconstructed from the
 * element default and the array is byte-for-byte what it would have been had the
 * element never arrived. The ids §5.1 counts are the ones consumed as elements,
 * not the ones that merely appeared on the wire. A well-typed but **empty**
 * element — an empty frame, an empty string — is bound and does count; that is a
 * present element and not the same thing at all.
 *
 * @warning **Initialize @c dst with @ref sofab_object_init before every message.**
 * Decoding writes only the fields the wire actually carries; a field the sender
 * omitted because it equals its default is not written, so whatever the
 * destination already held stays. Re-using one object for a second message
 * therefore keeps stale values unless it is re-initialized first. This applies to
 * a @b sequence field as much as to a leaf one: MESSAGE_SPEC §2 omits an
 * all-default sequence field outright, and the §7.4 wrapper-replace reset inside
 * this callback only runs when a wrapper actually opens on the wire.
 *
 * @param ctx     Pointer to the active input stream context.
 * @param id      Field ID reported by the decoder.
 * @param size    Size of the field value in bytes (unused; kept for the callback signature).
 * @param count   Number of array elements (unused; kept for the callback signature).
 * @param usrptr  Pointer to the @ref sofab_object_decoder_t driving this object.
 */
extern void sofab_object_field_cb (
    sofab_istream_t *ctx,
    sofab_id_t id,
    size_t size,
    size_t count,
    void *usrptr);

#ifdef __cplusplus
}
#endif

/** @} */ // end of defgroup

#endif /* SOFAB_OBJECT_H */
