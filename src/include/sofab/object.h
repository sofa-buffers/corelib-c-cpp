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

#ifndef _SOFAB_OBJECT_H
#define _SOFAB_OBJECT_H

/**
 * @defgroup c_api C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SOFAB_OBJECT_C
# define SOFAB_OBJECT_EXTERN extern
#else
# define SOFAB_OBJECT_EXTERN
#endif

/* includes *******************************************************************/
#include <stddef.h>

#include "sofab/istream.h"
#include "sofab/ostream.h"

/* constants ******************************************************************/
#define SOFAB_OBJECT_FIELDTYPE_UNSIGNED 		0x0
#define SOFAB_OBJECT_FIELDTYPE_SIGNED   		0x1
#define SOFAB_OBJECT_FIELDTYPE_FP32     		0x2
#define SOFAB_OBJECT_FIELDTYPE_FP64     		0x3
#define SOFAB_OBJECT_FIELDTYPE_STRING   		0x4
#define SOFAB_OBJECT_FIELDTYPE_BLOB     		0x5
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED	0x6
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED   	0x7
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32     	0x8
#define SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64     	0x9
#define SOFAB_OBJECT_FIELDTYPE_SEQUENCE       	0xA

/* macros *********************************************************************/
#define SOFAB_OBJECT_FIELD(id, obj, field, type) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), 0, type, (sizeof(((obj *)0)->field) & 0xF) }

#define SOFAB_OBJECT_FIELD_SEQUENCE(id, obj, field, type, idx) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), idx, type, (sizeof(((obj *)0)->field) & 0xF) }

#define SOFAB_OBJECT_FIELD_ARRAY(id, obj, field, type) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), 0, type, (sizeof(((obj *)0)->field[0]) & 0xF) }

#define SOFAB_OBJECT_DESCR(field_list, field_count, nested_list, nested_count) \
    { (field_list), (nested_list), NULL, (field_count), (nested_count) }

#define SOFAB_OBJECT_DESCR_WITH_DEFAULTS(field_list, field_count,nested_list, nested_count, default_struct) \
    { (field_list), (nested_list), (default_struct), (field_count), (nested_count) }

/* types **********************************************************************/
/*!
 * @brief Description of a single field within a SofaBuffer object.
 */
typedef struct
{
    const uint16_t id;				/*!< Field ID */
    const uint16_t offset;			/*!< Offset within the object structure */
    const uint16_t size;			/*!< Size of the field in bytes */
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
 *
 * @param ctx       Pointer to the output stream context.
 * @param info      Pointer to the object descriptor.
 * @param src       Pointer to the source object to serialize.
 */
extern sofab_ret_t sofab_object_encode (
    sofab_ostream_t *ctx,
    const sofab_object_descr_t *info,
    const void *src);

 /*!
 * @brief Field callback invoked during object decoding.
 *
 * Use this function as the field callback when initializing an input stream
 * for decoding objects with @ref sofab_istream_init.
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

#endif /* _SOFAB_OBJECT_H */
