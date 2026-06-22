/*!
 * @file bench.c
 * @brief SofaBuffers C — throughput benchmark (MB/s, CPU time).
 *
 * Standalone benchmark reporting encode/decode throughput for two workloads:
 * a 1000-element u64 array and a small "typical" mixed message. Each workload
 * runs in a ~1 second loop and reports MB/s.
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

/* shared buffers (static => not part of the measured stack work) */
static uint64_t src[N];
static uint8_t  enc_u64_buf[N * 11 + 16];
static size_t   enc_u64_used;

static uint8_t  typ_buf[256];
static size_t   typ_used;
static const uint16_t arr16[4] = {10, 20, 30, 40};

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

static void make_src(void)
{
    for (int i = 0; i < N; i++)
        src[i] = (uint64_t)i * 0x9E3779B97F4A7C15ULL;
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

/* ---- measurement (process CPU time) -------------------------------------- */

static double cpu_now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static double measure(void (*fn)(void), size_t bytes)
{
    fn(); /* warmup */
    double t0 = cpu_now();
    long   it = 0;
    double el;
    do {
        fn();
        it++;
        el = cpu_now() - t0;
    } while (el < 1.0);
    return (double)bytes * (double)it / el / 1e6; /* MB/s, MB = 1e6 bytes */
}

/* ---- single-shot mode (one operation, for Callgrind instruction counts) -- */

static int run_one(const char *w)
{
    make_src();
    if (!strcmp(w, "encode_u64_array")) {
        run_encode_u64_array();
    } else if (!strcmp(w, "encode_typical")) {
        run_encode_typical();
    } else if (!strcmp(w, "decode_u64_array")) {
        run_encode_u64_array();          /* setup (excluded from collection) */
        run_decode_u64_array();
    } else if (!strcmp(w, "decode_typical")) {
        run_encode_typical();            /* setup (excluded from collection) */
        run_decode_typical();
    } else {
        fprintf(stderr, "unknown workload: %s\n", w);
        return 1;
    }

    /* report the message byte count so the harness can derive instr/byte */
    size_t bytes = (!strcmp(w, "encode_u64_array") || !strcmp(w, "decode_u64_array"))
                       ? enc_u64_used : typ_used;
    /* observe outputs so nothing is optimized away */
    fprintf(stderr, "arr0=%llu f1=%u s_f2=%d str=%s BYTES=%zu\n",
            (unsigned long long)dec_array[0], T.f1, T.s_f2, T.f5, bytes);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2)
        return run_one(argv[1]);

    make_src();
    run_encode_u64_array();
    run_encode_typical();
    size_t ba = enc_u64_used, bt = typ_used;

    printf("=== SofaBuffers C throughput (CPU time, MB/s) ===\n");
    printf("%-26s %12s\n", "Workload", "MB/s");
    printf("%-26s %12s\n", "--------", "----");
    printf("%-26s %12.2f\n", "encode: u64 array (1000)", measure(run_encode_u64_array, ba));
    printf("%-26s %12.2f\n", "encode: typical message",  measure(run_encode_typical, bt));
    printf("%-26s %12.2f\n", "decode: u64 array (1000)", measure(run_decode_u64_array, ba));
    printf("%-26s %12.2f\n", "decode: typical message",  measure(run_decode_typical, bt));
    printf("\nMB = 1e6 bytes. ~1s CPU-time loop per workload.\n");
    return 0;
}
