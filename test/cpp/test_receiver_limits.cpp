/*!
 * @file test_receiver_limits.cpp
 * @brief CORELIB_PLAN §6.2.1 receiver-side limits, and §6.3's three refusal tiers.
 *
 * The whole point of these cases is that **three different refusals must stay
 * three different answers**. A decoder that folds them together is wrong in a way
 * no round-trip test notices: every one of the messages below is refused either
 * way, and only the *reason* differs.
 *
 *   tier 1  the schema declares a bound and the field breaks it
 *           -> Error::InvalidMessage. These bytes are invalid, full stop.
 *   tier 2  the schema declares nothing and this receiver's configured cap is
 *           exceeded -> Error::LimitExceeded. The bytes are FINE; a receiver
 *           configured more loosely decodes them. Reporting this as
 *           InvalidMessage is what §6.2.1 forbids in as many words.
 *   tier 3  both admit the field, but the destination this caller handed over is
 *           too short -> Error::InvalidArgument. Not the message's fault and not
 *           the deployment's: the call's.
 *
 * The tier-2/tier-3 pair is the one that is easy to get wrong, because on a
 * heap-free destination they look identical from the outside — an unbounded field
 * that does not fit. They are told apart by *which ceiling stopped it*: the
 * configured cap, or the storage.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace
{

/* The receiver's policy for these cases: deliberately tight, so a cap breach is
 * a few bytes of test data rather than a megabyte. §6.2.1 has no unset state, so
 * every stream states all three. */
constexpr sofab::Limits kTight{/*array*/ 4, /*string*/ 8, /*blob*/ 8};

/* A cap far above anything these messages carry, for the cases that must show a
 * limit NOT binding. */
constexpr sofab::Limits kLoose{1024, 1024, 1024};

/*! Encode one `string` field at id 1 carrying @p n bytes of 'a'. */
std::vector<uint8_t> stringField(size_t n)
{
    sofab::OStream os{n + 32};
    os.write(1, std::string(n, 'a'));
    os.flush();
    return {os.data(), os.data() + os.bytesUsed()};
}

/*! Encode one `blob` field at id 1 carrying @p n bytes. */
std::vector<uint8_t> blobField(size_t n)
{
    std::vector<uint8_t> payload(n, 0x5A);
    sofab::OStream os{n + 32};
    os.write(1, payload.data(), static_cast<int32_t>(payload.size()));
    os.flush();
    return {os.data(), os.data() + os.bytesUsed()};
}

/*! Encode one `u32` array field at id 1 carrying @p n elements. */
std::vector<uint8_t> arrayField(size_t n)
{
    std::vector<uint32_t> payload(n, 7u);
    sofab::OStream os{n * 6 + 32};
    os.write(1, std::span<const uint32_t>(payload.data(), payload.size()));
    os.flush();
    return {os.data(), os.data() + os.bytesUsed()};
}

/*!
 * A message with one `string` field at id 1, whose declared `maxlen` and whose
 * destination shape are both chosen by the test.
 *
 * @tparam Dst  `std::string` (growable) or `sofab::FixedString<N>` (heap-free).
 */
template <typename Dst>
struct StringMessage final : sofab::IStreamMessage
{
    long maxlen = -1;   //!< what the SCHEMA declares; -1 = it declares nothing
    Dst value{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t) noexcept override
    {
        if (id == 1) is.readString(value, size, maxlen);
    }
};

/*! The `blob` counterpart of @ref StringMessage. */
template <typename Dst>
struct BlobMessage final : sofab::IStreamMessage
{
    long maxlen = -1;
    Dst value{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t) noexcept override
    {
        if (id == 1) is.readBlob(value, size, maxlen);
    }
};

/*! The array counterpart: one `u32` array field at id 1. */
template <typename Dst>
struct ArrayMessage final : sofab::IStreamMessage
{
    long cap = -1;      //!< the schema's `count`; -1 = it declares none
    Dst value{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t count) noexcept override
    {
        if (id == 1) is.readArray(value, count, cap);
    }
};

/*! A `string` wrapper array (MESSAGE_SPEC §5.1) into growable storage. */
struct StringArrayMessage final : sofab::IStreamMessage
{
    sofab::StringSeq seq;
    std::vector<std::string> out;

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        if (id == 1) is.readSequence(seq, out);
    }
};

