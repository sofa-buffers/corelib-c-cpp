/*!
 * @file object.c
 * @brief SofaBuffers C - Object encoder and decoder.
 *
 * SPDX-License-Identifier: MIT
 */

#define _SOFAB_OBJECT_C

/* includes *******************************************************************/
#include "sofab/object.h"

#include <assert.h>

/* constants ******************************************************************/

/* macros *********************************************************************/

/* types **********************************************************************/

/* prototypes *****************************************************************/

/* static vars ****************************************************************/

/* functions ******************************************************************/

extern sofab_ret_t sofab_object_encode (
    sofab_ostream_t *ctx,
    const sofab_object_descr_t *info,
    const void *src)
{
    sofab_ret_t ret = SOFAB_RET_OK;
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    size_t nested_idx = 0;
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

    assert(ctx != NULL);
    assert(info != NULL);
    assert(src != NULL);

    for (size_t i = 0; i < info->field_count && ret == SOFAB_RET_OK; i++)
    {
        const sofab_object_descr_field_t *field = &info->field_list[i];

        switch (field->type)
        {
            case SOFAB_OBJECT_FIELDTYPE_UNSIGNED:
            {
                sofab_unsigned_t val;
                if (field->element_size == sizeof(uint8_t))
                    val = *((uint8_t *)((const uint8_t *)src + field->offset));
                else if (field->element_size == sizeof(uint16_t))
                    val = *((uint16_t *)((const uint8_t *)src + field->offset));
                else if (field->element_size == sizeof(uint32_t))
                    val = *((uint32_t *)((const uint8_t *)src + field->offset));
                else if (field->element_size == sizeof(uint64_t))
                    val = *((uint64_t *)((const uint8_t *)src + field->offset));
                else
                    return SOFAB_RET_E_USAGE; // Unsupported size

                ret = sofab_ostream_write_unsigned(ctx, field->id, val);
                break;
            }

            case SOFAB_OBJECT_FIELDTYPE_SIGNED:
            {
                sofab_signed_t sval;
                if (field->element_size == sizeof(int8_t))
                    sval = *((int8_t *)((const uint8_t *)src + field->offset));
                else if (field->element_size == sizeof(int16_t))
                    sval = *((int16_t *)((const uint8_t *)src + field->offset));
                else if (field->element_size == sizeof(int32_t))
                    sval = *((int32_t *)((const uint8_t *)src + field->offset));
                else if (field->element_size == sizeof(int64_t))
                    sval = *((int64_t *)((const uint8_t *)src + field->offset));
                else
                    return SOFAB_RET_E_USAGE; // Unsupported size

                ret = sofab_ostream_write_signed(ctx, field->id, sval);
                break;
            }

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP32:
                ret = sofab_ostream_write_fp32(ctx, field->id, *((float *)((const uint8_t *)src + field->offset)));
                break;

            case SOFAB_OBJECT_FIELDTYPE_FP64:
                ret = sofab_ostream_write_fp64(ctx, field->id, *((double *)((const uint8_t *)src + field->offset)));
                break;

            case SOFAB_OBJECT_FIELDTYPE_STRING:
                ret = sofab_ostream_write_string(ctx, field->id, (char *)((const uint8_t *)src + field->offset));
                break;

            case SOFAB_OBJECT_FIELDTYPE_BLOB:
                ret = sofab_ostream_write_blob(ctx, field->id, (uint8_t *)((const uint8_t *)src + field->offset), field->size);
                break;
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
            {
                size_t element_count = field->size / field->element_size;
                ret = sofab_ostream_write_array_of_unsigned(ctx, field->id,
                    (const void *)((const uint8_t *)src + field->offset), element_count, field->element_size);
                break;
            }

            case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
            {
                size_t element_count = field->size / field->element_size;
                ret = sofab_ostream_write_array_of_signed(ctx, field->id,
                    (const void *)((const uint8_t *)src + field->offset), element_count, field->element_size);
                break;
            }

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
            {
                size_t element_count = field->size / sizeof(float);
                ret = sofab_ostream_write_array_of_fp32(ctx, field->id,
                    (const float *)((const uint8_t *)src + field->offset), element_count);
                break;
            }

            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
            {
                size_t element_count = field->size / sizeof(double);
                ret = sofab_ostream_write_array_of_fp64(ctx, field->id,
                    (const double *)((const uint8_t *)src + field->offset), element_count);
                break;
            }
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */
#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_SEQUENCE:
                ret = sofab_ostream_write_sequence_begin(ctx, field->id);
                ret |= sofab_object_encode(ctx,
                    info->nested_list[nested_idx++],
                    (const uint8_t *)src + field->offset);
                ret |= sofab_ostream_write_sequence_end(ctx);
                break;
#endif /* !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT) */

            default:
                // Unsupported field type in descriptor
                return SOFAB_RET_E_USAGE;
        }
    }

    return ret;
}

extern void sofab_object_field_cb (sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    sofab_object_decoder_t *decoder = (sofab_object_decoder_t *)usrptr;
    const sofab_object_descr_t *info = decoder->info;

    (void)size;
    (void)count;

    for (size_t i = 0; i < info->field_count; i++)
    {
        const sofab_object_descr_field_t *field = &info->field_list[i];
        if (field->id != id)
        {
            continue;
        }

        switch (field->type)
        {
            case SOFAB_OBJECT_FIELDTYPE_UNSIGNED:
                sofab_istream_read_field(ctx, decoder->dst + field->offset, field->element_size,
                    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
                break;

            case SOFAB_OBJECT_FIELDTYPE_SIGNED:
                sofab_istream_read_field(ctx, decoder->dst + field->offset, field->element_size,
                    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED));
                break;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_FP32:
                sofab_istream_read_fp32(ctx, (float *)(decoder->dst + field->offset));
                break;

            case SOFAB_OBJECT_FIELDTYPE_FP64:
                sofab_istream_read_fp64(ctx, (double *)(decoder->dst + field->offset));
                break;

            case SOFAB_OBJECT_FIELDTYPE_STRING:
                sofab_istream_read_string(ctx, (char *)(decoder->dst + field->offset), field->size);
                break;

            case SOFAB_OBJECT_FIELDTYPE_BLOB:
                sofab_istream_read_blob(ctx, decoder->dst + field->offset, field->size);
                break;
#endif /* !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) */

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_UNSIGNED:
                sofab_istream_read_array(ctx,
                    decoder->dst + field->offset,
                    field->size / field->element_size, field->element_size,
                    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED));
                break;

            case SOFAB_OBJECT_FIELDTYPE_ARRAY_SIGNED:
                sofab_istream_read_array(ctx,
                    decoder->dst + field->offset,
                    field->size / field->element_size, field->element_size,
                    SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED));
                break;

#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP32:
                sofab_istream_read_array_of_fp32(ctx,
                    (float *)(decoder->dst + field->offset),
                    field->size / sizeof(float));
                break;

            case SOFAB_OBJECT_FIELDTYPE_ARRAY_FP64:
                sofab_istream_read_array_of_fp64(ctx,
                    (double *)(decoder->dst + field->offset),
                    field->size / sizeof(double));
                break;
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

        break;
    }
}
