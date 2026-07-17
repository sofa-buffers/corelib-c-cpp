/*!
 * @file utf8.h
 * @brief SofaBuffers C - Hand-written UTF-8 validator (SOFAB_STRICT_UTF8).
 *
 * A `string` payload is UTF-8 (MESSAGE_SPEC §8); this module provides the
 * validator the corelib uses to enforce that on the encode and decode paths when
 * @ref SOFAB_STRICT_UTF8 is enabled (the default). It is a security surface — a
 * real validator, not a byte-range shortcut — and is declared (and compiled)
 * only when the check is on, so a build with @c SOFAB_DISABLE_STRICT_UTF8 pays
 * zero @c .text / @c .rodata cost (CORELIB_PLAN §6.4).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_UTF8_H
#define SOFAB_UTF8_H

/**
 * @defgroup c_api C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/* includes *******************************************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "sofab/sofab.h"

/* prototypes *****************************************************************/

#if SOFAB_STRICT_UTF8
/*!
 * @brief Test whether @p len bytes at @p data are a well-formed UTF-8 string.
 *
 * A complete, self-contained validator (not incremental): it accepts exactly the
 * byte sequences that are valid UTF-8 for the whole @p data range. It is a real
 * validator, not a lead-byte-range shortcut, and @b rejects (returns @c false):
 *  - @b overlong encodings — any sequence whose code point could have been
 *    encoded in fewer bytes, including the 2-byte @c C0 @c 80 (Java "Modified
 *    UTF-8" NUL) and the overlong 3-/4-byte forms;
 *  - @b surrogate code points @c U+D800 – @c U+DFFF (never valid in UTF-8);
 *  - code points @b above @c U+10FFFF (out of Unicode range);
 *  - bare continuation bytes (@c 0x80 – @c 0xBF with no lead), invalid lead
 *    bytes (@c 0xC0, @c 0xC1, @c 0xF5 – @c 0xFF), and any multi-byte sequence
 *    @b truncated within @p data (a lead byte whose continuation bytes run past
 *    @p len).
 *
 * Embedded @c U+0000 — a single @c 0x00 byte — is @b valid UTF-8 and accepted;
 * only its overlong form @c C0 @c 80 is rejected.
 *
 * The caller owns cross-chunk framing: a sequence merely split at a chunk
 * boundary must be reassembled (or validated once the declared payload is
 * complete) before calling this, so a well-formed prefix is not mistaken for an
 * error (CORELIB_PLAN §5.2 / §6.4).
 *
 * @param data  Pointer to the bytes to validate (may be @c NULL only if @p len
 *              is 0).
 * @param len   Number of bytes at @p data.
 * @return @c true if @p data is valid UTF-8 (an empty range is valid), @c false
 *         otherwise.
 */
extern bool sofab_utf8_valid(const uint8_t *data, size_t len);
#endif /* SOFAB_STRICT_UTF8 */

#ifdef __cplusplus
}
#endif

/** @} */ // end of defgroup

#endif /* SOFAB_UTF8_H */
