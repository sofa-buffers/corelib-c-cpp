/*!
 * @file freestanding.cpp
 * @brief Compile-only coverage for the C++ wrapper under -ffreestanding.
 *
 * SPDX-License-Identifier: MIT
 *
 * The per-target toolchain files build C++ with -ffreestanding
 * (utils/cortex-m/toolchain-arm-none-eabi.cmake), where libstdc++ offers only a
 * subset of the library: <string> and <vector> refuse to be included, and
 * <memory>/<functional> include but do not define std::shared_ptr or
 * std::function. sofab.hpp gates that convenience layer behind
 * SOFAB_CPP_HAVE_HOSTED; this translation unit is what holds the gate closed.
 *
 * It uses ONLY the heap-free half of the wrapper — FixedString, OStreamInline,
 * IStreamObject, nested sequences, a function-pointer flush callback — so it is
 * exactly the surface an MCU consumer has. Anything that slipped back into the
 * ungated core (a bare std::string overload, an OStream reference, a
 * std::function typedef) fails to compile here.
 *
 * Compile-only by design: this target exists for the bare-metal jobs, which
 * cannot run what they build. The hosted round-trip assertions live in
 * test/cpp-smoke/smoke.cpp, and this file mirrors its message shape so the two
 * stay comparable.
 */

#include "sofab/sofab.hpp"

#include <cstdint>

#if SOFAB_CPP_HAVE_HOSTED
#  error "freestanding.cpp must be compiled with -ffreestanding (or -DSOFAB_CPP_HAVE_HOSTED=0); it is the guard for the no-heap wrapper surface."
#endif

namespace
{

class Child : public sofab::IStreamMessage
{
public:
    struct Data
    {
        uint32_t id    = 0;
        float    value = 0.0f;
    } data_;

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        switch (id)
        {
            case 1: is.read(data_.id);    break;
            case 2: is.read(data_.value); break;
            default: break;
        }
    }
};

class Parent : public sofab::IStreamMessage
{
public:
    struct Data
    {
        uint32_t               header = 0;
        Child                  child;
        uint32_t               footer = 0;
        bool                   flag   = false;
        float                  ratio  = 0.0f;
        sofab::FixedString<16> tag;                  // heap-free string field
        sofab::FixedBytes<8>   blob;                 // heap-free blob field
    } data_;

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t) noexcept override
    {
        switch (id)
        {
            case 1: is.read(data_.header); break;
            case 2: is.read(data_.child);  break;   // nested sequence
            case 3: is.read(data_.footer); break;
            case 4: is.read(data_.flag);   break;
            case 5: is.read(data_.ratio);  break;
            case 6:
                data_.tag.set_len(size);
                if (size)
                {
                    is.read(data_.tag);
                }
                break;
            case 7:
                data_.blob.set_len(size);
                if (size)
                {
                    is.read(data_.blob);
                }
                break;
            default: break;
        }
    }
};

/*! @brief Freestanding flush callback: a plain function pointer, not a
 *  std::function — captureless by construction, which is the whole point. */
volatile size_t g_flushed = 0;
void onFlush(std::span<const uint8_t> bytes) noexcept
{
    g_flushed = bytes.size();
}

} // namespace

/*! @brief Encode and decode one message using only the no-heap surface. */
extern "C" int sofab_freestanding_roundtrip(void)
{
    static const uint8_t blobIn[] = {0xDE, 0xAD, 0xBE, 0xEF};

    // OStreamInline: buffer inside the object. The hosted OStream (shared_ptr)
    // is deliberately unavailable here.
    sofab::OStreamInline<256> os{&onFlush};
    os.write(1, 7u)
      .sequenceBeginLazy(2)
          .write(1, 42u)
          .write(2, 3.1415f)
      .sequenceEnd()
      .write(3, 99u)
      .write(4, true)
      .write(5, 2.5f)
      .write(6, sofab::FixedString<16>{"tag42"});
    os.write(7, blobIn, static_cast<int32_t>(sizeof(blobIn)));

    const size_t used = os.bytesUsed();

    sofab::IStreamObject<Parent> is;
    if (is.feed(os.data(), used).code() != sofab::Error::None)
    {
        return -1;
    }

    const Parent::Data &d = (*is).data_;
    if (d.header != 7u || d.child.data_.id != 42u || d.footer != 99u
        || !d.flag || d.tag != "tag42")
    {
        return -2;
    }

    return 0;
}
