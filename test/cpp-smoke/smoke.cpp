/*!
 * @file smoke.cpp
 * @brief Minimal C++ wrapper smoke test for reduced feature configurations.
 *
 * SPDX-License-Identifier: MIT
 *
 * The full hand-written unit tests (test/cpp) exercise every type — fp64,
 * 64-bit integers, arrays of all kinds — so they only compile in the full
 * ("max") build. This smoke instead uses ONLY capabilities that remain
 * available in every gracefully-supported reduced config: 32-bit integers,
 * bool, fp32, strings, blobs and nested sequences. It deliberately avoids
 * fp64 (no-fp64), 64-bit integers (no-int64) and arrays/spans (no-array) so the
 * same source compiles and round-trips under each of those builds, proving the
 * wrapper header is valid there — the coverage the C-only feature matrix can't
 * give. Builds that disable FIXLEN or SEQUENCE are rejected by a #error in
 * sofab.hpp and are not built with this smoke.
 */

#include "sofab/sofab.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;

// Catch2 is intentionally not linked here (it would pull a network dependency
// into every reduced-config CI job); use a tiny always-on check instead so the
// runtime assertions survive -DNDEBUG / Release builds.
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

class SmokeChild : public sofab::IStreamMessage
{
public:
    struct Data
    {
        uint32_t id    = 0;
        float    value = 0.0f;
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t, size_t) noexcept override
    {
        switch (id)
        {
            case 1: istream.read(data_.id);    break;
            case 2: istream.read(data_.value); break;
        }
    }
};

class SmokeParent : public sofab::IStreamMessage
{
public:
    struct Data
    {
        uint32_t             header = 0;
        SmokeChild           child;
        uint32_t             footer = 0;
        bool                 flag   = false;
        float                ratio  = 0.0f;
        std::string          name;
        std::vector<uint8_t> blob;
        sofab::FixedString<16> tag;                  // heap-free string field
    } data_;

    void deserialize(sofab::IStreamImpl &istream, sofab::id id, size_t size, size_t) noexcept override
    {
        switch (id)
        {
            case 1: istream.read(data_.header); break;
            case 2: istream.read(data_.child);  break;   // nested sequence
            case 3: istream.read(data_.footer); break;
            case 4: istream.read(data_.flag);   break;
            case 5: istream.read(data_.ratio);  break;
            case 6:
                data_.name.resize(size);
                istream.read(data_.name);
                break;
            case 7:
                data_.blob.resize(size);
                istream.read(data_.blob.data(), data_.blob.size());
                break;
            case 8:
                // Mirrors the generator contract for a FixedString field.
                data_.tag.set_len(size);
                if (size)
                {
                    istream.read(data_.tag);
                }
                break;
        }
    }
};

int main()
{
    const std::string name      = "sofa";
    const uint8_t     blobIn[]  = {0xDE, 0xAD, 0xBE, 0xEF};

    sofab::OStream ostream{256};
    ostream
        .write(1, 7u)                       // parent header
        .sequenceBegin(2)                   // nested child (fresh id scope)
            .write(1, 42u)                  //   child id
            .write(2, 3.1415f)              //   child value (fp32)
        .sequenceEnd()
        .write(3, 99u)                      // parent footer
        .write(4, true)                     // bool
        .write(5, 2.5f)                     // fp32
        .write(6, std::string_view{name});  // string
    ostream.write(7, blobIn, static_cast<int32_t>(sizeof(blobIn)));  // blob
    ostream.write(8, sofab::FixedString<16>{"tag42"});  // heap-free string

    const auto used = ostream.bytesUsed();
    CHECK(used > 0);

    sofab::IStreamObject<SmokeParent> istream;
    auto result = istream.feed(ostream.data(), used);

    CHECK(result.code() == sofab::Error::None);

    const auto &d = (*istream).data_;
    CHECK(d.header == 7u);
    CHECK(d.child.data_.id == 42u);
    CHECK(d.child.data_.value == 3.1415f);
    CHECK(d.footer == 99u);
    CHECK(d.flag == true);
    CHECK(d.ratio == 2.5f);
    CHECK(d.name == "sofa");
    CHECK(d.blob == std::vector<uint8_t>(blobIn, blobIn + sizeof(blobIn)));
    CHECK(d.tag == "tag42");
    CHECK(std::string_view{d.tag.c_str()} == "tag42");

    if (g_failures == 0)
    {
        std::puts("sofab C++ smoke: OK");
        return 0;
    }

    std::printf("sofab C++ smoke: %d check(s) failed\n", g_failures);
    return 1;
}
