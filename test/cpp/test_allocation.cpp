/*!
 * @file test_allocation.cpp
 * @brief CORELIB_PLAN §6.6.4 — the *measured* half of the memory rule.
 *
 * §6.6.4 is explicit that reading the source is not enough:
 *
 *   Source inspection alone is still **not sufficient**, because an indirect
 *   allocation through a caller-supplied container leaves no `malloc` in the
 *   source to find. Conformance therefore requires **both**: **read** — no
 *   allocation primitive is reachable from a codec entry point; **measure** — an
 *   allocation count, or the heap high-water mark, over a complete encode and a
 *   complete decode, measured after the codec's one-time construction, which
 *   **MUST** be zero (§13).
 *
 * This repository had the read half and nothing else: `run_callgrind.sh` counts
 * instructions, `tools/footprint.sh` reports static storage, `stack-usage.sh`
 * reports frame sizes. None of them would notice an allocation appearing.
 *
 * That gap is not hypothetical. The defect this file's third case pins — the
 * growable C++ profile taking heap on the decode path — survived a source-level
 * audit of this repository precisely because the number was never measured.
 *
 * @par What is measured, and what the answers mean
 * Global `operator new` / `delete` are replaced and counted. The counter is armed
 * **after** the streams are constructed, which is the boundary §6.6 draws:
 * "Constructing the encoder or decoder … is the caller's act, happens once, at
 * setup, and MAY allocate. The prohibition binds everything **after**
 * construction."
 *
 *   - the C core, driven directly: **zero**, and this is a hard assertion;
 *   - the heap-free C++ profile (`OStreamInline`, `FixedString`, `InlineVector`):
 *     **zero**, likewise;
 *   - the growable profile (`std::string`, `std::vector`): non-zero by design —
 *     the caller chose a destination that allocates. What is asserted there is
 *     the property §6.6 actually protects: the count **does not grow with the
 *     message**. A hostile length or count must not buy more allocations than a
 *     small one of the same shape.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace
{

/*! Armed only around a measured region, so Catch2's own allocations are not
 *  counted and the harness needs no cooperation from the rest of the binary. */
struct AllocCounter
{
    static inline bool   armed = false;
    static inline size_t allocs = 0;
    static inline size_t bytes = 0;

    AllocCounter() noexcept  { allocs = 0; bytes = 0; armed = true; }
    ~AllocCounter() noexcept { armed = false; }
};

} // namespace

void *operator new(std::size_t n)
{
    if (AllocCounter::armed)
    {
        AllocCounter::allocs++;
        AllocCounter::bytes += n;
    }
    void *p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc{};
    return p;
}

void *operator new[](std::size_t n) { return operator new(n); }
void  operator delete(void *p) noexcept { std::free(p); }
void  operator delete[](void *p) noexcept { std::free(p); }
void  operator delete(void *p, std::size_t) noexcept { std::free(p); }
void  operator delete[](void *p, std::size_t) noexcept { std::free(p); }

namespace
{

/*! One message of every shape the codec has a path for: a scalar, a string, a
 *  blob, all three array wire types and a nested sequence. `n` scales the
 *  variable-length parts, which is what the third case varies. */
std::vector<uint8_t> buildMessage(sofab::OStreamImpl &os, size_t n)
{
    const std::string  text(n, 'x');
    std::vector<uint8_t> blob(n, 0x5A);
    std::vector<uint32_t> us(n, 7u);
    std::vector<int32_t>  is(n, -7);
    std::vector<float>    fs(n, 1.5f);

    os.write(1, uint32_t{0xDEADBEEF});
    os.write(2, text);
    os.write(3, blob.data(), static_cast<int32_t>(blob.size()));
    os.write(4, std::span<const uint32_t>(us.data(), us.size()));
    os.write(5, std::span<const int32_t>(is.data(), is.size()));
    os.write(6, std::span<const float>(fs.data(), fs.size()));
    os.sequenceBeginLazy(7);
    os.write(0, uint32_t{99});
    os.sequenceEnd();
    os.flush();

    return {os.data(), os.data() + os.bytesUsed()};
}

/*! A handler binding heap-free destinations only. */
struct HeapFreeSink final : sofab::IStreamMessage
{
    uint32_t                        scalar = 0;
    sofab::FixedString<64>          text{};
    sofab::FixedBytes<64>           blob{};
    sofab::InlineVector<uint32_t,8> us{};
    sofab::InlineVector<int32_t,8>  is{};
    sofab::InlineVector<float,8>    fs{};
    uint32_t                        nested = 0;

    void deserialize(sofab::IStreamImpl &in, sofab::id id, size_t size, size_t count) noexcept override
    {
        switch (id)
        {
            case 1: in.read(scalar); break;
            case 2: in.readString(text, size); break;
            case 3: in.readBlob(blob, size); break;
            case 4: in.readArray(us, count); break;
            case 5: in.readArray(is, count); break;
            case 6: in.readArray(fs, count); break;
            case 0: in.read(nested); break;
            default: break;
        }
    }
};

/*! The same, into growable destinations the caller owns. */
struct GrowableSink final : sofab::IStreamMessage
{
    uint32_t              scalar = 0;
    std::string           text{};
    std::vector<uint8_t>  blob{};
    std::vector<uint32_t> us{};
    std::vector<int32_t>  is{};
    std::vector<float>    fs{};
    uint32_t              nested = 0;

