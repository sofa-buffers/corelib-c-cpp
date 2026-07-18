/*!
 * @file utf8.c
 * @brief SofaBuffers C - Hand-written UTF-8 validator (see utf8.h).
 *
 * SPDX-License-Identifier: MIT
 */

#define SOFAB_C

/* includes *******************************************************************/
#include "sofab/utf8.h"

#if SOFAB_STRICT_UTF8

/* functions ******************************************************************/

extern bool sofab_utf8_valid(const uint8_t *data, size_t len)
{
    size_t i = 0;

    while (i < len)
    {
        uint8_t b0 = data[i];

        /* 0x00 .. 0x7F: ASCII, one byte (embedded U+0000 is valid). */
        if (b0 < 0x80u)
        {
            i++;
            continue;
        }

        /* Decode the lead byte: how many continuation bytes follow, the initial
         * code-point bits, and the smallest code point that is NOT overlong for
         * this length. Everything else (0x80..0xBF bare continuation, 0xC0/0xC1
         * always-overlong leads, 0xF5..0xFF out-of-range leads) is rejected. */
        size_t   need;   /* number of continuation bytes expected */
        uint32_t cp;     /* accumulating code point */
        uint32_t min_cp; /* smallest non-overlong value for this length */

        if ((b0 & 0xE0u) == 0xC0u)        /* 110xxxxx: 2-byte sequence */
        {
            /* 0xC0/0xC1 give a max value of 0x7F -> always overlong; the
             * min_cp = 0x80 check below rejects them. */
            need = 1;
            cp = (uint32_t)(b0 & 0x1Fu);
            min_cp = 0x80u;
        }
        else if ((b0 & 0xF0u) == 0xE0u)   /* 1110xxxx: 3-byte sequence */
        {
            need = 2;
            cp = (uint32_t)(b0 & 0x0Fu);
            min_cp = 0x800u;
        }
        else if ((b0 & 0xF8u) == 0xF0u)   /* 11110xxx: 4-byte sequence */
        {
            /* 0xF5..0xF7 have a lead value >= 5, pushing cp above U+10FFFF; the
             * range check below rejects them. */
            need = 3;
            cp = (uint32_t)(b0 & 0x07u);
            min_cp = 0x10000u;
        }
        else
        {
            /* 0x80..0xBF bare continuation byte, or 0xF8..0xFF invalid lead. */
            return false;
        }

        /* The continuation bytes must all lie within [i, len); a sequence whose
         * bytes run past the end of the payload is truncated -> invalid. (For a
         * chunked decoder this range is the COMPLETE payload, so this is the
         * end-of-payload truncation case, not the end-of-chunk split case.) */
        if (need >= len - i)
        {
            return false;
        }

        for (size_t k = 1; k <= need; k++)
        {
            uint8_t bc = data[i + k];
            if ((bc & 0xC0u) != 0x80u)   /* not a 10xxxxxx continuation byte */
            {
                return false;
            }
            cp = (cp << 6) | (uint32_t)(bc & 0x3Fu);
        }

        if (cp < min_cp)                 /* overlong encoding */
        {
            return false;
        }
        if (cp > 0x10FFFFu)              /* above the Unicode range */
        {
            return false;
        }
        if (cp >= 0xD800u && cp <= 0xDFFFu) /* UTF-16 surrogate code point */
        {
            return false;
        }

        i += need + 1;
    }

    return true;
}

#endif /* SOFAB_STRICT_UTF8 */
