/*!
 * @file bench.c
 * @brief SofaBuffers C — throughput benchmark (MB/s, CPU time).
 *
 * Standalone benchmark reporting encode/decode throughput for the four BENCH_SPEC
 * datasets: a 1000-element u64 array, a small "typical" mixed message, an
 * unbounded 1 MB blob (one-shot, streamed and decoded in chunks) and the
 * "composite" message that exercises the paths the other three never reach.
 * Each workload runs in a ~1 second loop and reports MB/s.
 *
 * Throughput is measured against *process CPU time* (clock(), not wall-clock),
 * so the number reflects the cost of the implementation rather than OS
 * scheduling noise or the wall-clock speed of the host. MB = 1e6 bytes.
 *
 * Two modes:
 *   bench_c              -> timed MB/s table (default, CPU time).
 *   bench_c <workload>   -> run one operation once and exit; used by
 *                           run_callgrind.sh to count instructions/op under
 *                           Callgrind (a machine-independent metric). The
 *                           run_<workload> functions are noinline with external
 *                           linkage so --toggle-collect=run_<workload> can
 *                           isolate exactly one operation.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sofab/istream.h"
#include "sofab/ostream.h"

#define N 1000

/* --- blob 1MB (BENCH_SPEC "blob 1MB message") ------------------------------
 * One unbounded blob field at id 1. The payload is exactly 1,000,000 bytes, so
 * MB/s reads directly against the MB = 1e6 convention, and the encoding is
 * 1,000,005 bytes on every implementation: a 1-byte header ((1 << 3) | 2), a
 * 4-byte fixlen_word ((1000000 << 3) | 3) and the payload. That total is a
 * parity check across ports and is verified below before anything is printed.
 *
 * The streaming row drives the same bytes through a buffer of exactly 4096 —
 * fixed rather than this port's own size, so the row stays comparable across
 * languages — which is the only place in the suite where the divisible-run path
 * (CORELIB_PLAN §5.1) is exercised at all. */
#define BLOB_LEN     1000000
#define BLOB_ENCODED 1000005
#define BLOB_CHUNK   4096

/* --- composite (BENCH_SPEC "composite message") ----------------------------
 * Five fields covering what the flat datasets miss: a wrapper array (a header
 * per element, MESSAGE_SPEC §5.1) whose element ids straddle the one/two-byte
 * boundary, a non-ASCII string, sequence nesting three deep, a field the
 * encoder must NOT write, and the suite's only two-byte field header. */
#define COMP_ITEMS    64
#define COMP_ITEM_MAX 12 /* "item-63" + NUL, rounded up */

/* "a" U+00E4 U+20AC U+1D11E as UTF-8 — 1-, 2-, 3- and 4-byte sequences, ten
 * bytes per repetition. Written as hex escapes so the file stays ASCII; the
 * 4-byte sequence is a surrogate pair in every UTF-16 port. */
#define COMP_GLYPHS "a\xC3\xA4\xE2\x82\xAC\xF0\x9D\x84\x9E"
#define COMP_X2(s)  s s
#define COMP_X4(s)  s s s s
/* 2 * 4 * 4 = 32 repetitions = 320 bytes */
#define COMP_TEXT   COMP_X2(COMP_X4(COMP_X4(COMP_GLYPHS)))
#define COMP_TEXT_LEN (sizeof(COMP_TEXT) - 1)

/* shared buffers (static => not part of the measured stack work) */
static uint64_t src[N];
static uint8_t  enc_u64_buf[N * 11 + 16];
static size_t   enc_u64_used;

static uint8_t  typ_buf[256];
static size_t   typ_used;
static const uint16_t arr16[4] = {10, 20, 30, 40};

static uint8_t  blob_src[BLOB_LEN];
static uint8_t  blob_buf[BLOB_ENCODED];  /* one-shot: sized by hand, not from MAX_SIZE */
static uint8_t  blob_chunk_buf[BLOB_CHUNK];
static size_t   blob_used;               /* bytes of the one-shot encoding */
static size_t   blob_streamed;           /* bytes handed to the sink */
static uint8_t  blob_sink_xor;           /* keeps the sink call from being elided */
static uint8_t  blob_dec[BLOB_LEN];

