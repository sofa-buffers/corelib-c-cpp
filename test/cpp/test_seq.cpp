/*!
 * @file test_seq.cpp
 * @brief SofaBuffers test for the object/row wrapper-array collectors (seq.hpp)
 *
 * SPDX-License-Identifier: MIT
 *
 * These collectors are not wire-visible: two implementations can disagree about
 * a gap-filled element, or about how much an announced index may allocate, and
 * still accept byte-identical input. The shared conformance vectors cannot cover
 * that in principle, so the placement rule of MESSAGE_SPEC §5.1, the §7 bound and
 * the §7.3 skip are pinned here (CORELIB_PLAN §7).
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <vector>

/* helpers ********************************************************************/

namespace
{

//! A two-field struct element, as a wrapper array of structs carries it.
//
// `final` because the collectors hold elements BY VALUE in a container, which
// destroys them through their own type: clang's
// -Wdelete-non-abstract-non-virtual-dtor flags that for any non-final class with
// virtual functions and a non-virtual destructor, which IStreamMessage is.
struct Point final : sofab::IStreamMessage
{
    uint32_t x = 0;
    uint32_t y = 0;

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        switch (id)
        {
            case 0: is.read(x); break;
            case 1: is.read(y); break;
            default: break;
        }
    }

    bool operator==(const Point &o) const noexcept { return x == o.x && y == o.y; }
};

//! Value of a @ref Point, for comparison (the type is not an aggregate).
Point pt(uint32_t x, uint32_t y)
{
    Point p;
    p.x = x;
    p.y = y;
    return p;
}

//! Decodes field 1 as a wrapper array through @p Seq into @p Container.
template <typename Seq, typename Container>
class Holder : public sofab::IStreamMessage
{
public:
    Seq seq;
    Container out;

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        if (id == 1) is.readSequence(seq, out);
    }

    Holder *operator->() noexcept { return this; }
};

using DynPoints   = Holder<sofab::MessageSeq<std::vector<Point>>, std::vector<Point>>;
using InlinePoints = Holder<sofab::FixedMessageSeq<sofab::InlineVector<Point, 2>>,
                            sofab::InlineVector<Point, 2>>;
using DynRows     = Holder<sofab::MessageSeq<std::vector<std::vector<uint32_t>>>,
                           std::vector<std::vector<uint32_t>>>;
using InlineRows  = Holder<
    sofab::FixedMessageSeq<sofab::InlineVector<sofab::InlineVector<uint32_t, 3>, 2>>,
    sofab::InlineVector<sofab::InlineVector<uint32_t, 3>, 2>>;
using DynFp32Rows = Holder<sofab::MessageSeq<std::vector<std::vector<float>>>,
                           std::vector<std::vector<float>>>;

//! Write one struct element of the wrapper array at element index @p id.
void writePoint(sofab::OStream &os, uint32_t id, uint32_t x, uint32_t y)
{
    os.sequenceBeginLazy(id);
    os.write(0, x);
    os.write(1, y);
    os.sequenceEnd();
}

} // namespace

/* the element type decides the §7.3 tag, from the type alone *****************/

TEST_CASE("seq: the element traits agree with what the reads expect")
{
    // fixed_capacity_v separates a heap-free container (whose capacity IS its
    // schema bound) from a growable one, at compile time.
    static_assert(sofab::fixed_capacity_v<std::vector<Point>> == -1);
    static_assert(sofab::fixed_capacity_v<sofab::InlineVector<Point, 2>> == 2);
    static_assert(sofab::fixed_capacity_v<sofab::FixedString<9>> == 9);

    // A struct/union element is a nested sequence; a native-scalar row is the
    // array wire type its element kind selects, split at the fixlen sub-type
    // where fp32 and fp64 share one wire type.
    static_assert(sofab::seq_elem_wire_v<Point> == static_cast<int>(sofab::Wire::SequenceStart));
    static_assert(sofab::seq_elem_fix_v<Point> == -1);
    static_assert(sofab::seq_elem_wire_v<std::vector<uint32_t>>
                  == static_cast<int>(sofab::Wire::ArrayUnsigned));
    static_assert(sofab::seq_elem_wire_v<std::vector<int64_t>>
                  == static_cast<int>(sofab::Wire::ArraySigned));
    static_assert(sofab::seq_elem_wire_v<std::vector<float>>
                  == static_cast<int>(sofab::Wire::ArrayFixlen));
    static_assert(sofab::seq_elem_fix_v<std::vector<float>> == static_cast<int>(sofab::Fix::Fp32));
    static_assert(sofab::seq_elem_fix_v<std::vector<double>> == static_cast<int>(sofab::Fix::Fp64));

    SUCCEED("compile-time only");
}

/* placement ******************************************************************/

