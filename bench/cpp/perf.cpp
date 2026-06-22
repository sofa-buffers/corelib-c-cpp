/*!
 * @file perf.cpp
 * @brief SofaBuffers C++ — combined per-operation cost benchmark.
 *
 * Mirror of bench/c/perf.c: encodes/decodes the identical message (same field
 * ids, types and values) through the header-only C++ wrapper (sofab.hpp) and
 * prints the identical report, so the C and C++ implementations can be compared
 * directly. Two complementary metrics per workload:
 *
 *   1. CPU cycles/op  -- cost of the code itself, read off the hardware cycle
 *      counter (x86 TSC / AArch64 virtual count register). Tracks code changes
 *      rather than the host's clock speed.
 *
 *   2. Throughput MB/s -- a "speedtest" for this machine, derived from process
 *      CPU time (std::clock(), not wall-clock). MB = 1e6 bytes.
 *
 * Both metrics are gathered over the same adaptive ~1 s CPU-time loop, so they
 * describe the exact same work.
 *
 * Thin subclasses expose the protected ctx_/buffer_ so the streams drive a
 * caller-owned buffer (no per-iteration allocation or zeroing), exactly like
 * the C benchmark.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <span>
#include <string>

/*****************************************************************************/
/* hardware cycle counter (same as the C benchmark)                          */
/*****************************************************************************/
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define PERF_HAVE_CYCLES 1
static inline uint64_t perf_cycles()
{
    return (uint64_t)__rdtsc();
}
#elif defined(__aarch64__)
#define PERF_HAVE_CYCLES 1
static inline uint64_t perf_cycles()
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}
#else
#define PERF_HAVE_CYCLES 0
static inline uint64_t perf_cycles()
{
    return 0;
}
#endif