/*! Write one element of a `string` wrapper array at element index @p id. */
void writeElement(sofab::OStream &os, uint32_t id, const std::string &v)
{
    os.write(id, v);
}

} // namespace

/* ---------------------------------------------------------------------------
 * tier 2 — a cap breach on a schema-UNBOUNDED field is LimitExceeded, never
 *          INVALID. This is A2-0002: before it, no such answer existed.
 * ------------------------------------------------------------------------- */

TEST_CASE("limits: an unbounded string past the cap is LimitExceeded, not INVALID")
{
    const auto wire = stringField(9);   // cap is 8

    sofab::IStreamObject<StringMessage<std::string>> in{kTight};
    // the schema declares no maxlen -- which is what arms the receiver cap
    (*in).maxlen = -1;

    auto r = in.feed(wire.data(), wire.size());

    REQUIRE(r.code() == sofab::Error::LimitExceeded);
    REQUIRE(r.limitExceeded());
    // The distinction the clause is about: this is NOT the INVALID outcome.
    REQUIRE_FALSE(r.invalid());
    REQUIRE(r.code() != sofab::Error::InvalidMessage);
    REQUIRE_FALSE(r.ok());
    REQUIRE_FALSE(r.incomplete());

    // Rejected, never clamped: nothing was materialized (§6.2.1).
    REQUIRE((*in).value.empty());
}

TEST_CASE("limits: the same bytes decode under a looser cap")
{
    // The property that makes tier 2 a POLICY rejection rather than a verdict on
    // the bytes: identical input, different receiver, different answer -- and
    // §6.2.1 says that is "not an interop failure and not a conformance defect".
    const auto wire = stringField(9);

    sofab::IStreamObject<StringMessage<std::string>> loose{kLoose};
    auto r = loose.feed(wire.data(), wire.size());

    REQUIRE(r.ok());
    REQUIRE((*loose).value == std::string(9, 'a'));
}

TEST_CASE("limits: an unbounded blob past the cap is LimitExceeded")
{
    const auto wire = blobField(9);     // blob cap is 8

    sofab::IStreamObject<BlobMessage<std::vector<uint8_t>>> in{kTight};
    (*in).maxlen = -1;

    REQUIRE(in.feed(wire.data(), wire.size()).code() == sofab::Error::LimitExceeded);
    REQUIRE((*in).value.empty());
}

TEST_CASE("limits: string and blob are separate caps")
{
    // §6.2.1 lists max_dyn_string_len and max_dyn_blob_len as two limits, not
    // one: a deployment may well take a megabyte of opaque bytes and no such
    // quantity of text. A port that shares one number passes every case above
    // and fails this one.
    constexpr sofab::Limits split{/*array*/ 4, /*string*/ 4, /*blob*/ 64};
    const auto text = stringField(8);
    const auto bytes = blobField(8);

    sofab::IStreamObject<StringMessage<std::string>> s{split};
    REQUIRE(s.feed(text.data(), text.size()).code() == sofab::Error::LimitExceeded);

    sofab::IStreamObject<BlobMessage<std::vector<uint8_t>>> b{split};
    REQUIRE(b.feed(bytes.data(), bytes.size()).ok());
    REQUIRE((*b).value.size() == 8);
}

TEST_CASE("limits: an unbounded array past the cap is LimitExceeded")
{
    const auto wire = arrayField(5);    // array cap is 4

    sofab::IStreamObject<ArrayMessage<std::vector<uint32_t>>> in{kTight};
    (*in).cap = -1;

    auto r = in.feed(wire.data(), wire.size());
    REQUIRE(r.code() == sofab::Error::LimitExceeded);
    REQUIRE((*in).value.empty());     // refused at the count header, before the resize
}

