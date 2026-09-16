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

// Instantiates sofab::fixed_capacity_v, which nothing else in this translation
// unit reaches. That is not incidental: this file is the only C++ the RL78 job
// compiles, and Renesas' Clang 17 is the oldest front end in the matrix. The
// obvious spelling of that variable template -- an immediately-invoked constexpr
// lambda holding an inline requires-expression -- segfaults Clang 17 on
// instantiation, and until this line existed nothing instantiated it here, so
// the crash sat latent on main and surfaced only when an unrelated change
// happened to call readArray from this file. Asserting the values pins the
// spelling: revert it to the lambda and this job goes red rather than the next
// PR that touches an array.
static_assert(sofab::fixed_capacity_v<sofab::InlineVector<signed char, 4>> == 4,
              "a container that states its capacity must report it");
static_assert(sofab::fixed_capacity_v<sofab::FixedString<16>> == 16,
              "the fixed string is capacity-bearing too");
static_assert(sofab::fixed_capacity_v<int> == -1,
              "a type with no capacity() must report -1, not fail to compile");

namespace
{

/*! @brief An `enum` field's element type, as the generator declares it: a scoped
 *  enum at its normative declared width (MESSAGE_SPEC §1). The encode side takes
 *  a container of these directly, so nothing here materialises a converted copy
 *  -- which is the point on a profile with no heap.
 *
 *  Everything it touches below is behind @c SOFAB_CPP_HAVE_ARRAY: an enum array
 *  is an array, so it is part of the wire-feature subset a
 *  @c SOFAB_DISABLE_ARRAY_SUPPORT build compiles out, exactly like the integer
 *  array it replaces. */
enum class Gear : std::int8_t
{
    Reverse = -1,
    Neutral = 0,
    First   = 1,
    Second  = 2,
};

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
#if SOFAB_CPP_HAVE_ARRAY
        sofab::InlineVector<std::int8_t, 4> gears;   // heap-free enum array, at its width
#endif
    } data_;

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t count) noexcept override
    {
#if !SOFAB_CPP_HAVE_ARRAY
        (void)count;   // the array field below is compiled out with its support
#endif
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
#if SOFAB_CPP_HAVE_ARRAY
            case 8: is.readArray(data_.gears, count); break;
#endif
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

#if SOFAB_CPP_HAVE_ARRAY
    // The enum array goes over as stored: a container of scoped enums, written
    // with the one expression every other array uses. The negative constant is
    // there so the signed array wire type and its zig-zag are on this path too --
    // the underlying width picks both, and it is signed here.
    sofab::InlineVector<Gear, 4> gears;
    gears.push_back(Gear::Reverse);
    gears.push_back(Gear::Neutral);
    gears.push_back(Gear::Second);
    os.write(8, gears);
#endif

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

#if SOFAB_CPP_HAVE_ARRAY
    if (d.gears.size() != 3 || d.gears[0] != static_cast<std::int8_t>(Gear::Reverse)
        || d.gears[2] != static_cast<std::int8_t>(Gear::Second))
    {
        return -3;
    }
#endif

    return 0;
}
