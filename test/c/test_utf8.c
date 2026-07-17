/*!
 * @file test_utf8.c
 * @brief SofaBuffers test for the strict UTF-8 validator and its wiring.
 *
 * Exercises three surfaces of SOFAB_STRICT_UTF8 (CORELIB_PLAN §6.4):
 *   1. the hand-written validator sofab_utf8_valid() directly — valid strings,
 *      every overlong form (incl. C0 80), surrogates, out-of-range code points,
 *      truncated sequences, bare continuation / invalid lead bytes, and the
 *      1/2/3/4-byte boundaries;
 *   2. encode wiring — a non-UTF-8 `string` is refused with SOFAB_RET_E_ARGUMENT
 *      while a `blob` with the same bytes is accepted;
 *   3. decode wiring — a materialized invalid string is SOFAB_RET_E_INVALID_MSG,
 *      a skipped one is never validated, and cross-chunk framing is honored
 *      (a split multi-byte sequence stays INCOMPLETE; only a payload complete to
 *      its declared length is validated).
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/utf8.h"
#include "sofab/ostream.h"
#include "sofab/istream.h"

#include "unity.h"

#include <string.h>

#if SOFAB_STRICT_UTF8

/* validator: valid strings **************************************************/

static void expect_valid(const void *b, size_t n, const char *what)
{
    TEST_ASSERT_TRUE_MESSAGE(sofab_utf8_valid((const uint8_t *)b, n), what);
}

static void expect_invalid(const void *b, size_t n, const char *what)
{
    TEST_ASSERT_FALSE_MESSAGE(sofab_utf8_valid((const uint8_t *)b, n), what);
}

static void test_utf8_valid_ascii_and_empty(void)
{
    expect_valid("", 0, "empty string is valid");
    expect_valid("Hello Couch!", 12, "plain ASCII");
    /* embedded U+0000 (a single 00 byte) is valid UTF-8 */
    expect_valid("a\0b", 3, "embedded NUL is valid");
    const uint8_t nul = 0x00;
    expect_valid(&nul, 1, "single NUL byte is valid");
    const uint8_t del = 0x7F; /* U+007F, highest 1-byte */
    expect_valid(&del, 1, "U+007F 1-byte boundary");
}

static void test_utf8_valid_multibyte_boundaries(void)
{
    expect_valid("\xC2\x80", 2, "U+0080 lowest 2-byte");
    expect_valid("\xDF\xBF", 2, "U+07FF highest 2-byte");
    expect_valid("\xE0\xA0\x80", 3, "U+0800 lowest 3-byte");
    expect_valid("\xEF\xBF\xBF", 3, "U+FFFF highest 3-byte");
    expect_valid("\xF0\x90\x80\x80", 4, "U+10000 lowest 4-byte");
    expect_valid("\xF4\x8F\xBF\xBF", 4, "U+10FFFF highest 4-byte");
    /* the just-below-surrogate and just-above-surrogate code points are valid */
    expect_valid("\xED\x9F\xBF", 3, "U+D7FF (just below surrogates)");
    expect_valid("\xEE\x80\x80", 3, "U+E000 (just above surrogates)");
    /* the äöüÄÖÜß mix used in the shared vectors */
    expect_valid("\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x84\xC3\x96\xC3\x9C\xC3\x9F", 14, "aeoeue mix");
}

/* validator: overlong encodings *********************************************/

static void test_utf8_reject_overlong(void)
{
    expect_invalid("\xC0\x80", 2, "overlong C0 80 (Modified-UTF-8 NUL)");
    expect_invalid("\xC0\xAF", 2, "overlong C0 AF");
    expect_invalid("\xC1\xBF", 2, "overlong C1 BF (would be U+007F)");
    expect_invalid("\xE0\x80\x80", 3, "overlong 3-byte E0 80 80");
    expect_invalid("\xE0\x9F\xBF", 3, "overlong 3-byte E0 9F BF (below U+0800)");
    expect_invalid("\xF0\x80\x80\x80", 4, "overlong 4-byte F0 80 80 80");
    expect_invalid("\xF0\x8F\xBF\xBF", 4, "overlong 4-byte F0 8F BF BF (below U+10000)");
}