TEST_CASE("limits: the cap is enforced at the length header, before the allocation")
{
    // §6.2.1's enforcement point, and the shape of A2-0001's report: a message
    // that is nothing BUT a header claims a huge length, and the decode must be
    // over before a single payload byte is asked for. Feeding only the header --
    // the payload never arrives at all -- an unguarded decoder answers INCOMPLETE
    // (having already committed the allocation); a guarded one has already
    // refused.
    // A2-0001's reproducer verbatim: six bytes, no payload at all. Header 0x0A
    // is id 1 / wire type fixlen; the varint that follows is
    // fixlen_word = (100000000 << 3) | 2, i.e. a 100 MB string.
    const uint8_t header[] = {0x0A, 0x82, 0x90, 0xBC, 0xFD, 0x02};

    sofab::IStreamObject<StringMessage<std::string>> in{kTight};
    auto r = in.feed(header, sizeof(header));

    REQUIRE(r.code() == sofab::Error::LimitExceeded);
    // Not INCOMPLETE: an unguarded decoder answers "send me the other 100 MB",
    // having already committed the allocation.
    REQUIRE_FALSE(r.incomplete());
    REQUIRE((*in).value.capacity() < 100000);
}

TEST_CASE("limits: a limit rejection is terminal")
{
    // §6.3: "a terminal, receiver-local policy rejection". Every later feed
    // repeats the same answer -- and it stays LimitExceeded rather than decaying
    // into the core's plain InvalidMessage.
    const auto wire = stringField(9);

    sofab::IStreamObject<StringMessage<std::string>> in{kTight};
    REQUIRE(in.feed(wire.data(), wire.size()).code() == sofab::Error::LimitExceeded);

    const uint8_t more[] = {0x00, 0x01};
    REQUIRE(in.feed(more, sizeof(more)).code() == sofab::Error::LimitExceeded);
    REQUIRE(in.feed(nullptr, 0).code() == sofab::Error::LimitExceeded);
}

/* ---------------------------------------------------------------------------
 * the cap MUST NOT touch a field the schema already bounds
 * ------------------------------------------------------------------------- */

TEST_CASE("limits: a schema-bounded field is judged by its schema bound, not by the cap")
{
    // §6.2.1: receiver limits "MUST NOT be applied to a field the schema already
    // bounds. There the schema bound governs and its violation is INVALID" --
    // "a schema bound is a statement about validity, a receiver limit about
    // capacity". Both directions have to hold, and they are opposite answers.

    SECTION("within the schema bound but past the cap: the cap does not bind")
    {
        // maxlen 64, cap 8, field 32 bytes. A port that applies the cap anyway
        // rejects a message it must accept.
        const auto wire = stringField(32);

        sofab::IStreamObject<StringMessage<std::string>> in{kTight};
        (*in).maxlen = 64;

        REQUIRE(in.feed(wire.data(), wire.size()).ok());
        REQUIRE((*in).value == std::string(32, 'a'));
    }

    SECTION("past the schema bound: INVALID, never LimitExceeded")
    {
        // maxlen 4, field 9 bytes -- also past the cap of 8, so a port that
        // checked the cap first would answer LimitExceeded and promise a limit
        // to raise. The schema decides, and it says these bytes are invalid.
        const auto wire = stringField(9);

        sofab::IStreamObject<StringMessage<std::string>> in{kTight};
        (*in).maxlen = 4;

        auto r = in.feed(wire.data(), wire.size());
        REQUIRE(r.code() == sofab::Error::InvalidMessage);
        REQUIRE(r.invalid());
        REQUIRE_FALSE(r.limitExceeded());
    }

    SECTION("the same holds for an array count")
    {
        const auto wire = arrayField(8);    // array cap is 4

        sofab::IStreamObject<ArrayMessage<std::vector<uint32_t>>> ok{kTight};
        (*ok).cap = 16;                     // the schema bounds it: the cap is inert
        REQUIRE(ok.feed(wire.data(), wire.size()).ok());
        REQUIRE((*ok).value.size() == 8);

        sofab::IStreamObject<ArrayMessage<std::vector<uint32_t>>> bad{kTight};
        (*bad).cap = 2;                     // and its violation is INVALID
        REQUIRE(bad.feed(wire.data(), wire.size()).code() == sofab::Error::InvalidMessage);
    }
}

/* ---------------------------------------------------------------------------
 * tier 3 — the destination, not the message and not the deployment
 * ------------------------------------------------------------------------- */