static char     comp_items[COMP_ITEMS][COMP_ITEM_MAX];
static uint8_t  comp_buf[2048];
static size_t   comp_used;

/* decode targets */
static uint64_t dec_array[N];
static struct
{
    uint32_t f1;
    int32_t  f2;
    bool     f3;
    float    f4;
    char     f5[16];
    uint16_t f6[4];
    uint32_t s_f1;
    int32_t  s_f2;
} T;
static sofab_istream_decoder_t nested_dec;

static struct
{
    char     items[COMP_ITEMS][COMP_ITEM_MAX];
    char     text[COMP_TEXT_LEN + 1];
    uint32_t deep;  /* field 3 -> 1 -> 1 -> 1 */
    int32_t  tail;  /* field 3 -> 2 */
    uint32_t f130;
} C;
static sofab_istream_decoder_t comp_dec_items, comp_dec_l1, comp_dec_l2, comp_dec_l3;

static void make_src(void)
{
    for (int i = 0; i < N; i++)
        src[i] = (uint64_t)i * 0x9E3779B97F4A7C15ULL;
}

/* Same constant as the u64 array, low byte of the wrapping multiply. */
static void make_blob(void)
{
    for (size_t i = 0; i < BLOB_LEN; i++)
        blob_src[i] = (uint8_t)((uint64_t)i * 0x9E3779B97F4A7C15ULL);
}

/* Element i is "item-" + i in decimal, no padding: item-0 ... item-63. */
static void make_composite(void)
{
    for (int i = 0; i < COMP_ITEMS; i++)
        snprintf(comp_items[i], sizeof comp_items[i], "item-%d", i);
}

static void encode_typical(sofab_ostream_t *os)
{
    sofab_ostream_write_unsigned(os, 1, 0xDEADBEEF);
    sofab_ostream_write_signed(os, 2, -12345);
    sofab_ostream_write_boolean(os, 3, true);
    sofab_ostream_write_fp32(os, 4, 3.14159f);
    sofab_ostream_write_string(os, 5, "sofab");
    sofab_ostream_write_array_of_unsigned(os, 6, arr16, 4, sizeof(uint16_t));
    sofab_ostream_write_sequence_begin(os, 7);
    sofab_ostream_write_unsigned(os, 1, 99);
    sofab_ostream_write_signed(os, 2, -7);
    sofab_ostream_write_sequence_end(os);
}

/* The composite message opens its sequences lazily, because field 4 below is
 * only omitted if the opener holds its header back. A SOFAB_DISABLE_LAZY_SEQ
 * build has no such opener, so it frames the two sequences that do have content
 * eagerly and drops field 4 outright — which is what "omitted" means on the
 * wire, so the encoded bytes are identical in both builds. What that build no
 * longer measures is the discard path itself, which is the switch's whole
 * point. This keeps bench_c buildable in the reduced configuration, which is how
 * the hold-back's Ir/op cost in the README is measured. */
#if defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
#  define COMP_SEQ_BEGIN(os, id) sofab_ostream_write_sequence_begin((os), (id))
#else
#  define COMP_SEQ_BEGIN(os, id) sofab_ostream_write_sequence_begin_lazy((os), (id))
#endif

