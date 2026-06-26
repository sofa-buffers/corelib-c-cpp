/*!
 * @file bench.cpp
 * @brief Throughput benchmark for the pure-C++20 SofaBuffers implementation.
 *
 * Encodes and decodes a representative message in a tight loop and reports
 * throughput (MB/s) and per-message cost. Build at -O3 for representative
 * numbers. Unlike the C library (size-optimised), this implementation is tuned
 * for throughput: bulk memcpy payloads, single-write field headers, whole-array
 * float copies on little-endian, and a zero-copy decode over contiguous memory.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

struct Sample : sofab::IStreamMessage
{
    uint64_t id = 0; int64_t delta = 0; double value = 0; bool active = false;
    std::string name; std::array<float, 8> samples{};
    std::array<uint32_t, 8> codes{};

    void deserialize(sofab::IStreamImpl &is, sofab::id i, size_t, size_t) noexcept override
    {
        switch (i)
        {
            case 1: is.read(id); break;
            case 2: is.read(delta); break;
            case 3: is.read(value); break;
            case 4: is.read(active); break;
            case 5: is.read(name); break;
            case 6: is.read(samples); break;
            case 7: is.read(codes); break;
        }
    }
};

template <typename OS>
void encodeSample(OS &os)
{
    std::array<float, 8> samples{1.5f, -2.5f, 3.25f, 4e30f, -1e-20f, 0.0f, 100.0f, -7.0f};
    std::array<uint32_t, 8> codes{1, 200, 30000, 4000000, 500000000, 0x80000000u, 7, 42};
    os.write(1, uint64_t{1234567890123ull})
      .write(2, int64_t{-9876543210ll})
      .write(3, 3.141592653589793)
      .write(4, true)
      .write(5, std::string_view{"sensor/temperature/rack-7"})
      .write(6, samples)
      .write(7, codes);
}

} // namespace

int main()
{
    using clock = std::chrono::steady_clock;
    constexpr int kIters = 2'000'000;

    /* one encode to learn the message size */
    sofab::OStreamInline<512> probe;
    encodeSample(probe);
    const size_t msgSize = probe.bytesUsed();

    /* --- encode throughput --- */
    volatile size_t sink = 0;
    auto t0 = clock::now();
    for (int i = 0; i < kIters; ++i)
    {
        sofab::OStreamInline<512> os;
        encodeSample(os);
        sink += os.bytesUsed();
    }
    auto t1 = clock::now();

    /* --- decode throughput --- */
    std::vector<uint8_t> wire(probe.data(), probe.data() + msgSize);
    volatile uint64_t acc = 0;
    auto t2 = clock::now();
    for (int i = 0; i < kIters; ++i)
    {
        sofab::IStreamObject<Sample> in;
        in.feed(wire.data(), wire.size());
        acc += (*in).id + (*in).codes[3] + static_cast<uint64_t>((*in).name.size());
    }
    auto t3 = clock::now();

    auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
    double encMs = ms(t0, t1), decMs = ms(t2, t3);
    double bytes = static_cast<double>(msgSize) * kIters;

    std::printf("message size:      %zu bytes\n", msgSize);
    std::printf("encode: %8.1f MB/s   %6.1f ns/msg\n", bytes / encMs / 1e3, encMs * 1e6 / kIters);
    std::printf("decode: %8.1f MB/s   %6.1f ns/msg\n", bytes / decMs / 1e3, decMs * 1e6 / kIters);
    (void)sink; (void)acc;
    return 0;
}