TEST_CASE("limits: an unbounded field the destination cannot hold is InvalidArgument")
{
    // §6.3's third tier, and the distinction the fix exists to make: the schema
    // declares nothing (maxlen -1) and the configured cap ADMITS the field, so
    // there is no limit to raise -- what does not fit is the FixedString this
    // caller handed over. LimitExceeded would name a limit that was never
    // crossed; InvalidMessage would call a well-formed message malformed.
    const auto wire = stringField(6);       // cap is 8: the field clears it

    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> in{kTight};
    (*in).maxlen = -1;

    auto r = in.feed(wire.data(), wire.size());

    REQUIRE(r.code() == sofab::Error::InvalidArgument);
    REQUIRE_FALSE(r.limitExceeded());
    REQUIRE_FALSE(r.invalid());

    // Refused, not filled part-way (§6.6.3).
    REQUIRE((*in).value.size() == 0);
}

TEST_CASE("limits: tier 2 and tier 3 are told apart by WHICH ceiling stopped it")
{
    // The same destination and the same schema (none) -- only the length moves.
    // Below the cap it is the storage that refuses; above it, the policy. A port
    // that reports one code for both fails exactly here.
    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> small{kTight};
    const auto six = stringField(6);        // 4 < 6 <= 8
    REQUIRE(small.feed(six.data(), six.size()).code() == sofab::Error::InvalidArgument);

    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> big{kTight};
    const auto nine = stringField(9);       // 8 < 9
    REQUIRE(big.feed(nine.data(), nine.size()).code() == sofab::Error::LimitExceeded);
}

TEST_CASE("limits: tier 1 still wins over tier 3")
{
    // A schema bound the field breaks is INVALID even when the destination is
    // also too small: the message is malformed, and that is the truer statement.
    const auto wire = stringField(6);

    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> in{kTight};
    (*in).maxlen = 5;

    REQUIRE(in.feed(wire.data(), wire.size()).code() == sofab::Error::InvalidMessage);
}

TEST_CASE("limits: an over-capacity array count is refused, not silently truncated")
{
    // A heap-free container's resize() CLAMPS, so without the tier-3 check the
    // surplus elements would go missing and the decode would report COMPLETE --
    // a short array presented as the whole value.
    const auto wire = arrayField(3);        // array cap is 4: the count clears it

    sofab::IStreamObject<ArrayMessage<sofab::InlineVector<uint32_t, 2>>> in{kTight};
    (*in).cap = -1;

    auto r = in.feed(wire.data(), wire.size());
    REQUIRE(r.code() == sofab::Error::InvalidArgument);
    REQUIRE((*in).value.size() == 0);
}

TEST_CASE("limits: a growable destination never reaches tier 3")
{
    // It can hold whatever tiers 1 and 2 let through, so the only two answers a
    // std::string can give are ok and LimitExceeded.
    const auto wire = stringField(8);       // exactly at the cap

    sofab::IStreamObject<StringMessage<std::string>> in{kTight};
    REQUIRE(in.feed(wire.data(), wire.size()).ok());
    REQUIRE((*in).value.size() == 8);
}

/* ---------------------------------------------------------------------------
 * a wrapper array has no count header, so the cap binds the element INDEX
 * (§6.2.1, §7.2 item 8)
 * ------------------------------------------------------------------------- */