static void encode_composite(sofab_ostream_t *os)
{
    /* Field 1 — string array in wrapper form (MESSAGE_SPEC §5.1): an ordinary
     * sequence whose child id IS the array index, so every element carries its
     * own header. Ids 0..15 encode in one byte, 16..63 in two. The opener is the
     * lazy one throughout this message: field 4 below needs it to be omitted,
     * and using one opener everywhere keeps a single code path — the first
     * element write commits the held-back header, so the bytes are the same as
     * an eager open would produce. */
    COMP_SEQ_BEGIN(os, 1);
    for (int i = 0; i < COMP_ITEMS; i++)
        sofab_ostream_write_string(os, (sofab_id_t)i, comp_items[i]);
    sofab_ostream_write_sequence_end(os);

    /* Field 2 — 320 UTF-8 bytes covering 1-, 2-, 3- and 4-byte sequences. */
    sofab_ostream_write_string(os, 2, COMP_TEXT);

    /* Field 3 — nesting at depth 3, so the lazy hold-back run grows past the
     * single level `typical` and `perf` reach. */
    COMP_SEQ_BEGIN(os, 3);
    COMP_SEQ_BEGIN(os, 1);
    COMP_SEQ_BEGIN(os, 1);
    sofab_ostream_write_unsigned(os, 1, 7);
    sofab_ostream_write_sequence_end(os);
    sofab_ostream_write_sequence_end(os);
    sofab_ostream_write_signed(os, 2, -1);
    sofab_ostream_write_sequence_end(os);

    /* Field 4 — a struct equal to its declared default, which the encoder must
     * NOT write (MESSAGE_SPEC §2). Opened lazily and closed with nothing in it:
     * the held-back header is discarded and no byte reaches the wire. This is
     * the only field in the suite that takes the omit branch, and the only one
     * that exercises the hold-back's discard path. */
#if !defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
    sofab_ostream_write_sequence_begin_lazy(os, 4);
    sofab_ostream_write_sequence_end(os);
#endif

    /* Field 130 — the suite's only two-byte field header: (130 << 3) | 0. */
    sofab_ostream_write_unsigned(os, 130, 0xDEADBEEF);
}

/* Consume and discard, as BENCH_SPEC requires of the streaming sink: it must
 * not accumulate the bytes (that would charge the streaming row a copy the
 * one-shot row never pays) and must not do I/O (not deterministic under
 * Callgrind). Touching one byte is the minimum that keeps the call from being
 * optimised away; the length total is the encoded-size parity check. */
static void blob_sink(sofab_ostream_t *ctx, const uint8_t *data, size_t len, void *usr)
{
    (void)ctx;
    (void)usr;
    if (len)
        blob_sink_xor ^= data[0];
    blob_streamed += len;
}

/* ---- workloads ----------------------------------------------------------- */

__attribute__((noinline)) void run_encode_u64_array(void)
{
    sofab_ostream_t os;
    sofab_ostream_init(&os, enc_u64_buf, sizeof enc_u64_buf, 0, NULL, NULL);
    sofab_ostream_write_array_of_unsigned(&os, 1, src, N, sizeof(uint64_t));
    enc_u64_used = sofab_ostream_bytes_used(&os);
}

__attribute__((noinline)) void run_encode_typical(void)
{
    sofab_ostream_t os;
    sofab_ostream_init(&os, typ_buf, sizeof typ_buf, 0, NULL, NULL);
    encode_typical(&os);
    typ_used = sofab_ostream_bytes_used(&os);
}

static void cb_array(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)usr;
    if (id == 1)
        sofab_istream_read_array_of_u64(ctx, dec_array, count);
}

__attribute__((noinline)) void run_decode_u64_array(void)
{
    sofab_istream_t is;
    sofab_istream_init(&is, cb_array, NULL);
    sofab_istream_feed(&is, enc_u64_buf, enc_u64_used);
}

static void cb_child(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)count;
    (void)usr;
    if (id == 1)
        sofab_istream_read_u32(ctx, &T.s_f1);
    else if (id == 2)
        sofab_istream_read_i32(ctx, &T.s_f2);
}

static void cb_typical(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)usr;
    switch (id)
    {
        case 1: sofab_istream_read_u32(ctx, &T.f1); break;
        case 2: sofab_istream_read_i32(ctx, &T.f2); break;
        case 3: sofab_istream_read_bool(ctx, &T.f3); break;
        case 4: sofab_istream_read_fp32(ctx, &T.f4); break;
        case 5: sofab_istream_read_string(ctx, T.f5, sizeof T.f5); break;
        case 6: sofab_istream_read_array_of_u16(ctx, T.f6, count); break;
        case 7: sofab_istream_read_sequence(ctx, &nested_dec, cb_child, NULL); break;
        default: break;
    }
}

