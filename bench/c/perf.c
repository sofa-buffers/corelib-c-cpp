/*!
 * @file perf.c
 * @brief SofaBuffers C — combined per-operation cost benchmark.
 *
 * Standalone executable reporting two complementary metrics for a single
 * representative message (scalars of every width, signed/unsigned integer
 * arrays, a float array, a string and a nested sequence), serialized and
 * deserialized through the low-level stream API:
 *
 *   1. CPU cycles/op  -- a value to judge the *cost of the code itself*. Read
 *      straight off the hardware cycle counter (x86 TSC / AArch64 virtual count
 *      register). Largely independent of the wall clock, so it tracks code
 *      changes rather than how fast the host happens to be clocked.
 *
 *   2. Throughput MB/s -- a "speedtest" value for *this machine*: how many
 *      message-bytes per second the encoder/decoder sustains. Derived from
 *      process CPU time (clock(), not wall-clock), MB = 1e6 bytes.
 *
 * Both metrics are gathered over the same adaptive ~1 s CPU-time loop, so they
 * describe the exact same work. The C and C++ perf tools encode the identical
 * message and print the identical report, so the two can be compared directly.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/istream.h"
#include "sofab/ostream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*****************************************************************************/
/* hardware cycle counter                                                    */
/*****************************************************************************/
/*
 * Counts work (CPU cycles), not seconds, so it is a machine-speed-independent
 * companion to the CPU-time figure. On x86 the time-stamp counter, on AArch64
 * the virtual count register; elsewhere it is unavailable and reported as such.
 */
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define PERF_HAVE_CYCLES 1
static inline uint64_t perf_cycles(void)
{
    return (uint64_t)__rdtsc();
}
#elif defined(__aarch64__)
#define PERF_HAVE_CYCLES 1
static inline uint64_t perf_cycles(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}
#else
#define PERF_HAVE_CYCLES 0
static inline uint64_t perf_cycles(void)
{
    return 0;
}
#endif

/*****************************************************************************/
/* message under test                                                        */
/*****************************************************************************/

#define PERF_STRING "perf-benchmark-message"

static const uint32_t perf_samples[8] =
    {1000000u, 2000000u, 3000000u, 4000000u, 5000000u, 6000000u, 7000000u, 8000000u};
static const int32_t perf_deltas[8] =
    {-100000, -200000, -300000, -400000, -500000, -600000, -700000, -800000};
static const double perf_fp64[4] =
    {3.14159265, 6.28318530, 9.42477795, 12.56637060};

static size_t perf_encode(uint8_t *buf, size_t buflen)
{
    sofab_ostream_t os;
    sofab_ostream_init(&os, buf, buflen, 0, NULL, NULL);

    sofab_ostream_write_unsigned(&os, 1, 0xDEADBEEFu);
    sofab_ostream_write_signed(&os, 2, -12345);
    sofab_ostream_write_unsigned(&os, 3, 0x0123456789ABCDEFull);
    sofab_ostream_write_signed(&os, 4, -5000000000000ll);
    sofab_ostream_write_boolean(&os, 5, true);
    sofab_ostream_write_fp32(&os, 6, 3.14159f);
    sofab_ostream_write_fp64(&os, 7, 2.718281828459045);
    sofab_ostream_write_string(&os, 8, PERF_STRING);
    sofab_ostream_write_array_of_u32(&os, 9, perf_samples, 8);
    sofab_ostream_write_array_of_i32(&os, 10, perf_deltas, 8);
    sofab_ostream_write_array_of_fp64(&os, 11, perf_fp64, 4);
    sofab_ostream_write_sequence_begin(&os, 12);
    sofab_ostream_write_unsigned(&os, 1, 99);
    sofab_ostream_write_signed(&os, 2, -7);
    sofab_ostream_write_sequence_end(&os);

    return sofab_ostream_bytes_used(&os);
}

typedef struct
{
    uint32_t u32;
    int32_t  i32;
    uint64_t u64;
    int64_t  i64;
    bool     b;
    float    f32;
    double   f64;
    char     str[32];
    uint32_t samples[8];
    int32_t  deltas[8];
    double   fp64[4];
    uint32_t s_u32;
    int32_t  s_i32;
} perf_out_t;

static sofab_istream_decoder_t perf_child_dec;

static void perf_child_cb(
    sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    perf_out_t *o = (perf_out_t *)usr;
    (void)size;
    (void)count;

    if (id == 1)
        sofab_istream_read_u32(ctx, &o->s_u32);
    else if (id == 2)
        sofab_istream_read_i32(ctx, &o->s_i32);
}

