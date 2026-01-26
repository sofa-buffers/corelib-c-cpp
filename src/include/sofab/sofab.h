/*!
 * @file sofab.h
 * @brief SofaBuffers C - Type definitions and constants.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _SOFAB_H
#define _SOFAB_H

/**
 * @defgroup c_api C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SOFAB_C
# define SOFAB_EXTERN extern
#else
# define SOFAB_EXTERN
#endif

/* includes *******************************************************************/
#include <stdint.h>
#include <stdbool.h>

/* constants ******************************************************************/
/*! @brief SofaBuffers C API version */
#define SOFAB_API_VERSION 1

/* macros *********************************************************************/

/* types **********************************************************************/
/*! @brief Return status codes */
typedef enum
{
    SOFAB_RET_OK,                //!< OK
    SOFAB_RET_E_ARGUMENT,        //!< Invalid argument
    SOFAB_RET_E_USAGE,           //!< Invalid usage
    SOFAB_RET_E_BUFFER_FULL,     //!< Sofab serialization failed due to buffer overflow
    SOFAB_RET_E_INVALID_MSG,     //!< Sofab deserialization failed due to invalid message
} sofab_ret_t;

/*! @brief SofaBuffers 3bit field data types */
typedef enum
{
    SOFAB_TYPE_VARINT_UNSIGNED       = 0x00,
    SOFAB_TYPE_VARINT_SIGNED         = 0x01,
    SOFAB_TYPE_FIXLEN                = 0x02,
    SOFAB_TYPE_VARINTARRAY_UNSIGNED  = 0x03,
    SOFAB_TYPE_VARINTARRAY_SIGNED    = 0x04,
    SOFAB_TYPE_FIXLENARRAY           = 0x05,
    SOFAB_TYPE_SEQUENCE_START        = 0x06,
    SOFAB_TYPE_SEQUENCE_END          = 0x07,
} sofab_type_t;

/*! @brief SofaBuffers 3bit fixed-length data types */
typedef enum
{
    SOFAB_FIXLENTYPE_FP32            = 0x00,
    SOFAB_FIXLENTYPE_FP64            = 0x01,
    SOFAB_FIXLENTYPE_STRING          = 0x02,
    SOFAB_FIXLENTYPE_BLOB            = 0x03,
} sofab_fixlentype_t;

/*! @brief Field identifier type */
typedef uint32_t sofab_id_t;
#define SOFAB_ID_MAX (INT32_MAX)

/*! @brief Unsigned value type */
typedef uint64_t sofab_unsigned_t;
#define SOFAB_UNSIGNED_MAX (UINT64_MAX)

/*! @brief Signed value type */
typedef int64_t sofab_signed_t;
#define SOFAB_SIGNED_MAX (INT64_MAX)
#define SOFAB_SIGNED_MIN (INT64_MIN)

/*! @brief Maximum fixed-length field size in bytes */
#if SIZE_MAX == 0xFFFF
# define SOFAB_FIXLEN_MAX (INT16_MAX)
#else
# define SOFAB_FIXLEN_MAX (INT32_MAX)
#endif

/*! @brief Maximum number of elements in an array */
#if SIZE_MAX == 0xFFFF
# define SOFAB_ARRAY_MAX (INT16_MAX)
#else
# define SOFAB_ARRAY_MAX (INT32_MAX)
#endif

#if defined(__SIZEOF_DOUBLE__) && __SIZEOF_DOUBLE__ != 8
# define SOFAB_DISABLE_FP64_SUPPORT
#endif

/* configuration **************************************************************/
/*! @brief Disable support for 64-bit floating point types (if the check above fails). */
// #define SOFAB_DISABLE_FP64_SUPPORT

/*! @brief Disable support for fixed-length fields (floating point types, strings, and blobs). */
// #define SOFAB_DISABLE_FIXLEN_SUPPORT

/*! @brief Disable support for array fields. */
// #define SOFAB_DISABLE_ARRAY_SUPPORT

/*! @brief Disable support for sequence fields. */
// #define SOFAB_DISABLE_SEQUENCE_SUPPORT

/*! @brief Disable integer overflow checks when reading integer values. */
// #define SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK

/* exported vars **************************************************************/
// SOFAB_EXTERN type sofab_<varname>;

/* prototypes *****************************************************************/

#ifdef __cplusplus
}
#endif

/** @} */ // end of defgroup

#endif /* _SOFAB_H */
