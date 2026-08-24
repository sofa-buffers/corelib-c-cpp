/*!
 * @file test_limits.c
 * @brief SofaBuffers test for the §6.2 format ceilings on a constrained profile
 *
 * CORELIB_PLAN §6.2 lets a constrained profile lower FIXLEN_MAX / ARRAY_MAX from
 * 2,147,483,647 to 65,535, and this port takes that allowance wherever `size_t`
 * is 16 bits — because the decoder narrows the declared length and count into a
 * `size_t` and a wider ceiling would let a value pass the bound check and then be
 * truncated modulo SIZE_MAX+1 (issue #150).
 *
 * The behaviour that matters therefore only exists on a target no CI job can run
 * a test image on. So this suite does not need a 16-bit target: it builds the
 * library sources with the ceilings *pinned* to the constrained values
 * (-DSOFAB_FIXLEN_MAX=65535 -DSOFAB_ARRAY_MAX=65535, see CMakeLists.txt) and
 * exercises the ceiling behaviour on the host. What it pins is the half that is
 * target-independent: that the ceiling is enforced, that it is enforced at the
 * right value, and that the value one below it still decodes.
 *
 * The other half — that the ceiling can never exceed what it is narrowed into,
 * which is the actual defect — is a compile-time assertion in istream.c. That one
 * does hold on AVR, because it fires during the build the AVR job already runs.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/istream.h"
#include "sofab/ostream.h"

#include "unity.h"

#include <string.h>

/* This suite is meaningless unless the build actually lowered the ceilings: with
 * them left at INT32_MAX every "above the ceiling" case below is instead a
 * plain in-range value and the tests would pass while testing nothing. Fail the
 * build rather than report a green suite that asserts the wrong thing. */
#if SOFAB_FIXLEN_MAX != 65535 || SOFAB_ARRAY_MAX != 65535
# error "test_limits.c must be built with -DSOFAB_FIXLEN_MAX=65535 -DSOFAB_ARRAY_MAX=65535"
#endif

/*****************************************************************************/
/* unity hooks */
/*****************************************************************************/

void setUp(void)    { }
void tearDown(void) { }

/*****************************************************************************/
/* helpers */
/*****************************************************************************/

/* A callback that binds nothing: every field is left unread, so it takes the
 * skip path. That is deliberate — a bound destination would be rejected by its
 * own capacity check (MESSAGE_SPEC §7.1) and could not tell us whether the
 * *ceiling* fired. Skipped, the only thing that can reject the field is §6.2. */
static void _counting_callback(
    sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
{
    (void)ctx;
    (void)id;
    (void)size;
    (void)count;
    (*(int *)usrptr)++;
}

static void _discard_flush(
    sofab_ostream_t *ctx, const uint8_t *data, size_t len, void *usrptr)
{
    (void)ctx;
    (void)data;
    (void)len;
    (void)usrptr;
}

/* Wire bytes. Field id 0 throughout, so the header byte is just the type tag.
 *
 *   fixlen blob : [0x02][varint((length << 3) | SOFAB_FIXLENTYPE_BLOB)]
 *   uint array  : [0x03][varint(element_count)]
 *
 * Blob rather than string so the bytes decode identically with and without
 * SOFAB_STRICT_UTF8 — a blob payload is never validated. */
#define HDR_FIXLEN  0x02
#define HDR_ARRAY_U 0x03

/*****************************************************************************/
/* decoder: the fixlen length ceiling */
/*****************************************************************************/

static sofab_ret_t _feed(const uint8_t *buf, size_t len, int *calls)
{
    sofab_istream_t ctx;
    *calls = 0;
    sofab_istream_init(&ctx, _counting_callback, calls);
    return sofab_istream_feed(&ctx, buf, len);
}

static void test_fixlen_length_at_ceiling_is_incomplete (void)
{
    /* 65535 == SOFAB_FIXLEN_MAX: the largest length this profile accepts. It must
     * NOT be rejected — the field is simply awaiting its payload. This is the
     * off-by-one guard on the ceiling itself: a ceiling of INT16_MAX (32767)
     * would wrongly reject this, which is what merely fixing the #if condition
     * and leaving the value alone would have produced. */
    const uint8_t buffer[] = {HDR_FIXLEN, 0xFB, 0xFF, 0x1F};   /* len 65535 */
    int calls;

    TEST_ASSERT_EQUAL(SOFAB_RET_INCOMPLETE, _feed(buffer, sizeof(buffer), &calls));
    TEST_ASSERT_EQUAL_INT(1, calls);   /* accepted: the field callback did fire */
}

static void test_fixlen_length_above_ceiling_is_invalid (void)
{
    /* 65536 == SOFAB_FIXLEN_MAX + 1. On a 16-bit-size_t target this is the exact
     * value that used to narrow to 0, completing the field as empty and leaving
     * the decoder to read 65536 payload bytes as field headers (issue #150). */
    const uint8_t buffer[] = {HDR_FIXLEN, 0x83, 0x80, 0x20};   /* len 65536 */
    int calls;

    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, _feed(buffer, sizeof(buffer), &calls));
    TEST_ASSERT_EQUAL_INT(0, calls);
}

