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
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED	0x6 /*!< Array of unsigned integers. */
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED   	0x7 /*!< Array of signed integers. */
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32     	0x8 /*!< Array of 32-bit floats. */
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64     	0x9 /*!< Array of 64-bit doubles. */
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
      (uint8_t)sizeof(((obj *)0)->lfield), SOFAB_OBJECT_FIELDTYPE_BLOB, \
      (sizeof(((obj *)0)->dfield) & 0xF) }

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
    { (field_list), (nested_list), NULL, (field_count), (nested_count) }

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
    { (field_list), (nested_list), (default_struct), (field_count), (nested_count) }

/* types **********************************************************************/
/*!
 * @brief Description of a single field within a SofaBuffer object.
 */
typedef struct
{
    const sofab_object_descr_id_t id;		/*!< Field ID (width per SOFAB_OBJECT_DESCR_PROFILE) */
    const sofab_object_descr_offset_t offset;	/*!< Offset within the object structure (width per profile) */
    const sofab_object_descr_size_t size;		/*!< Size of the field in bytes (width per profile) */
    const uint8_t nested_idx;		/*!< Index into the nested object descriptor list */
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
 * @param ctx       Pointer to the output stream context.
 * @param info      Pointer to the object descriptor.
 * @param src       Pointer to the source object to serialize.
 *
 * @return SOFAB_RET_OK on success, otherwise an sofab_ret_t error code
 *         (e.g. SOFAB_RET_E_USAGE for an unsupported descriptor field type,
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
 * the appropriate read for each known field ID and ignores unknown fields.
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