__attribute__((noinline)) void run_decode_typical(void)
{
    sofab_istream_t is;
    sofab_istream_init(&is, cb_typical, NULL);
    sofab_istream_feed(&is, typ_buf, typ_used);
}

/* Caller buffer of 1,000,005 bytes and no sink: one contiguous write, no flush
 * logic. This is the floor the streaming row is read against. */
__attribute__((noinline)) void run_encode_blob_oneshot(void)
{
    sofab_ostream_t os;
    sofab_ostream_init(&os, blob_buf, sizeof blob_buf, 0, NULL, NULL);
    sofab_ostream_write_blob(&os, 1, blob_src, BLOB_LEN);
    blob_used = sofab_ostream_bytes_used(&os);
}

/* The same bytes through a 4096-byte buffer with a flush sink — ~245 flushes.
 * Pass-through is not granted (this port does not implement it at all), so the
 * row measures the copy path, as the required row must on every port. */
__attribute__((noinline)) void run_encode_blob_streaming(void)
{
    sofab_ostream_t os;
    blob_streamed = 0;
    sofab_ostream_init(&os, blob_chunk_buf, sizeof blob_chunk_buf, 0, blob_sink, NULL);
    sofab_ostream_write_blob(&os, 1, blob_src, BLOB_LEN);
    sofab_ostream_flush(&os);
}

static void cb_blob(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)count;
    (void)usr;
    if (id == 1)
        sofab_istream_read_blob(ctx, blob_dec, sizeof blob_dec);
}

/* Fed in 4096-byte chunks, so the payload straddles every chunk boundary. */
__attribute__((noinline)) void run_decode_blob(void)
{
    sofab_istream_t is;
    sofab_istream_init(&is, cb_blob, NULL);
    for (size_t off = 0; off < blob_used; off += BLOB_CHUNK) {
        size_t n = blob_used - off;
        if (n > BLOB_CHUNK)
            n = BLOB_CHUNK;
        sofab_istream_feed(&is, blob_buf + off, n);
    }
}

__attribute__((noinline)) void run_encode_composite(void)
{
    sofab_ostream_t os;
    sofab_ostream_init(&os, comp_buf, sizeof comp_buf, 0, NULL, NULL);
    encode_composite(&os);
    comp_used = sofab_ostream_bytes_used(&os);
}

/* Wrapper-array elements: the child id is the array index, so each element is
 * placed at C.items[id] rather than appended (§5.1). */
static void cb_comp_items(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)count;
    (void)usr;
    if (id < COMP_ITEMS)
        sofab_istream_read_string(ctx, C.items[id], COMP_ITEM_MAX);
}

static void cb_comp_l3(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)count;
    (void)usr;
    if (id == 1)
        sofab_istream_read_u32(ctx, &C.deep);
}

static void cb_comp_l2(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)count;
    (void)usr;
    if (id == 1)
        sofab_istream_read_sequence(ctx, &comp_dec_l3, cb_comp_l3, NULL);
}

static void cb_comp_l1(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)count;
    (void)usr;
    if (id == 1)
        sofab_istream_read_sequence(ctx, &comp_dec_l2, cb_comp_l2, NULL);
    else if (id == 2)
        sofab_istream_read_i32(ctx, &C.tail);
}

static void cb_composite(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)size;
    (void)count;
    (void)usr;
    switch (id)
    {
        case 1:   sofab_istream_read_sequence(ctx, &comp_dec_items, cb_comp_items, NULL); break;
        case 2:   sofab_istream_read_string(ctx, C.text, sizeof C.text); break;
        case 3:   sofab_istream_read_sequence(ctx, &comp_dec_l1, cb_comp_l1, NULL); break;
        case 130: sofab_istream_read_u32(ctx, &C.f130); break;
        default: break;
    }
}

