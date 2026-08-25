/*!
 * @file bench.cpp
 * @brief SofaBuffers C++ — throughput benchmark (MB/s, CPU time).
 *
 * Mirror of bench/c/bench.c: same workloads, data, ids and values, driven
 * through the header-only C++ wrapper (sofab.hpp), so the figures are directly
 * comparable. The wrapper's write()/read() templates inline to the same C
 * corelib calls; this benchmark measures whatever overhead those abstractions
 * add.
 *
 * Throughput is measured against *process CPU time* (std::clock(), not
 * wall-clock), so it reflects the cost of the implementation, not OS scheduling
 * or the machine clock. MB = 1e6 bytes.
 *
 * Two modes:
 *   bench_cpp              -> timed MB/s table (default, CPU time).
 *   bench_cpp <workload>   -> run one operation once and exit; used by
 *                            run_callgrind.sh to count instructions/op under
 *                            Callgrind (a machine-independent metric). The
 *                            run_<workload> functions are extern "C" + noinline
 *                            so --toggle-collect=run_<workload> matches the same
 *                            symbol names as the C benchmark.
 *
 * Thin subclasses expose the protected ctx_/buffer_ so the streams drive a
 * caller-owned static buffer (no heap allocation in the measured region),
 * exactly like the C benchmark.
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

#define N 1000

/* See bench/c/bench.c for what each dataset is for; the constants and values
 * are the same by construction, since the two ports must encode identical
 * bytes. */
#define BLOB_LEN     1000000
#define BLOB_ENCODED 1000005
#define BLOB_CHUNK   4096

#define COMP_ITEMS    64
#define COMP_ITEM_MAX 12

#define COMP_GLYPHS "a\xC3\xA4\xE2\x82\xAC\xF0\x9D\x84\x9E"
#define COMP_X2(s)  s s
#define COMP_X4(s)  s s s s
#define COMP_TEXT   COMP_X2(COMP_X4(COMP_X4(COMP_GLYPHS)))
#define COMP_TEXT_LEN (sizeof(COMP_TEXT) - 1)

