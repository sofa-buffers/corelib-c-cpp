/*!
 * @file object.h
 * @brief SofaBuffers C - Object encoder and decoder.
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

/* macros *********************************************************************/
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

#define SOFAB_OBJECT_FIELD(id, obj, field, type) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), type, (sizeof(((obj *)0)->field) & 0xF) }

#define SOFAB_OBJECT_FIELD_ARRAY(id, obj, field, type) \
    { id, offsetof(obj, field), sizeof(((obj *)0)->field), type, (sizeof(((obj *)0)->field[0]) & 0xF) }

#define SOFAB_OBJECT_FIELD_DESCR(field_list, field_count, nested_list, nested_count) \
    { field_list, nested_list, field_count, nested_count }

/* types **********************************************************************/
typedef struct
{
	uint16_t id;
	uint16_t offset;
	uint16_t size;
	uint8_t type;
	uint8_t element_size;
} sofab_object_descr_field_t;

typedef struct sofab_object_descr
{
	const sofab_object_descr_field_t *const field_list;
	const struct sofab_object_descr *const *nested_list;
	uint16_t field_count;
	uint16_t nested_count;
} sofab_object_descr_t;

typedef struct
{
	const sofab_object_descr_t *info;
	uint8_t info_index;
	uint8_t *dst;
	sofab_decoder_t decoder;
} sofab_object_decoder_t;

// typedef struct sofab_object_ctx
// {
// 	sofab_object_decoder_t *decoder_list;
// 	uint8_t decoder_list_size;
// } sofab_object_ctx_t;

/* prototypes *****************************************************************/

extern sofab_ret_t sofab_object_encode (
	sofab_ostream_t *ctx,
	const sofab_object_descr_t *info,
	const void *src);

extern void sofab_object_field_cb (
	sofab_istream_t *ctx,
	sofab_id_t id,
	size_t size,
	size_t count,
	void *usrptr);

// extern sofab_ret_t sofab_object_decode (
// 	sofab_istream_t *ctx,
// 	const sofab_object_descr_t *info,
// 	void *dst);

#ifdef __cplusplus
}
#endif

/** @} */ // end of defgroup

#endif /* _SOFAB_OBJECT_H */
