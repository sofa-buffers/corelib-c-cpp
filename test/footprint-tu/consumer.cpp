/*!
 * @file consumer.cpp
 * @brief The reference consumer translation unit, written against the C++ wrapper.
 *
 * SPDX-License-Identifier: MIT
 *
 * The C++ twin of consumer.c: the same message, the same round trip, through
 * sofab/sofab.hpp instead of the C API. See consumer.c for why both exist and
 * what has to stay in step between them.
 *
 * It uses only the heap-free surface (FixedString, OStreamInline,
 * IStreamObject, a function-pointer flush callback), because that surface is
 * the one that exists in BOTH wrapper modes -- so the freestanding and hosted
 * rows of the README table differ by what the header gates, not by what this
 * file asks for. tools/footprint-tu.sh compiles it in three modes: freestanding
 * (-ffreestanding, SOFAB_CPP_HAVE_HOSTED off), hosted, and hosted with RTTI on.
 */

#include "sofab/sofab.hpp"

#include <cstdint>

namespace
{

/*! @brief The nested sequence's two scalars (ids 1 and 2). */
class Child : public sofab::IStreamMessage
{
public:
    struct Data
    {
        std::uint32_t id    = 0;
        float         value = 0.0f;
    } data_;

    void deserialize (sofab::IStreamImpl &is, sofab::id id, size_t,
                      size_t) noexcept override
    {
        switch (id)
        {
            case 1: is.read(data_.id);    break;
            case 2: is.read(data_.value); break;
            default: break;
        }
    }
};

/*! @brief Five scalars, one nested sequence, one string -- the message the
 *  README's per-TU table is measured on. */
class Parent : public sofab::IStreamMessage
{
public:
    struct Data
    {
        std::uint32_t          header = 0;        // id 1
        Child                  child;             // id 2, a nested sequence
        std::uint32_t          footer = 0;        // id 3
        bool                   flag   = false;    // id 4
        float                  ratio  = 0.0f;     // id 5
        sofab::FixedString<16> tag;               // id 6, a string
        std::uint16_t          count  = 0;        // id 7
    } data_;

    void deserialize (sofab::IStreamImpl &is, sofab::id id, size_t size,
                      size_t) noexcept override
    {
        switch (id)
        {
            case 1: is.read(data_.header); break;
            case 2: is.read(data_.child);  break;
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
            case 7: is.read(data_.count); break;
            default: break;
        }
    }
};

} // namespace

/* Consumed through volatiles so -Os cannot delete the round trip. */
volatile std::uint32_t g_sink_u32;
volatile size_t        g_flushed;

namespace
{

void onFlush (std::span<const std::uint8_t> bytes) noexcept
{
    g_flushed = bytes.size();
}

} // namespace

extern "C" int sofab_tu_roundtrip (void)
{
    sofab::OStreamInline<256> os{&onFlush};

    os.write(1, 7u)
      .sequenceBeginLazy(2)
          .write(1, 42u)
          .write(2, 3.1415f)
      .sequenceEnd()
      .write(3, 99u)
      .write(4, true)
      .write(5, 2.5f)
      .write(6, sofab::FixedString<16>{"tag42"})
      .write(7, static_cast<std::uint16_t>(3u));

    const size_t used = os.bytesUsed();

    sofab::IStreamObject<Parent> is;
    if (is.feed(os.data(), used).code() != sofab::Error::None)
    {
        return -1;
    }

    const Parent::Data &d = (*is).data_;

    g_sink_u32 = d.header + d.footer + d.child.data_.id
               + static_cast<std::uint32_t>(d.count)
               + static_cast<std::uint32_t>(d.flag)
               + static_cast<std::uint32_t>(d.ratio)
               + static_cast<std::uint32_t>(d.child.data_.value)
               + static_cast<std::uint32_t>(
                     static_cast<unsigned char>(d.tag.data()[0]));

    return 0;
}