__attribute__((noinline)) void run_decode_composite(void)
{
    sofab_istream_t is;
    sofab_istream_init(&is, cb_composite, NULL);
    sofab_istream_feed(&is, comp_buf, comp_used);
}

/* Binding nothing skips the field, and a sequence never bound is skipped whole
 * — so this walks the identical bytes and materializes nothing, which is what a
 * router or filter runs in production. Its distance from run_decode_composite()
 * is what not-decoding is worth. */
static void cb_composite_skip(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)ctx;
    (void)id;
    (void)size;
    (void)count;
    (void)usr;
}

__attribute__((noinline)) void run_decode_composite_skip(void)
{
    sofab_istream_t is;
    sofab_istream_init(&is, cb_composite_skip, NULL);
    sofab_istream_feed(&is, comp_buf, comp_used);
}

/* ---- measurement (process CPU time) -------------------------------------- */

static double cpu_now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

/* Block size for the timed loop: enough iterations that one clock reading is a
 * rounding error against them. clock() is a real cost — on a host without a
 * vDSO fast path for CLOCK_PROCESS_CPUTIME_ID it runs to about a microsecond,
 * which is more than an entire `typical message` operation — so reading it once
 * per iteration, as this loop used to, measures mostly the clock. Worse, it is
 * a fixed cost per operation rather than a scaling factor, so it distorts the
 * workloads unevenly: barely visible on a 1000-element array, dominant on a
 * 37-byte message. BENCH_SPEC asks for a ~1 s CPU-time loop, a warmup and a
 * given MB/s formula; how often the clock is sampled inside that loop is ours
 * to choose, and the printed output is unchanged. */
#define BENCH_BLOCK_SECONDS 0.01 /* clock cost lands under ~0.01% of a block */

static long calibrate_block(void (*fn)(void))
{
    long block = 1;
    for (;; block *= 2) {
        double t0 = cpu_now();
        for (long k = 0; k < block; k++)
            fn();
        if (cpu_now() - t0 >= BENCH_BLOCK_SECONDS)
            return block;
    }
}

static double measure(void (*fn)(void), size_t bytes)
{
    fn(); /* warmup */
    long   block = calibrate_block(fn);
    double t0 = cpu_now();
    long   it = 0;
    double el;
    do {
        for (long k = 0; k < block; k++)
            fn();
        it += block;
        el = cpu_now() - t0;
    } while (el < 1.0);
    return (double)bytes * (double)it / el / 1e6; /* MB/s, MB = 1e6 bytes */
}

/* ---- single-shot mode (one operation, for Callgrind instruction counts) -- */

/* FNV-1a over the encoded message. The C and C++ tools must agree on it for
 * every workload — identical ids and values are supposed to produce identical
 * bytes, and the C-vs-C++ instruction table is only meaningful if they do. */