TEST_CASE("limits: a wrapper array's cap binds the element index")
{
    // "a wrapper array's length is highest present id + 1 (MESSAGE_SPEC §5.1),
    // so the index is what has to be checked, there being no count header to
    // check" -- and it is checked BEFORE the container it indexes into is
    // extended.

    SECTION("id at cap-1 decodes, and the array is cap long")
    {
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writeElement(os, 0, "a");
        writeElement(os, 3, "d");           // cap is 4, so 3 is the last legal index
        os.sequenceEnd();
        os.flush();

        sofab::IStreamObject<StringArrayMessage> in{kTight};
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE((*in).out.size() == 4);
        REQUIRE((*in).out[0] == "a");
        REQUIRE((*in).out[1].empty());        // the gap holds the element default
        REQUIRE((*in).out[3] == "d");
    }

    SECTION("id at the cap is LimitExceeded, and allocates nothing first")
    {
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writeElement(os, 0, "a");
        writeElement(os, 4, "e");           // id == cap
        os.sequenceEnd();
        os.flush();

        sofab::IStreamObject<StringArrayMessage> in{kTight};
        auto r = in.feed(os.data(), os.bytesUsed());

        REQUIRE(r.code() == sofab::Error::LimitExceeded);
        REQUIRE_FALSE(r.invalid());
        // Not extended toward the rejected index: the check runs before the grow.
        REQUIRE((*in).out.size() <= 1);
    }

    SECTION("a declared count governs instead, and its violation is INVALID")
    {
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writeElement(os, 2, "c");
        os.sequenceEnd();
        os.flush();

        sofab::IStreamObject<StringArrayMessage> in{kTight};
        (*in).seq.cap = 2;                  // the schema says: two elements
        REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::InvalidMessage);
    }

    SECTION("an element longer than the string cap is LimitExceeded")
    {
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writeElement(os, 0, std::string(9, 'a'));   // string cap is 8
        os.sequenceEnd();
        os.flush();

        sofab::IStreamObject<StringArrayMessage> in{kTight};
        REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::LimitExceeded);
    }
}

/* ---------------------------------------------------------------------------
 * a skipped field is never capped (§6.2.1, §6.7.2)
 * ------------------------------------------------------------------------- */

TEST_CASE("limits: a field the handler skips is never capped")
{
    // "A limit bounds an allocation, and a field the handler skips allocates
    // nothing -- it is walked, not materialized. A max_dyn_* limit MUST NOT be
    // applied to it, so a decode that steps over an over-cap field it was never
    // going to read stays COMPLETE."
    const auto wire = stringField(64);      // far past every cap in kTight

    struct SkipEverything final : sofab::IStreamMessage
    {
        void deserialize(sofab::IStreamImpl &, sofab::id, size_t, size_t) noexcept override { }
    };

    sofab::IStreamObject<SkipEverything> in{kTight};
    REQUIRE(in.feed(wire.data(), wire.size()).ok());
}

TEST_CASE("limits: a type-contradicting field is skipped, not capped")
{
    // MESSAGE_SPEC §7.3: the read declines before it binds, so the field is
    // walked like an unknown id. The cap must not fire on the way past.
    const auto wire = blobField(64);

    sofab::IStreamObject<StringMessage<std::string>> in{kTight};  // expects a string
    REQUIRE(in.feed(wire.data(), wire.size()).ok());
    REQUIRE((*in).value.empty());
}

/* ---------------------------------------------------------------------------
 * the codes themselves
 * ------------------------------------------------------------------------- */

TEST_CASE("limits: LimitExceeded is its own code, distinct from every other")
{
    // §6.3's five codes are five values. LimitExceeded sharing a number with
    // InvalidMessage would make every case above pass while the property they
    // are testing was absent.
    static_assert(sofab::Error::LimitExceeded != sofab::Error::InvalidMessage);
    static_assert(sofab::Error::LimitExceeded != sofab::Error::InvalidArgument);
    static_assert(sofab::Error::LimitExceeded != sofab::Error::BufferFull);
    static_assert(sofab::Error::LimitExceeded != sofab::Error::None);
    static_assert(sofab::Error::LimitExceeded != sofab::Error::Incomplete);
    static_assert(static_cast<int>(sofab::Error::LimitExceeded) == SOFAB_RET_E_LIMIT_EXCEEDED);

    SUCCEED("compile-time only");
}

TEST_CASE("limits: Limits has no unset state")
{
    // §6.2.1: "There is no unset state and no unlimited mode." The type enforces
    // it -- a Limits cannot be default-constructed, so a stream cannot exist
    // without all three numbers having been stated by whoever built it.
    static_assert(!std::is_default_constructible_v<sofab::Limits>);
    static_assert(!std::is_default_constructible_v<sofab::IStreamObject<StringArrayMessage>>);

    // And the stream carries exactly what it was given -- no number of the
    // corelib's own (ARCHITECTURE §9.5).
    sofab::IStreamObject<StringArrayMessage> in{kTight};
    REQUIRE(in.limits().max_dyn_array_count == 4);
    REQUIRE(in.limits().max_dyn_string_len == 8);
    REQUIRE(in.limits().max_dyn_blob_len == 8);

    SUCCEED("");
}