static void perf_field_cb(
    sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    perf_out_t *o = (perf_out_t *)usr;
    (void)size;

    switch (id)
    {
        case 1:  sofab_istream_read_u32(ctx, &o->u32); break;
        case 2:  sofab_istream_read_i32(ctx, &o->i32); break;
        case 3:  sofab_istream_read_u64(ctx, &o->u64); break;
        case 4:  sofab_istream_read_i64(ctx, &o->i64); break;
        case 5:  sofab_istream_read_bool(ctx, &o->b); break;
        case 6:  sofab_istream_read_fp32(ctx, &o->f32); break;
        case 7:  sofab_istream_read_fp64(ctx, &o->f64); break;
        case 8:  sofab_istream_read_string(ctx, o->str, sizeof o->str); break;
        case 9:  sofab_istream_read_array_of_u32(ctx, o->samples, count); break;
        case 10: sofab_istream_read_array_of_i32(ctx, o->deltas, count); break;
        case 11: sofab_istream_read_array_of_fp64(ctx, o->fp64, count); break;
        case 12: sofab_istream_read_sequence(ctx, &perf_child_dec, perf_child_cb, o); break;
        default: break;
    }
}

static void perf_decode(const uint8_t *buf, size_t len, perf_out_t *o)
{
    sofab_istream_t is;
    sofab_istream_init(&is, perf_field_cb, o);
    sofab_istream_feed(&is, buf, len);
}

/*****************************************************************************/
/* measurement                                                               */
/*****************************************************************************/

typedef struct
{
    unsigned long iters;
    double        cycles_op; /* hardware cycles per operation */
    double        ns_op;     /* CPU nanoseconds per operation */
    double        mb_s;      /* throughput, MB/s (MB = 1e6 bytes) */
} perf_result_t;

static double cpu_now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static void perf_report(const char *what, perf_result_t r, size_t bytes)
{
    printf("\n--- perf: %s ---\n", what);
    printf("  iterations    : %lu\n", r.iters);
    printf("  message size  : %zu bytes\n", bytes);
#if PERF_HAVE_CYCLES
    printf("  cycles/op     : %.1f  (hardware cycle counter)\n", r.cycles_op);
#else
    (void)r.cycles_op;
    printf("  cycles/op     : (cycle counter unavailable on this arch)\n");
#endif
    printf("  CPU time/op   : %.1f ns  (process CPU time, not wall-clock)\n", r.ns_op);
    printf("  throughput    : %.1f MB/s  (speedtest, MB = 1e6 bytes)\n", r.mb_s);
}

static perf_result_t measure_encode(uint8_t *buf, size_t buflen, size_t *msg_size)
{
    volatile size_t sink = 0;
    size_t          msg  = 0;

    for (unsigned i = 0; i < 1000u; i++) /* warmup */
        msg = perf_encode(buf, buflen);
    *msg_size = msg;

    unsigned long it = 0;
    double        el;
    uint64_t      c0 = perf_cycles();
    double        t0 = cpu_now();
    do {
        sink += perf_encode(buf, buflen);
        it++;
        el = cpu_now() - t0;
    } while (el < 1.0);
    uint64_t c1 = perf_cycles();
    (void)sink;

    perf_result_t r;
    r.iters     = it;
    r.cycles_op = (double)(c1 - c0) / (double)it;
    r.ns_op     = el / (double)it * 1e9;
    r.mb_s      = (double)msg * (double)it / el / 1e6;
    return r;
}

static perf_result_t measure_decode(const uint8_t *buf, size_t len, perf_out_t *out)
{
    volatile uint32_t sink = 0;

    for (unsigned i = 0; i < 1000u; i++) /* warmup */
        perf_decode(buf, len, out);

    unsigned long it = 0;
    double        el;
    uint64_t      c0 = perf_cycles();
    double        t0 = cpu_now();
    do {
        perf_decode(buf, len, out);
        sink += out->u32;
        it++;
        el = cpu_now() - t0;
    } while (el < 1.0);
    uint64_t c1 = perf_cycles();
    (void)sink;

    perf_result_t r;
    r.iters     = it;
    r.cycles_op = (double)(c1 - c0) / (double)it;
    r.ns_op     = el / (double)it * 1e9;
    r.mb_s      = (double)len * (double)it / el / 1e6;
    return r;
}

int main(void)
{
    uint8_t buffer[512];
    size_t  msg_size = 0;

    printf("=== SofaBuffers C per-op cost (cycles/op + throughput MB/s) ===\n");

    perf_result_t enc = measure_encode(buffer, sizeof buffer, &msg_size);
    perf_report("serialize (stream API)", enc, msg_size);

    perf_out_t out;
    perf_decode(buffer, msg_size, &out);
    /* sanity check that the decode actually reproduced the data */
    if (out.u32 != 0xDEADBEEFu || strcmp(out.str, PERF_STRING) != 0)
    {
        fprintf(stderr, "perf: decode self-check failed\n");
        return 1;
    }

    perf_result_t dec = measure_decode(buffer, msg_size, &out);
    perf_report("deserialize (stream API)", dec, msg_size);

    printf("\ncycles/op tracks code cost; MB/s is this machine's throughput.\n");
    return 0;
}