namespace
{

#define PERF_STRING "perf-benchmark-message"

const uint32_t perf_samples[8] =
    {1000000u, 2000000u, 3000000u, 4000000u, 5000000u, 6000000u, 7000000u, 8000000u};
const int32_t perf_deltas[8] =
    {-100000, -200000, -300000, -400000, -500000, -600000, -700000, -800000};
const double perf_fp64[4] =
    {3.14159265, 6.28318530, 9.42477795, 12.56637060};

/* Streams that init over a caller-owned raw buffer (like the C API). */
class OStreamRaw : public sofab::OStreamImpl
{
public:
    void init(uint8_t *b, size_t n) noexcept
    {
        buffer_ = b;
        sofab_ostream_init(&ctx_, b, n, 0, nullptr, nullptr);
    }
};

class IStreamRaw : public sofab::IStreamImpl
{
public:
    void init(sofab_istream_field_cb_t cb, void *usr) noexcept
    {
        sofab_istream_init(&ctx_, cb, usr);
    }
    sofab_istream_t *ctx() noexcept { return &ctx_; }
};

size_t perf_encode(uint8_t *buf, size_t buflen)
{
    OStreamRaw os;
    os.init(buf, buflen);

    os.write(1, static_cast<uint32_t>(0xDEADBEEFu));
    os.write(2, static_cast<int32_t>(-12345));
    os.write(3, static_cast<uint64_t>(0x0123456789ABCDEFull));
    os.write(4, static_cast<int64_t>(-5000000000000ll));
    os.write(5, true);
    os.write(6, 3.14159f);
    os.write(7, 2.718281828459045);
    os.write(8, PERF_STRING);
    os.write(9, std::span<const uint32_t>(perf_samples, 8));
    os.write(10, std::span<const int32_t>(perf_deltas, 8));
    os.write(11, std::span<const double>(perf_fp64, 4));
    os.sequenceBegin(12);
    os.write(1, static_cast<uint32_t>(99));
    os.write(2, static_cast<int32_t>(-7));
    os.sequenceEnd();

    return os.bytesUsed();
}

struct PerfOut
{
    uint32_t u32 = 0;
    int32_t  i32 = 0;
    uint64_t u64 = 0;
    int64_t  i64 = 0;
    bool     b = false;
    float    f32 = 0.0f;
    double   f64 = 0.0;
    std::string str;
    uint32_t samples[8] = {};
    int32_t  deltas[8] = {};
    double   fp64[4] = {};
    uint32_t s_u32 = 0;
    int32_t  s_i32 = 0;
};

struct DecCtx
{
    IStreamRaw &is;
    PerfOut    &out;
};

sofab_istream_decoder_t perf_child_dec;

void perf_child_cb(sofab_istream_t *, sofab_id_t id, size_t, size_t, void *usr)
{
    auto *c = static_cast<DecCtx *>(usr);
    if (id == 1)
        c->is.read(c->out.s_u32);
    else if (id == 2)
        c->is.read(c->out.s_i32);
}

void perf_field_cb(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    auto *c = static_cast<DecCtx *>(usr);
    auto &is = c->is;
    auto &o  = c->out;

    switch (id)
    {
        case 1:  is.read(o.u32); break;
        case 2:  is.read(o.i32); break;
        case 3:  is.read(o.u64); break;
        case 4:  is.read(o.i64); break;
        case 5:  is.read(o.b); break;
        case 6:  is.read(o.f32); break;
        case 7:  is.read(o.f64); break;
        case 8:  o.str.resize(size); is.read(o.str); break;
        case 9:  { std::span<uint32_t> s(o.samples, count); is.read(s); } break;
        case 10: { std::span<int32_t> s(o.deltas, count); is.read(s); } break;
        case 11: { std::span<double> s(o.fp64, count); is.read(s); } break;
        case 12: sofab_istream_read_sequence(ctx, &perf_child_dec, perf_child_cb, usr); break;
        default: break;
    }
}

void perf_decode(const uint8_t *buf, size_t len, PerfOut &out)
{
    IStreamRaw is;
    DecCtx ctx{is, out};
    is.init(perf_field_cb, &ctx);
    is.feed(buf, len);
}

/*****************************************************************************/
/* measurement                                                               */
/*****************************************************************************/

struct PerfResult
{
    unsigned long iters;
    double        cycles_op; /* hardware cycles per operation */
    double        ns_op;     /* CPU nanoseconds per operation */
    double        mb_s;      /* throughput, MB/s (MB = 1e6 bytes) */
};

double cpu_now()
{
    return (double)std::clock() / (double)CLOCKS_PER_SEC;
}

void perf_report(const char *what, PerfResult r, size_t bytes)
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

PerfResult measure_encode(uint8_t *buf, size_t buflen, size_t &msg_size)
{
    volatile size_t sink = 0;
    size_t          msg  = 0;

    for (unsigned i = 0; i < 1000u; i++) /* warmup */
        msg = perf_encode(buf, buflen);
    msg_size = msg;

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

    return PerfResult{
        it,
        (double)(c1 - c0) / (double)it,
        el / (double)it * 1e9,
        (double)msg * (double)it / el / 1e6,
    };
}

PerfResult measure_decode(const uint8_t *buf, size_t len, PerfOut &out)
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
        sink += out.u32;
        it++;
        el = cpu_now() - t0;
    } while (el < 1.0);
    uint64_t c1 = perf_cycles();
    (void)sink;

    return PerfResult{
        it,
        (double)(c1 - c0) / (double)it,
        el / (double)it * 1e9,
        (double)len * (double)it / el / 1e6,
    };
}

} // namespace

int main()
{
    uint8_t buffer[512];
    size_t  msg_size = 0;

    printf("=== SofaBuffers C++ per-op cost (cycles/op + throughput MB/s) ===\n");

    PerfResult enc = measure_encode(buffer, sizeof buffer, msg_size);
    perf_report("serialize (stream API)", enc, msg_size);

    PerfOut out;
    out.str.resize(32); /* presize so the in-loop resize never reallocates */
    perf_decode(buffer, msg_size, out);
    /* sanity check that the decode actually reproduced the data */
    if (out.u32 != 0xDEADBEEFu || out.str != PERF_STRING)
    {
        fprintf(stderr, "perf: decode self-check failed\n");
        return 1;
    }

    PerfResult dec = measure_decode(buffer, msg_size, out);
    perf_report("deserialize (stream API)", dec, msg_size);

    printf("\ncycles/op tracks code cost; MB/s is this machine's throughput.\n");
    return 0;
}