static void test_fixlen_length_above_ceiling_nonzero_residue_is_invalid (void)
{
    /* 65540 narrows to 4 rather than to 0 — the harder desynchronisation of the
     * two, because the decoder consumes a plausible short payload and resyncs on
     * the remaining bytes instead of visibly doing nothing. */
    const uint8_t buffer[] = {HDR_FIXLEN, 0xA3, 0x80, 0x20};   /* len 65540 */
    int calls;

    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, _feed(buffer, sizeof(buffer), &calls));
    TEST_ASSERT_EQUAL_INT(0, calls);
}

static void test_fixlen_length_above_ceiling_is_terminal (void)
{
    /* §5.2.3: INVALID wins over INCOMPLETE and stays INVALID. Feeding the payload
     * afterwards must not turn the verdict around — no continuation can make an
     * over-ceiling length valid. */
    const uint8_t head[] = {HDR_FIXLEN, 0x83, 0x80, 0x20};     /* len 65536 */
    const uint8_t tail[] = {0x00, 0x00, 0x00, 0x00};
    sofab_istream_t ctx;
    int calls = 0;

    sofab_istream_init(&ctx, _counting_callback, &calls);
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG,
                      sofab_istream_feed(&ctx, head, sizeof(head)));
    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG,
                      sofab_istream_feed(&ctx, tail, sizeof(tail)));
    TEST_ASSERT_EQUAL_INT(0, calls);
}

/*****************************************************************************/
/* decoder: the array count ceiling */
/*****************************************************************************/

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)

static void test_array_count_at_ceiling_is_incomplete (void)
{
    const uint8_t buffer[] = {HDR_ARRAY_U, 0xFF, 0xFF, 0x03};  /* count 65535 */
    int calls;

    TEST_ASSERT_EQUAL(SOFAB_RET_INCOMPLETE, _feed(buffer, sizeof(buffer), &calls));
    TEST_ASSERT_EQUAL_INT(1, calls);
}

static void test_array_count_above_ceiling_is_invalid (void)
{
    const uint8_t buffer[] = {HDR_ARRAY_U, 0x80, 0x80, 0x04};  /* count 65536 */
    int calls;

    TEST_ASSERT_EQUAL(SOFAB_RET_E_INVALID_MSG, _feed(buffer, sizeof(buffer), &calls));
    TEST_ASSERT_EQUAL_INT(0, calls);
}

#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

/*****************************************************************************/
/* encoder: the same ceilings, the other direction */
/*****************************************************************************/

/* §6.2 states the ceilings over the wire format, not over one direction of it.
 * Unchecked, the encoder writes the declared length from the full int32_t and
 * then copies `(size_t)datalen` bytes — a header announcing N followed by
 * N mod (SIZE_MAX+1) of them, i.e. a corrupt stream produced by a well-formed
 * call. These pin the rejection instead. */

static uint8_t _payload[65536];

static void test_encode_fixlen_at_ceiling_is_accepted (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[64];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, _discard_flush, NULL);
    TEST_ASSERT_EQUAL(SOFAB_RET_OK,
        sofab_ostream_write_blob(&ctx, 0, _payload, SOFAB_FIXLEN_MAX));
}

static void test_encode_fixlen_above_ceiling_is_argument_error (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[64];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, _discard_flush, NULL);
    TEST_ASSERT_EQUAL(SOFAB_RET_E_ARGUMENT,
        sofab_ostream_write_blob(&ctx, 0, _payload, SOFAB_FIXLEN_MAX + 1));

    /* Rejected before anything reached the wire: a half-written header is worse
     * than no header, because the caller cannot tell the stream is now junk. */
    TEST_ASSERT_EQUAL_size_t(0, sofab_ostream_flush(&ctx));
}

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)

static void test_encode_array_above_ceiling_is_argument_error (void)
{
    sofab_ostream_t ctx;
    uint8_t buffer[64];

    sofab_ostream_init(&ctx, buffer, sizeof(buffer), 0, _discard_flush, NULL);
    TEST_ASSERT_EQUAL(SOFAB_RET_E_ARGUMENT,
        sofab_ostream_write_array_of_u8(&ctx, 0, _payload, SOFAB_ARRAY_MAX + 1));
    TEST_ASSERT_EQUAL_size_t(0, sofab_ostream_flush(&ctx));
}

#endif /* !defined(SOFAB_DISABLE_ARRAY_SUPPORT) */

/*****************************************************************************/
/* runner */
/*****************************************************************************/

int main (void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fixlen_length_at_ceiling_is_incomplete);
    RUN_TEST(test_fixlen_length_above_ceiling_is_invalid);
    RUN_TEST(test_fixlen_length_above_ceiling_nonzero_residue_is_invalid);
    RUN_TEST(test_fixlen_length_above_ceiling_is_terminal);

#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    RUN_TEST(test_array_count_at_ceiling_is_incomplete);
    RUN_TEST(test_array_count_above_ceiling_is_invalid);
#endif

    RUN_TEST(test_encode_fixlen_at_ceiling_is_accepted);
    RUN_TEST(test_encode_fixlen_above_ceiling_is_argument_error);
#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    RUN_TEST(test_encode_array_above_ceiling_is_argument_error);
#endif

    return UNITY_END();
}