/* validator: surrogates and out-of-range ************************************/

static void test_utf8_reject_surrogates_and_range(void)
{
    expect_invalid("\xED\xA0\x80", 3, "lone surrogate U+D800");
    expect_invalid("\xED\xAF\xBF", 3, "lone surrogate U+DBFF (high)");
    expect_invalid("\xED\xB0\x80", 3, "lone surrogate U+DC00 (low)");
    expect_invalid("\xED\xBF\xBF", 3, "lone surrogate U+DFFF");
    expect_invalid("\xF4\x90\x80\x80", 4, "U+110000 above U+10FFFF");
    expect_invalid("\xF5\x80\x80\x80", 4, "F5 lead exceeds U+10FFFF");
    expect_invalid("\xF7\xBF\xBF\xBF", 4, "F7 lead exceeds U+10FFFF");
}

/* validator: bare continuation / invalid lead bytes *************************/

static void test_utf8_reject_stray_bytes(void)
{
    expect_invalid("\x80", 1, "bare continuation 0x80");
    expect_invalid("\xBF", 1, "bare continuation 0xBF");
    expect_invalid("\xFE", 1, "0xFE is never valid");
    expect_invalid("\xFF", 1, "0xFF is never valid");
    expect_invalid("A\x80""B", 3, "continuation byte in the middle");
    /* a valid char followed by a stray byte */
    expect_invalid("\xC2\x80\x80", 3, "valid 2-byte then a stray continuation");
}

/* validator: truncated multi-byte sequences (end-of-payload) ****************/

static void test_utf8_reject_truncated(void)
{
    expect_invalid("\xC2", 1, "2-byte lead, no continuation");
    expect_invalid("\xE2\x82", 2, "3-byte lead, one continuation missing");
    expect_invalid("\xE2", 1, "3-byte lead alone");
    expect_invalid("\xF0\x90\x80", 3, "4-byte lead, final continuation missing");
    expect_invalid("\xF0", 1, "4-byte lead alone");
    /* second byte is not a continuation byte */
    expect_invalid("\xC2\x41", 2, "2-byte lead then ASCII 'A'");
    expect_invalid("\xE2\x28\xA1", 3, "3-byte lead then '(' breaks the sequence");
}

/* encode wiring *************************************************************/

static void test_utf8_encode_rejects_invalid_string(void)
{
    uint8_t buf[64];
    sofab_ostream_t os;

    /* invalid UTF-8 as a string -> InvalidArgument, nothing emitted */
    sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_E_ARGUMENT,
        sofab_ostream_write_fixlen(&os, 0, "\xC0\x80", 2, SOFAB_FIXLENTYPE_STRING));

    /* the SAME bytes as a blob are accepted (blobs are opaque, never validated) */
    sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_OK,
        sofab_ostream_write_blob(&os, 0, "\xC0\x80", 2));

    /* a valid UTF-8 string is accepted */
    sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_OK,
        sofab_ostream_write_string(&os, 0, "\xC3\xA4")); /* ä */

    /* an empty string is valid */
    sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_OK, sofab_ostream_write_string(&os, 0, ""));
}

/* decode wiring *************************************************************/

/* build a single `string` field (id 0) whose payload is the raw bytes given */
static size_t build_string_msg(uint8_t *out, const uint8_t *payload, size_t len)
{
    size_t n = 0;
    out[n++] = (uint8_t)((0u << 3) | SOFAB_TYPE_FIXLEN);          /* header  */
    out[n++] = (uint8_t)((len << 3) | SOFAB_FIXLENTYPE_STRING);   /* word    */
    memcpy(out + n, payload, len);
    return n + len;
}

/* field callback that MATERIALIZES the string (reads it) */
static void read_string_cb(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)id; (void)size; (void)count;
    sofab_istream_read_string_noterm(ctx, (char *)usr, 64);
}

/* field callback that SKIPS every field (binds nothing) */
static void skip_cb(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)ctx; (void)id; (void)size; (void)count; (void)usr;
}