TEST_CASE("MessageSeq: an omitted interior element fills a gap, it does not shift")
{
    // MESSAGE_SPEC §2 lets a conformant encoder leave an interior all-default
    // element off the wire, so ids 0 and 2 are the THREE-element array
    // [p0, default, p2] -- an appending collector would decode a two-element one.
    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    writePoint(os, 0, 1, 2);
    writePoint(os, 2, 5, 6);
    os.sequenceEnd();

    SECTION("growable storage")
    {
        sofab::IStreamObject<DynPoints> in;
        (*in).seq.cap = 3;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE(in->out.size() == 3);
        REQUIRE(in->out[0] == pt(1, 2));
        REQUIRE(in->out[1] == pt(0, 0));
        REQUIRE(in->out[2] == pt(5, 6));
    }

    SECTION("inline storage")
    {
        // Same array; here the container's own capacity is the bound, so nothing
        // has to be declared.
        sofab::IStreamObject<Holder<sofab::FixedMessageSeq<sofab::InlineVector<Point, 3>>,
                                    sofab::InlineVector<Point, 3>>> in;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE(in->out.size() == 3);
        REQUIRE(in->out[0] == pt(1, 2));
        REQUIRE(in->out[1] == pt(0, 0));
        REQUIRE(in->out[2] == pt(5, 6));
    }
}

TEST_CASE("MessageSeq: a repeated element id continues that element, it does not append")
{
    // §7.4: the id is the index, so reopening it addresses the SAME element. An
    // appending collector would decode two elements here.
    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    os.sequenceBeginLazy(0);
    os.write(0, uint32_t{1});
    os.sequenceEnd();
    os.sequenceBeginLazy(0);
    os.write(1, uint32_t{2});
    os.sequenceEnd();
    os.sequenceEnd();

    sofab::IStreamObject<DynPoints> in;
    (*in).seq.cap = 4;
    REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
    REQUIRE(in->out.size() == 1);
    REQUIRE(in->out[0] == pt(1, 2));
}

TEST_CASE("MessageSeq: the length is the highest present id + 1, never the count")
{
    // `count` is a capacity (§3): a shorter array stays short, it is not filled
    // out to N.
    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    writePoint(os, 0, 7, 8);
    os.sequenceEnd();

    sofab::IStreamObject<DynPoints> in;
    (*in).seq.cap = 3;
    REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
    REQUIRE(in->out.size() == 1);
    REQUIRE(in->out[0] == pt(7, 8));
}

TEST_CASE("MessageSeq: an empty wrapper array decodes to an empty container")
{
    sofab::OStream os{64};
    os.sequenceBeginLazy(1);
    os.sequenceEndKeep();   // present but empty

    sofab::IStreamObject<DynPoints> in;
    (*in).seq.cap = 3;
    REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
    REQUIRE(in->out.empty());
}

TEST_CASE("MessageSeq: a second occurrence of the field replaces the array")
{
    // §7.4 for the field itself: the sequence IS the array's value, so the later
    // occurrence replaces rather than extends. readSequence owns the clear; this
    // pins that the collector does not defeat it by placing into stale slots.
    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    writePoint(os, 0, 1, 1);
    writePoint(os, 1, 2, 2);
    os.sequenceEnd();
    os.sequenceBeginLazy(1);
    writePoint(os, 0, 9, 9);
    os.sequenceEnd();

    sofab::IStreamObject<DynPoints> in;
    (*in).seq.cap = 4;
    REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
    REQUIRE(in->out.size() == 1);
    REQUIRE(in->out[0] == pt(9, 9));
}

/* the schema bound ***********************************************************/

TEST_CASE("MessageSeq: the last index below the count is placed, the count itself is INVALID")
{
    auto wire = [](uint32_t id) {
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writePoint(os, id, 4, 5);
        os.sequenceEnd();
        return std::vector<uint8_t>(os.data(), os.data() + os.bytesUsed());
    };

    SECTION("growable: cap - 1 is the last index that fits")
    {
        const auto bytes = wire(1);
        sofab::IStreamObject<DynPoints> in;
        (*in).seq.cap = 2;
        REQUIRE(in.feed(bytes.data(), bytes.size()).ok());
        REQUIRE(in->out.size() == 2);
        REQUIRE(in->out[1] == pt(4, 5));
    }

    SECTION("growable: cap itself is a schema-bound violation")
    {
        const auto bytes = wire(2);
        sofab::IStreamObject<DynPoints> in;
        (*in).seq.cap = 2;
        REQUIRE(in.feed(bytes.data(), bytes.size()).code() == sofab::Error::InvalidMessage);
        REQUIRE(in->out.empty());
        // Rejected from the id alone, BEFORE the fill loop: nothing was reserved.
        REQUIRE(in->out.capacity() == 0);
    }

    SECTION("inline: capacity - 1 is the last index that fits")
    {
        const auto bytes = wire(1);
        sofab::IStreamObject<InlinePoints> in;
        REQUIRE(in.feed(bytes.data(), bytes.size()).ok());
        REQUIRE(in->out.size() == 2);
        REQUIRE(in->out[1] == pt(4, 5));
    }

    SECTION("inline: the capacity itself is a schema-bound violation")
    {
        // Also the issue #126 guard: InlineVector::emplace_back reuses its last
        // slot once full, so an unguarded fill loop would never reach this id.
        const auto bytes = wire(2);
        sofab::IStreamObject<InlinePoints> in;
        REQUIRE(in.feed(bytes.data(), bytes.size()).code() == sofab::Error::InvalidMessage);
        REQUIRE(in->out.empty());
    }
}

