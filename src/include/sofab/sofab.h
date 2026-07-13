/*!
 * @file sofab.h
 * @brief SofaBuffers C - Type definitions and constants.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_H
#define SOFAB_H

/**
 * @defgroup c_api C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SOFAB_C
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
    SOFAB_RET_INCOMPLETE,        //!< Consumed bytes end inside a field or with an open
                                 //!< sequence: a valid but partial decode. NOT an error —
                                 //!< the caller owns end-of-input and may feed more bytes.
                                 //!< Distinct from SOFAB_RET_OK (a complete message
                                 //!< boundary) and SOFAB_RET_E_INVALID_MSG (malformed).
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

/*! @brief Scalar varint value types.
 *
 * By default the unsigned/signed scalar values carried by varint fields are
 * 64-bit. Defining @ref SOFAB_DISABLE_INT64_SUPPORT narrows them to 32-bit,
 * which removes the 64-bit varint math (smaller/faster on 32-bit MCUs) at the
 * cost of the 64-bit value range and wire compatibility for larger values —
 * see the @c SOFAB_DISABLE_INT64_SUPPORT notes below before enabling it.
 */
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
/*! @brief Unsigned value type */
typedef uint64_t sofab_unsigned_t;
#define SOFAB_UNSIGNED_MAX (UINT64_MAX)

/*! @brief Signed value type */
typedef int64_t sofab_signed_t;
#define SOFAB_SIGNED_MAX (INT64_MAX)
#define SOFAB_SIGNED_MIN (INT64_MIN)

/*! @brief Largest encodable field id (the (id<<3)|type header must fit in
 *  @ref sofab_unsigned_t). */
#define SOFAB_ID_MAX (INT32_MAX)
#else
/*! @brief Unsigned value type (32-bit; see SOFAB_DISABLE_INT64_SUPPORT) */
typedef uint32_t sofab_unsigned_t;
#define SOFAB_UNSIGNED_MAX (UINT32_MAX)

/*! @brief Signed value type (32-bit; see SOFAB_DISABLE_INT64_SUPPORT) */
typedef int32_t sofab_signed_t;
#define SOFAB_SIGNED_MAX (INT32_MAX)
#define SOFAB_SIGNED_MIN (INT32_MIN)

/*! @brief Largest encodable field id. The (id<<3)|type header is itself a
 *  varint accumulated in @ref sofab_unsigned_t, so with 32-bit values the id
 *  is capped to what leaves room for the 3 type bits (UINT32_MAX >> 3). */
#define SOFAB_ID_MAX (UINT32_MAX >> 3)
#endif /* !defined(SOFAB_DISABLE_INT64_SUPPORT) */

/*! @brief Maximum fixed-length field size in bytes */
#if SIZE_MAX == INT16_MAX
# define SOFAB_FIXLEN_MAX (INT16_MAX)
#else
# define SOFAB_FIXLEN_MAX (INT32_MAX)
#endif

/*! @brief Maximum number of elements in an array */
#if SIZE_MAX == INT16_MAX
# define SOFAB_ARRAY_MAX (INT16_MAX)
#else
# define SOFAB_ARRAY_MAX (INT32_MAX)
#endif

// disable double support automatically on platforms where double is not 8 bytes
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

/*!
 * @brief Narrow unsigned/signed scalar varint values from 64-bit to 32-bit.
 *
 * Changes @ref sofab_unsigned_t / @ref sofab_signed_t to 32-bit and removes the
 * 64-bit integer convenience API (read_u64/read_i64, write/read array_of_u64 and
 * array_of_i64). Useful on 32-bit MCUs where 64-bit varint math is emulated.
 *
 * @warning Change only if you know what you are doing — side effects:
 *  - @b Wire @b compatibility: the format itself is width-agnostic, so messages
 *    whose every value fits in 32 bits stay byte-identical and interoperable.
 *    A value beyond 32-bit range from a 64-bit peer is rejected as a malformed
 *    message (@ref SOFAB_RET_E_INVALID_MSG) and never silently truncated.
 *  - @b ABI: the value types appear in public signatures and context structs, so
 *    32-bit and 64-bit builds are not ABI-compatible — do not mix them.
 *  - @b Field @b ids: @ref SOFAB_ID_MAX shrinks to @c UINT32_MAX>>3, because the
 *    field header is itself a varint of @c (id<<3)|type.
 *  - @b Conformance: the shipped test vectors include 64-bit values and will not
 *    decode in this mode.
 */
// #define SOFAB_DISABLE_INT64_SUPPORT