static uint32_t fnv1a(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static int run_one(const char *w)
{
    size_t bytes;

    make_src();
    make_blob();
    make_composite();

    if (!strcmp(w, "encode_u64_array")) {
        run_encode_u64_array();
        bytes = enc_u64_used;
    } else if (!strcmp(w, "encode_typical")) {
        run_encode_typical();
        bytes = typ_used;
    } else if (!strcmp(w, "encode_blob_oneshot")) {
        run_encode_blob_oneshot();
        bytes = blob_used;
    } else if (!strcmp(w, "encode_blob_streaming")) {
        run_encode_blob_streaming();
        bytes = blob_streamed;
    } else if (!strcmp(w, "encode_composite")) {
        run_encode_composite();
        bytes = comp_used;
    } else if (!strcmp(w, "decode_u64_array")) {
        run_encode_u64_array();          /* setup (excluded from collection) */
        run_decode_u64_array();
        bytes = enc_u64_used;
    } else if (!strcmp(w, "decode_typical")) {
        run_encode_typical();            /* setup (excluded from collection) */
        run_decode_typical();
        bytes = typ_used;
    } else if (!strcmp(w, "decode_blob")) {
        run_encode_blob_oneshot();       /* setup (excluded from collection) */
        run_decode_blob();
        bytes = blob_used;
    } else if (!strcmp(w, "decode_composite")) {
        run_encode_composite();          /* setup (excluded from collection) */
        run_decode_composite();
        bytes = comp_used;
    } else if (!strcmp(w, "decode_composite_skip")) {
        run_encode_composite();          /* setup (excluded from collection) */
        run_decode_composite_skip();
        bytes = comp_used;
    } else {
        fprintf(stderr, "unknown workload: %s\n", w);
        return 1;
    }

    /* observe outputs so nothing is optimized away, and report the message byte
     * count (the harness's `bytes` column) */
    fprintf(stderr,
            "arr0=%llu f1=%u s_f2=%d str=%s blob0=%u xor=%u item63=%s deep=%u "
            "csum=%08x BYTES=%zu\n",
            (unsigned long long)dec_array[0], T.f1, T.s_f2, T.f5,
            blob_dec[0], blob_sink_xor, C.items[COMP_ITEMS - 1], C.deep,
            fnv1a(comp_buf, comp_used), bytes);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2)
        return run_one(argv[1]);

    make_src();
    make_blob();
    make_composite();
    run_encode_u64_array();
    run_encode_typical();
    run_encode_blob_oneshot();
    run_encode_blob_streaming();
    run_encode_composite();
    size_t ba = enc_u64_used, bt = typ_used, bb = blob_used, bc = comp_used;

    /* Parity checks: a port whose encoding diverges prints a different size
     * (BENCH_SPEC). The composite message has no published reference size yet,
     * so only the two fixed ones are asserted here. */
    if (bb != BLOB_ENCODED || blob_streamed != BLOB_ENCODED) {
        fprintf(stderr,
                "bench: blob 1MB encodes to %zu bytes one-shot and %zu streamed, "
                "BENCH_SPEC requires %d for both\n",
                bb, blob_streamed, BLOB_ENCODED);
        return 1;
    }

    /* Sanity check that the two new decodes actually reproduce the data, so a
     * silently-skipping decoder cannot post a fast row. Outside the timed loop. */
    run_decode_blob();
    run_decode_composite();
    if (memcmp(blob_dec, blob_src, BLOB_LEN) != 0
        || strcmp(C.items[0], "item-0") != 0
        || strcmp(C.items[COMP_ITEMS - 1], "item-63") != 0
        || memcmp(C.text, COMP_TEXT, COMP_TEXT_LEN) != 0
        || C.deep != 7 || C.tail != -1 || C.f130 != 0xDEADBEEFu) {
        fprintf(stderr, "bench: decode self-check failed\n");
        return 1;
    }

    printf("=== SofaBuffers C throughput (CPU time, MB/s) ===\n");
    printf("%-26s %12s\n", "Workload", "MB/s");
    printf("%-26s %12s\n", "--------", "----");
    printf("%-26s %12.2f\n", "encode: u64 array (1000)",   measure(run_encode_u64_array, ba));
    printf("%-26s %12.2f\n", "encode: typical message",    measure(run_encode_typical, bt));
    printf("%-26s %12.2f\n", "encode: blob 1MB one-shot",  measure(run_encode_blob_oneshot, bb));
    printf("%-26s %12.2f\n", "encode: blob 1MB streaming", measure(run_encode_blob_streaming, bb));
    printf("%-26s %12.2f\n", "encode: composite",          measure(run_encode_composite, bc));
    printf("%-26s %12.2f\n", "decode: u64 array (1000)",   measure(run_decode_u64_array, ba));
    printf("%-26s %12.2f\n", "decode: typical message",    measure(run_decode_typical, bt));
    printf("%-26s %12.2f\n", "decode: blob 1MB",           measure(run_decode_blob, bb));
    printf("%-26s %12.2f\n", "decode: composite",          measure(run_decode_composite, bc));
    printf("%-26s %12.2f\n", "decode: composite skip-all", measure(run_decode_composite_skip, bc));
    printf("\nMB = 1e6 bytes. ~1s CPU-time loop per workload.\n");
    return 0;
}