TEST_CASE("MessageSeq: an index near 2^31 is rejected without allocating")
{
    // The adversarial shape: one tiny element announces an index two billion
    // slots out. The bound is decided from the id, so neither container ever
    // grows towards it.
    const uint32_t huge = 0x7FFFFFF0u;

    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    writePoint(os, huge, 1, 1);
    os.sequenceEnd();

    SECTION("growable")
    {
        sofab::IStreamObject<DynPoints> in;
        (*in).seq.cap = 4;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::InvalidMessage);
        REQUIRE(in->out.capacity() == 0);
    }

    SECTION("inline")
    {
        sofab::IStreamObject<InlinePoints> in;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::InvalidMessage);
        REQUIRE(in->out.empty());
    }
}

/* §7.3: a contradicting element is skipped, intact ***************************/

TEST_CASE("MessageSeq: an element whose wire type contradicts the declared one is skipped")
{
    SECTION("the container keeps what the elements before it put there")
    {
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writePoint(os, 0, 3, 4);
        os.write(1, uint32_t{9});   // a varint where a struct element is declared
        os.sequenceEnd();

        sofab::IStreamObject<DynPoints> in;
        (*in).seq.cap = 4;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE(in->out.size() == 1);        // no phantom element at index 1
        REQUIRE(in->out[0] == pt(3, 4));
    }

    SECTION("the skip is decided before the bound, so a stray id is not INVALID")
    {
        // An id that is not this array's index at all cannot breach the index
        // bound either: §7.3 comes first, exactly as it does in readSequence.
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        os.write(9, uint32_t{9});
        os.sequenceEnd();

        sofab::IStreamObject<DynPoints> in;
        (*in).seq.cap = 2;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE(in->out.empty());
    }

    SECTION("an fp64 row is not an fp32 row (the fixlen sub-type separates them)")
    {
        const std::array<double, 2> row = {1.0, 2.0};
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        os.write(0, row);
        os.sequenceEnd();

        sofab::IStreamObject<DynFp32Rows> in;
        (*in).seq.cap = 2;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE(in->out.empty());
    }
}

/* matrix rows ****************************************************************/

TEST_CASE("MessageSeq: a native-scalar row is placed by id and sized by the wire count")
{
    const std::array<uint32_t, 3> r0 = {1, 2, 3};
    const std::array<uint32_t, 1> r2 = {7};

    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    os.write(0, r0);
    os.write(2, r2);
    os.sequenceEnd();

    SECTION("growable rows")
    {
        sofab::IStreamObject<DynRows> in;
        (*in).seq.cap = 3;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE(in->out.size() == 3);
        REQUIRE(in->out[0] == std::vector<uint32_t>{1, 2, 3});
        REQUIRE(in->out[1].empty());          // the gap row, at the element default
        REQUIRE(in->out[2] == std::vector<uint32_t>{7});
    }

    SECTION("inline rows: the outer bound still binds when the elements are rows")
    {
        // Index 2 is at the outer container's capacity of 2. The element before
        // it was already placed, which is what an INVALID message leaves behind.
        sofab::IStreamObject<InlineRows> in;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::InvalidMessage);
        REQUIRE(in->out.size() == 1);
    }
}

TEST_CASE("MessageSeq: a row longer than the row's own capacity is INVALID")
{
    // §7.1: the heap-free row's capacity IS the schema `count` it was generated
    // for, so an over-long row is rejected rather than truncated into it.
    const std::array<uint32_t, 5> row = {1, 2, 3, 4, 5};

    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    os.write(0, row);
    os.sequenceEnd();

    sofab::IStreamObject<InlineRows> in;
    REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::InvalidMessage);
}

TEST_CASE("MessageSeq: rows within the capacity round-trip through inline storage")
{
    const std::array<uint32_t, 3> r0 = {1, 2, 3};
    const std::array<uint32_t, 2> r1 = {8, 9};

    sofab::OStream os{256};
    os.sequenceBeginLazy(1);
    os.write(0, r0);
    os.write(1, r1);
    os.sequenceEnd();

    sofab::IStreamObject<InlineRows> in;
    REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
    REQUIRE(in->out.size() == 2);
    REQUIRE(in->out[0].size() == 3);
    REQUIRE(in->out[0][2] == 3);
    REQUIRE(in->out[1].size() == 2);
    REQUIRE(in->out[1][0] == 8);
}