/* object descriptor configuration ********************************************/
/*!
 * @brief Object descriptor size profiles.
 *
 * The integer widths used for the @ref sofab_object_descr_field_t members
 * @c id, @c offset and @c size are selectable so the descriptor footprint can
 * be tuned to the messages actually in use. Smaller profiles shrink every
 * descriptor table at the cost of a lower addressable range.
 *
 * @c offset and @c size both index into the described object structure, so
 * they always share the same width; @c id is bounded by the highest field ID
 * in use. All three follow the selected profile.
 *
 * Profiles are ordered ascending by capacity so generated code can verify at
 * compile time that the corelib is configured wide enough, e.g.
 * @code
 *   // require at least 16-bit descriptors
 *   #if SOFAB_OBJECT_DESCR_PROFILE < SOFAB_OBJECT_DESCR_MEDIUM
 *   # error "generated descriptors require at least the MEDIUM profile"
 *   #endif
 *
 *   // or check concrete limits
 *   #if MY_MESSAGE_MAX_FIELD_ID > SOFAB_OBJECT_DESCR_ID_MAX
 *   # error "field IDs exceed the configured descriptor id width"
 *   #endif
 * @endcode
 */
#define SOFAB_OBJECT_DESCR_SMALL  1  /*!< uint8_t  id/offset/size (max 255) */
#define SOFAB_OBJECT_DESCR_MEDIUM 2  /*!< uint16_t id/offset/size (max 65535) */
#define SOFAB_OBJECT_DESCR_BIG    3  /*!< uint32_t id/offset/size (max 4294967295) */

/*! @brief Selected object descriptor profile (default: MEDIUM). */
#ifndef SOFAB_OBJECT_DESCR_PROFILE
# define SOFAB_OBJECT_DESCR_PROFILE SOFAB_OBJECT_DESCR_MEDIUM
#endif

#if SOFAB_OBJECT_DESCR_PROFILE == SOFAB_OBJECT_DESCR_SMALL
typedef uint8_t  sofab_object_descr_id_t;
typedef uint8_t  sofab_object_descr_offset_t;
typedef uint8_t  sofab_object_descr_size_t;
# define SOFAB_OBJECT_DESCR_ID_MAX     (UINT8_MAX)
# define SOFAB_OBJECT_DESCR_OFFSET_MAX (UINT8_MAX)
# define SOFAB_OBJECT_DESCR_SIZE_MAX   (UINT8_MAX)
#elif SOFAB_OBJECT_DESCR_PROFILE == SOFAB_OBJECT_DESCR_MEDIUM
typedef uint16_t sofab_object_descr_id_t;
typedef uint16_t sofab_object_descr_offset_t;
typedef uint16_t sofab_object_descr_size_t;
# define SOFAB_OBJECT_DESCR_ID_MAX     (UINT16_MAX)
# define SOFAB_OBJECT_DESCR_OFFSET_MAX (UINT16_MAX)
# define SOFAB_OBJECT_DESCR_SIZE_MAX   (UINT16_MAX)
#elif SOFAB_OBJECT_DESCR_PROFILE == SOFAB_OBJECT_DESCR_BIG
typedef uint32_t sofab_object_descr_id_t;
typedef uint32_t sofab_object_descr_offset_t;
typedef uint32_t sofab_object_descr_size_t;
# define SOFAB_OBJECT_DESCR_ID_MAX     (UINT32_MAX)
# define SOFAB_OBJECT_DESCR_OFFSET_MAX (UINT32_MAX)
# define SOFAB_OBJECT_DESCR_SIZE_MAX   (UINT32_MAX)
#else
# error "SOFAB_OBJECT_DESCR_PROFILE must be SOFAB_OBJECT_DESCR_SMALL, _MEDIUM or _BIG"
#endif

/* sanity checks **************************************************************/
#if !defined(__SIZEOF_DOUBLE__) && !defined(SOFAB_DISABLE_FP64_SUPPORT)
typedef char sofab_check_size_double[(sizeof(double) == 8) ? 1 : -1];
#endif

typedef char sofab_check_size_float[(sizeof(float) == 4) ? 1 : -1];
typedef char sofab_check_size_char_t[(sizeof(char) == 1) ? 1 : -1];
typedef char sofab_check_size_uint8_t[(sizeof(uint8_t) == 1) ? 1 : -1];
typedef char sofab_check_size_int8_t[(sizeof(int8_t) == 1) ? 1 : -1];
typedef char sofab_check_size_uint16_t[(sizeof(uint16_t) == 2) ? 1 : -1];
typedef char sofab_check_size_int16_t[(sizeof(int16_t) == 2) ? 1 : -1];
typedef char sofab_check_size_uint32_t[(sizeof(uint32_t) == 4) ? 1 : -1];
typedef char sofab_check_size_int32_t[(sizeof(int32_t) == 4) ? 1 : -1];
typedef char sofab_check_size_uint64_t[(sizeof(uint64_t) == 8) ? 1 : -1];
typedef char sofab_check_size_int64_t[(sizeof(int64_t) == 8) ? 1 : -1];

/* exported vars **************************************************************/
// SOFAB_EXTERN type sofab_<varname>;

/* prototypes *****************************************************************/

#ifdef __cplusplus
}
#endif

/** @} */ // end of defgroup

#endif /* SOFAB_H */