static void test_utf8_decode_rejects_materialized_invalid(void)
{
    const uint8_t bad[] = {0xC0, 0x80}; /* overlong NUL */
    uint8_t msg[8];
    size_t  n = build_string_msg(msg, bad, sizeof(bad));

    char dst[64];
    sofab_istream_t is;
    sofab_istream_init(&is, read_string_cb, dst);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_E_INVALID_MSG, sofab_istream_feed(&is, msg, n));
}

static void test_utf8_decode_skipped_not_validated(void)
{
    const uint8_t bad[] = {0xC0, 0x80};
    uint8_t msg[8];
    size_t  n = build_string_msg(msg, bad, sizeof(bad));

    /* the string is skipped (never read), so its bytes are never validated:
     * the message decodes to a clean boundary (COMPLETE / OK). */
    sofab_istream_t is;
    sofab_istream_init(&is, skip_cb, NULL);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_OK, sofab_istream_feed(&is, msg, n));
}

static void test_utf8_decode_valid_string_ok(void)
{
    const uint8_t ok[] = {0xC3, 0xA4}; /* ä */
    uint8_t msg[8];
    size_t  n = build_string_msg(msg, ok, sizeof(ok));

    char dst[64];
    sofab_istream_t is;
    sofab_istream_init(&is, read_string_cb, dst);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_OK, sofab_istream_feed(&is, msg, n));
    TEST_ASSERT_EQUAL_UINT8(0xC3, (uint8_t)dst[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA4, (uint8_t)dst[1]);
}

static void test_utf8_decode_split_multibyte_stays_incomplete(void)
{
    /* a valid 3-byte char (U+20AC euro) split across the chunk boundary: the
     * well-formed prefix must NOT be reported INVALID; it stays INCOMPLETE until
     * the final byte arrives, then completes cleanly. */
    const uint8_t euro[] = {0xE2, 0x82, 0xAC};
    uint8_t msg[8];
    size_t  n = build_string_msg(msg, euro, sizeof(euro)); /* 02 1a e2 82 ac */

    char dst[64];
    sofab_istream_t is;
    sofab_istream_init(&is, read_string_cb, dst);

    /* header + word + first two payload bytes: mid-sequence, INCOMPLETE */
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_INCOMPLETE, sofab_istream_feed(&is, msg, n - 1));
    /* final byte completes the valid char: OK, never INVALID */
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_OK, sofab_istream_feed(&is, &msg[n - 1], 1));
}

static void test_utf8_decode_truncated_at_payload_is_invalid(void)
{
    /* a 3-byte lead with only two payload bytes DECLARED (length 2): the payload
     * is complete to its declared length but the sequence is truncated -> INVALID
     * (distinct from the end-of-chunk split above). */
    const uint8_t trunc[] = {0xE2, 0x82};
    uint8_t msg[8];
    size_t  n = build_string_msg(msg, trunc, sizeof(trunc));

    char dst[64];
    sofab_istream_t is;
    sofab_istream_init(&is, read_string_cb, dst);
    TEST_ASSERT_EQUAL_INT(SOFAB_RET_E_INVALID_MSG, sofab_istream_feed(&is, msg, n));
}

int test_utf8_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_utf8_valid_ascii_and_empty);
    RUN_TEST(test_utf8_valid_multibyte_boundaries);
    RUN_TEST(test_utf8_reject_overlong);
    RUN_TEST(test_utf8_reject_surrogates_and_range);
    RUN_TEST(test_utf8_reject_stray_bytes);
    RUN_TEST(test_utf8_reject_truncated);

    RUN_TEST(test_utf8_encode_rejects_invalid_string);

    RUN_TEST(test_utf8_decode_rejects_materialized_invalid);
    RUN_TEST(test_utf8_decode_skipped_not_validated);
    RUN_TEST(test_utf8_decode_valid_string_ok);
    RUN_TEST(test_utf8_decode_split_multibyte_stays_incomplete);
    RUN_TEST(test_utf8_decode_truncated_at_payload_is_invalid);

    return UNITY_END();
}

#else /* !SOFAB_STRICT_UTF8 */

int test_utf8_main(void)
{
    return 0; /* strict UTF-8 compiled out: nothing to validate */
}

#endif /* SOFAB_STRICT_UTF8 */