namespace
{

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

/* shared buffers */
uint64_t src[N];
uint8_t  enc_u64_buf[N * 11 + 16];
size_t   enc_u64_used;

uint8_t  typ_buf[256];
size_t   typ_used;
const uint16_t arr16[4] = {10, 20, 30, 40};

uint8_t  blob_src[BLOB_LEN];
uint8_t  blob_buf[BLOB_ENCODED];
uint8_t  blob_chunk_buf[BLOB_CHUNK];
size_t   blob_used;
size_t   blob_streamed;
uint8_t  blob_sink_xor;
uint8_t  blob_dec[BLOB_LEN];

char     comp_items[COMP_ITEMS][COMP_ITEM_MAX];
uint8_t  comp_buf[2048];
size_t   comp_used;

/* decode targets */
uint64_t dec_array[N];
struct Targets
{
    uint32_t f1;
    int32_t  f2;
    bool     f3;
    float    f4;
    std::string f5;
    uint16_t f6[4];
    uint32_t s_f1;
    int32_t  s_f2;
} T;
sofab_istream_decoder_t child_dec;

struct CompTargets
{
    std::string items[COMP_ITEMS];
    std::string text;
    uint32_t    deep;
    int32_t     tail;
    uint32_t    f130;
} C;
sofab_istream_decoder_t comp_dec_items, comp_dec_l1, comp_dec_l2, comp_dec_l3;

void make_src()
{
    for (int i = 0; i < N; i++)
        src[i] = (uint64_t)i * 0x9E3779B97F4A7C15ULL;
}

void make_blob()
{
    for (size_t i = 0; i < BLOB_LEN; i++)
        blob_src[i] = (uint8_t)((uint64_t)i * 0x9E3779B97F4A7C15ULL);
}

void make_composite()
{
    for (int i = 0; i < COMP_ITEMS; i++)
        snprintf(comp_items[i], sizeof comp_items[i], "item-%d", i);
}

void encode_typical(OStreamRaw &os)
{
    os.write(1, static_cast<uint32_t>(0xDEADBEEF));
    os.write(2, static_cast<int32_t>(-12345));
    os.write(3, true);
    os.write(4, 3.14159f);
    os.write(5, "sofab");
    os.write(6, std::span<const uint16_t>(arr16, 4));
    os.sequenceBeginLazy(7);
    os.write(1, static_cast<uint32_t>(99));
    os.write(2, static_cast<int32_t>(-7));
    os.sequenceEnd();
}

/* Field-by-field mirror of encode_composite() in bench/c/bench.c — see there for
 * what each field exercises. Same ids, same values, same bytes. */
void encode_composite(sofab::OStreamImpl &os)
{
    os.sequenceBeginLazy(1); /* wrapper array: child id == array index (§5.1) */
    for (int i = 0; i < COMP_ITEMS; i++)
        os.write(static_cast<sofab_id_t>(i), comp_items[i]);
    os.sequenceEnd();

    os.write(2, COMP_TEXT);

    os.sequenceBeginLazy(3);
    os.sequenceBeginLazy(1);
    os.sequenceBeginLazy(1);
    os.write(1, static_cast<uint32_t>(7));
    os.sequenceEnd();
    os.sequenceEnd();
    os.write(2, static_cast<int32_t>(-1));
    os.sequenceEnd();

    /* omitted: opened lazily, closed empty (§2) */
    os.sequenceBeginLazy(4);
    os.sequenceEnd();

    os.write(130, static_cast<uint32_t>(0xDEADBEEF));
}

void cb_array(sofab_istream_t *, sofab_id_t id, size_t, size_t count, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    if (id == 1)
    {
        auto sp = std::span<uint64_t>(dec_array, count);
        is->read(sp);
    }
}

void cb_child(sofab_istream_t *, sofab_id_t id, size_t, size_t, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    if (id == 1)
        is->read(T.s_f1);
    else if (id == 2)
        is->read(T.s_f2);
}

void cb_blob(sofab_istream_t *, sofab_id_t id, size_t size, size_t, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    if (id == 1)
        is->read(blob_dec, size > BLOB_LEN ? BLOB_LEN : size);
}

void cb_comp_items(sofab_istream_t *, sofab_id_t id, size_t size, size_t, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    if (id < COMP_ITEMS)
    {
        auto &s = C.items[id];
        s.resize(size); /* capacity is reserved in main(), so no allocation here */
        is->read(s);
    }
}

void cb_comp_l3(sofab_istream_t *, sofab_id_t id, size_t, size_t, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    if (id == 1)
        is->read(C.deep);
}

void cb_comp_l2(sofab_istream_t *ctx, sofab_id_t id, size_t, size_t, void *usr)
{
    if (id == 1)
        sofab_istream_read_sequence(ctx, &comp_dec_l3, cb_comp_l3, usr);
}

void cb_comp_l1(sofab_istream_t *ctx, sofab_id_t id, size_t, size_t, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    if (id == 1)
        sofab_istream_read_sequence(ctx, &comp_dec_l2, cb_comp_l2, usr);
    else if (id == 2)
        is->read(C.tail);
}

void cb_composite(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    switch (id)
    {
        case 1:   sofab_istream_read_sequence(ctx, &comp_dec_items, cb_comp_items, usr); break;
        case 2:   C.text.resize(size); is->read(C.text); break;
        case 3:   sofab_istream_read_sequence(ctx, &comp_dec_l1, cb_comp_l1, usr); break;
        case 130: is->read(C.f130); break;
        default: break;
    }
}

/* Binds nothing, so every field — and every sub-sequence — is skipped. */
void cb_composite_skip(sofab_istream_t *, sofab_id_t, size_t, size_t, void *)
{
}

void cb_typical(sofab_istream_t *, sofab_id_t id, size_t, size_t, void *usr)
{
    auto *is = static_cast<IStreamRaw *>(usr);
    switch (id)
    {
        case 1: is->read(T.f1); break;
        case 2: is->read(T.f2); break;
        case 3: is->read(T.f3); break;
        case 4: is->read(T.f4); break;
        case 5: is->read(T.f5); break;
        case 6: { auto sp = std::span<uint16_t>(T.f6, 4); is->read(sp); break; }
        case 7: sofab_istream_read_sequence(is->ctx(), &child_dec, cb_child, usr); break;
        default: break;
    }
}

double cpu_now()
{
    return (double)std::clock() / (double)CLOCKS_PER_SEC;
}

/* Block size for the timed loop: enough iterations that one clock reading is a
 * rounding error against them. std::clock() is a real cost — on a host without
 * a vDSO fast path for CLOCK_PROCESS_CPUTIME_ID it runs to about a microsecond,
 * which is more than an entire `typical message` operation — so reading it once
 * per iteration, as this loop used to, measures mostly the clock. Worse, it is
 * a fixed cost per operation rather than a scaling factor, so it distorts the
 * workloads unevenly: barely visible on a 1000-element array, dominant on a
 * 37-byte message. BENCH_SPEC asks for a ~1 s CPU-time loop, a warmup and a
 * given MB/s formula; how often the clock is sampled inside that loop is ours
 * to choose, and the printed output is unchanged. */
constexpr double kBlockSeconds = 0.01; /* clock cost lands under ~0.01% of a block */

long calibrateBlock(void (*fn)())
{
    for (long block = 1;; block *= 2) {
        double t0 = cpu_now();
        for (long k = 0; k < block; k++)
            fn();
        if (cpu_now() - t0 >= kBlockSeconds)
            return block;
    }
}

double measure(void (*fn)(), size_t bytes)
{
    fn(); /* warmup */
    long   block = calibrateBlock(fn);
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

} // namespace

/* ---- workloads (extern "C" + noinline = stable toggle points) ------------ */

extern "C" __attribute__((noinline)) void run_encode_u64_array()
{
    OStreamRaw os;
    os.init(enc_u64_buf, sizeof enc_u64_buf);
    os.write(1, std::span<const uint64_t>(src, N));
    enc_u64_used = os.bytesUsed();
}

extern "C" __attribute__((noinline)) void run_encode_typical()
{
    OStreamRaw os;
    os.init(typ_buf, sizeof typ_buf);
    encode_typical(os);
    typ_used = os.bytesUsed();
}

extern "C" __attribute__((noinline)) void run_decode_u64_array()
{
    IStreamRaw is;
    is.init(cb_array, &is);
    is.feed(enc_u64_buf, enc_u64_used);
}

extern "C" __attribute__((noinline)) void run_decode_typical()
{
    IStreamRaw is;
    is.init(cb_typical, &is);
    is.feed(typ_buf, typ_used);
}

extern "C" __attribute__((noinline)) void run_encode_blob_oneshot()
{
    OStreamRaw os;
    os.init(blob_buf, sizeof blob_buf);
    os.write(1, blob_src, static_cast<int32_t>(BLOB_LEN));
    blob_used = os.bytesUsed();
}

/* The C++ port's streaming path is OStreamView over caller storage plus a
 * flushCallback, so that is what this row drives — measuring the wrapper's own
 * sink dispatch (a std::function call per flush) rather than re-measuring the C
 * path with C++ syntax. It is the only row where the two ports reach the sink by
 * different means, which is worth remembering when reading its C++/C ratio.
 * The sink itself obeys BENCH_SPEC: consume and discard, no accumulation. */
extern "C" __attribute__((noinline)) void run_encode_blob_streaming()
{
    blob_streamed = 0;
    sofab::OStreamView os(
        [](std::span<const uint8_t> chunk) noexcept {
            if (!chunk.empty())
                blob_sink_xor ^= chunk[0];
            blob_streamed += chunk.size();
        },
        blob_chunk_buf, sizeof blob_chunk_buf);
    os.write(1, blob_src, static_cast<int32_t>(BLOB_LEN));
    os.flush();
}

extern "C" __attribute__((noinline)) void run_decode_blob()
{
    IStreamRaw is;
    is.init(cb_blob, &is);
    for (size_t off = 0; off < blob_used; off += BLOB_CHUNK)
    {
        size_t n = blob_used - off;
        if (n > BLOB_CHUNK)
            n = BLOB_CHUNK;
        is.feed(blob_buf + off, n);
    }
}

extern "C" __attribute__((noinline)) void run_encode_composite()
{
    OStreamRaw os;
    os.init(comp_buf, sizeof comp_buf);
    encode_composite(os);
    comp_used = os.bytesUsed();
}

extern "C" __attribute__((noinline)) void run_decode_composite()
{
    IStreamRaw is;
    is.init(cb_composite, &is);
    is.feed(comp_buf, comp_used);
}

extern "C" __attribute__((noinline)) void run_decode_composite_skip()
{
    IStreamRaw is;
    is.init(cb_composite_skip, &is);
    is.feed(comp_buf, comp_used);
}

/* ---- single-shot mode (one operation, for Callgrind instruction counts) -- */

/* FNV-1a over the encoded message; must match bench_c's for every workload —
 * see the note there. */
static uint32_t fnv1a(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* String read buffers, presized outside the measured run so a resize() inside a
 * field callback never allocates. */
static void presize_targets()
{
    T.f5.resize(16);
    C.text.reserve(COMP_TEXT_LEN);
    for (int i = 0; i < COMP_ITEMS; i++)
        C.items[i].reserve(COMP_ITEM_MAX);
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

    fprintf(stderr,
            "arr0=%llu f1=%u s_f2=%d str=%.5s blob0=%u xor=%u item63=%s deep=%u "
            "csum=%08x BYTES=%zu\n",
            (unsigned long long)dec_array[0], T.f1, T.s_f2, T.f5.c_str(),
            blob_dec[0], blob_sink_xor, C.items[COMP_ITEMS - 1].c_str(), C.deep,
            fnv1a(comp_buf, comp_used), bytes);
    return 0;
}

int main(int argc, char **argv)
{
    presize_targets();

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

    if (bb != BLOB_ENCODED || blob_streamed != BLOB_ENCODED) {
        fprintf(stderr,
                "bench: blob 1MB encodes to %zu bytes one-shot and %zu streamed, "
                "BENCH_SPEC requires %d for both\n",
                bb, blob_streamed, BLOB_ENCODED);
        return 1;
    }

    run_decode_blob();
    run_decode_composite();
    if (memcmp(blob_dec, blob_src, BLOB_LEN) != 0
        || C.items[0] != "item-0"
        || C.items[COMP_ITEMS - 1] != "item-63"
        || C.text != std::string(COMP_TEXT, COMP_TEXT_LEN)
        || C.deep != 7 || C.tail != -1 || C.f130 != 0xDEADBEEFu) {
        fprintf(stderr, "bench: decode self-check failed\n");
        return 1;
    }

    printf("=== SofaBuffers C++ throughput (CPU time, MB/s) ===\n");
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