    void deserialize(sofab::IStreamImpl &in, sofab::id id, size_t size, size_t count) noexcept override
    {
        switch (id)
        {
            case 1: in.read(scalar); break;
            case 2: in.readString(text, size); break;
            case 3: in.readBlob(blob, size); break;
            case 4: in.readArray(us, count); break;
            case 5: in.readArray(is, count); break;
            case 6: in.readArray(fs, count); break;
            case 0: in.read(nested); break;
            default: break;
        }
    }
};

/*! Decode @p wire into @p sink and return the allocations that cost, counting
 *  only what happens after the decoder exists. */
template <typename Sink>
size_t decodeAllocations(const std::vector<uint8_t> &wire, Sink &sink)
{
    sofab::IStreamObject<Sink> stream;   /* ONE-TIME CONSTRUCTION -- may allocate */

    AllocCounter counted;                /* armed here, per §6.6 */
    auto r = stream.feed(wire.data(), wire.size());
    const size_t n = AllocCounter::allocs;
    REQUIRE(r.ok());

    sink = *stream;
    return n;
}

} // namespace

TEST_CASE("§6.6.4: the C core encodes and decodes without allocating")
{
    uint8_t obuf[512];
    uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    sofab_ostream_t os;
    sofab_istream_t is;
    char sink[64] = {0};

    /* Construction first; the counter is armed after it, per §6.6. */
    sofab_ostream_init(&os, obuf, sizeof(obuf), 0, nullptr, nullptr);

    size_t used = 0;
    {
        AllocCounter counted;
        sofab_ostream_write_unsigned(&os, 1, 0xDEADBEEFu);
        sofab_ostream_write_string(&os, 2, "a string that is comfortably long");
        sofab_ostream_write_blob(&os, 3, payload, sizeof(payload));
        sofab_ostream_write_sequence_begin(&os, 4);
        sofab_ostream_write_unsigned(&os, 0, 99u);
        sofab_ostream_write_sequence_end(&os);
        used = sofab_ostream_bytes_used(&os);

        REQUIRE(AllocCounter::allocs == 0);
    }

    struct Ctx { char *dst; size_t cap; } ctx{sink, sizeof(sink)};
    sofab_istream_init(&is,
        [](sofab_istream_t *c, sofab_id_t id, size_t, size_t, void *u) noexcept
        {
            auto *x = static_cast<Ctx *>(u);
            if (id == 2) sofab_istream_read_string(c, x->dst, x->cap);
        },
        &ctx);

    {
        AllocCounter counted;
        const sofab_ret_t ret = sofab_istream_feed(&is, obuf, used);

        REQUIRE(ret == SOFAB_RET_OK);
        REQUIRE(AllocCounter::allocs == 0);
    }
}

TEST_CASE("§6.6.4: the heap-free C++ profile encodes and decodes without allocating")
{
    sofab::OStreamInline<512> os;

    std::vector<uint8_t> wire;
    {
        /* The message is built from std::string / std::vector sources, which
         * allocate on the caller's side; only the encode is measured. */
        const std::string  text(8, 'x');
        std::vector<uint8_t> blob(8, 0x5A);
        std::vector<uint32_t> us(8, 7u);

        AllocCounter counted;
        os.write(1, uint32_t{0xDEADBEEF});
        os.write(2, std::string_view{text});
        os.write(3, blob.data(), static_cast<int32_t>(blob.size()));
        os.write(4, std::span<const uint32_t>(us.data(), us.size()));
        os.flush();

        REQUIRE(AllocCounter::allocs == 0);
        wire.assign(os.data(), os.data() + os.bytesUsed());
    }

    HeapFreeSink sink;
    const size_t n = decodeAllocations(wire, sink);

    INFO("heap-free decode allocations: " << n);
    REQUIRE(n == 0);
    REQUIRE(sink.scalar == 0xDEADBEEFu);
    REQUIRE(sink.us.size() == 8);
}

TEST_CASE("§6.6.4: the growable profile allocates once per destination, never per element")
{
    /* Non-zero here is not a defect: the caller chose destinations that allocate,
     * and §6.6 puts that storage on the caller's side of the line. What must hold
     * is the property the rule actually protects -- the codec must **check the
     * count and then allocate it exactly, once**, never reserve-and-extend. §6.6:
     * "everything with a count or length on the wire ahead of its payload checks
     * that word and allocates exactly it, once."
     *
     * So the ceiling is one allocation per growable destination, and it must not
     * move with the size of the message. This message has five: text, blob and
     * the three arrays.
     *
     * The count is not *equal* across the two sizes, and that is std::string's
     * doing rather than the codec's: an 8-byte string fits its small-string
     * buffer and allocates nothing, a 4096-byte one does not. Asserting equality
     * would be asserting a libstdc++ implementation detail. Asserting the ceiling
     * is asserting the codec. */
    constexpr size_t kGrowableDestinations = 5;

    sofab::OStream small{4096};
    sofab::OStream large{262144};

    const auto wireSmall = buildMessage(small, 8);
    const auto wireLarge = buildMessage(large, 4096);

    GrowableSink a, b;
    const size_t nSmall = decodeAllocations(wireSmall, a);
    const size_t nLarge = decodeAllocations(wireLarge, b);

    INFO("growable decode allocations: " << nSmall << " (n=8) vs " << nLarge << " (n=4096)");

    /* A codec that grew into its destinations would need log2(4096/initial)
     * reallocations per container here, so the ceiling is what separates
     * check-then-allocate from reserve-and-extend. */
    REQUIRE(nSmall <= kGrowableDestinations);
    REQUIRE(nLarge <= kGrowableDestinations);

    /* And a 512x larger message buys at most the one allocation std::string's
     * small-buffer threshold accounts for. */
    REQUIRE(nLarge - nSmall <= 1);

    REQUIRE(a.text.size() == 8);
    REQUIRE(b.text.size() == 4096);
    REQUIRE(b.us.size() == 4096);
}
