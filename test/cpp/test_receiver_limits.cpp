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
 * @par Who holds the cap
 * Not this corelib. §6.2.1 gives the numbers to generated code — "an element
 * count trivial on a server is brutal in C" — and leaves the codec "the report
 * and the category […] the visitor decides". So the messages below hand their cap
 * over per call, which is what the generator emits:
 *
 *     is.readString(name, _size, -1, SOFAB_MAX_DYN_STRING_LEN);
 *
 * Passing it beats checking it before the call, and the difference is not style:
 * `readString` applies §7.3 first, so a field whose wire type contradicts the
 * read is skipped before any ceiling is consulted. A caller-side `if (_size >
 * CAP) exceedLimit();` sits in front of that check and caps a field that was
 * about to be skipped — which §6.2.1 forbids in as many words. The last two cases
 * in this file pin exactly that.
 *
 * A growable wrapper array is the same rule at one remove: it announces no count
 * and delivers no callback at the element index, so the number is set on the
 * collector (`seq.dynCap`) and applied there, still never invented.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace
{

/* The receiver's policy for these cases: deliberately tight, so a cap breach is
 * a few bytes of test data rather than a megabyte. These stand in for the
 * SOFAB_MAX_DYN_* defines generated code carries. */
constexpr long kDynArrayCount = 4;
constexpr long kDynStringLen  = 8;
constexpr long kDynBlobLen    = 8;

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
 * A message with one `string` field at id 1, whose declared `maxlen`, whose
 * configured cap and whose destination shape are all chosen by the test.
 *
 * `deserialize` is written the way the generator emits it: one read call, with
 * the schema bound and the receiver cap both handed over as arguments.
 *
 * @tparam Dst  `std::string` (growable) or `sofab::FixedString<N>` (heap-free).
 */
/*! Apply the two message-side bounds the way generated code does — in the
 *  handler, at the `size` / `count` the field callback reports, before any
 *  destination is bound.
 *
 *  CORELIB_PLAN §6.2.1 leaves the codec "the report and the category": "the
 *  visitor decides. The codec never invents a limit of its own and never clamps
 *  to one." MESSAGE_SPEC §7 says the same about the schema bound, because "the
 *  corelib cannot know the schema". So both live here, and the corelib is asked
 *  only to carry the verdict.
 *
 *  @return `true` when the field was refused and must not be read.
 */
inline bool refusedByHandler(sofab::IStreamImpl &is, size_t n, long schemaBound, long dynCap) noexcept
{
    if (schemaBound >= 0)
    {
        if (n > static_cast<size_t>(schemaBound)) { is.invalidate(); return true; }
        return false;
    }
    if (dynCap >= 0 && n > static_cast<size_t>(dynCap)) { is.exceedLimit(); return true; }
    return false;
}

template <typename Dst>
struct StringMessage final : sofab::IStreamMessage
{
    long maxlen = -1;   //!< what the SCHEMA declares; -1 = it declares nothing
    long dynCap = -1;   //!< what the DEPLOYMENT configures; -1 = none supplied
    Dst value{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t) noexcept override
    {
        if (id != 1) return;
        /* §7.3 FIRST. Moving the bounds out of the codec moves this with them:
         * `readString` used to make the type test before it consulted any
         * ceiling, so a field about to be skipped was never measured. A handler
         * that checks its cap above this line caps a field it is about to walk
         * past -- which §6.2.1 forbids in as many words, and which is the shape
         * generator#420 is about. */
        if (is.wire() != sofab::Wire::Fixlen || is.fixType() != sofab::Fix::String) return;
        if (refusedByHandler(is, size, maxlen, dynCap)) return;
        is.readString(value, size);
    }
};

/*! The `blob` counterpart of @ref StringMessage. */
template <typename Dst>
struct BlobMessage final : sofab::IStreamMessage
{
    long maxlen = -1;
    long dynCap = -1;
    Dst value{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t) noexcept override
    {
        if (id != 1) return;
        if (is.wire() != sofab::Wire::Fixlen || is.fixType() != sofab::Fix::Blob) return; /* §7.3 first */
        if (refusedByHandler(is, size, maxlen, dynCap)) return;
        is.readBlob(value, size);
    }
};

/*! The array counterpart: one `u32` array field at id 1. */
template <typename Dst>
struct ArrayMessage final : sofab::IStreamMessage
{
    long cap = -1;      //!< the schema's `count`; -1 = it declares none
    long dynCap = -1;
    Dst value{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t count) noexcept override
    {
        if (id != 1) return;
        if (is.wire() != sofab::Wire::ArrayUnsigned) return; /* §7.3 first */
        if (refusedByHandler(is, count, cap, dynCap)) return;
        is.readArray(value, count);
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

    sofab::IStreamObject<StringMessage<std::string>> in;
    // the schema declares no maxlen -- which is what arms the receiver cap
    (*in).maxlen = -1;
    (*in).dynCap = kDynStringLen;

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

    sofab::IStreamObject<StringMessage<std::string>> loose;
    (*loose).dynCap = 1024;
    auto r = loose.feed(wire.data(), wire.size());

    REQUIRE(r.ok());
    REQUIRE((*loose).value == std::string(9, 'a'));
}

TEST_CASE("limits: an unbounded blob past the cap is LimitExceeded")
{
    const auto wire = blobField(9);     // blob cap is 8

    sofab::IStreamObject<BlobMessage<std::vector<uint8_t>>> in;
    (*in).maxlen = -1;
    (*in).dynCap = kDynBlobLen;

    REQUIRE(in.feed(wire.data(), wire.size()).code() == sofab::Error::LimitExceeded);
    REQUIRE((*in).value.empty());
}

TEST_CASE("limits: string and blob are separate caps")
{
    // §6.2.1 lists max_dyn_string_len and max_dyn_blob_len as two limits, not
    // one: a deployment may well take a megabyte of opaque bytes and no such
    // quantity of text. A port that shares one number passes every case above
    // and fails this one.
    const auto text = stringField(8);
    const auto bytes = blobField(8);

    sofab::IStreamObject<StringMessage<std::string>> s;
    (*s).dynCap = 4;
    REQUIRE(s.feed(text.data(), text.size()).code() == sofab::Error::LimitExceeded);

    sofab::IStreamObject<BlobMessage<std::vector<uint8_t>>> b;
    (*b).dynCap = 64;
    REQUIRE(b.feed(bytes.data(), bytes.size()).ok());
    REQUIRE((*b).value.size() == 8);
}

TEST_CASE("limits: an unbounded array past the cap is LimitExceeded")
{
    const auto wire = arrayField(5);    // array cap is 4

    sofab::IStreamObject<ArrayMessage<std::vector<uint32_t>>> in;
    (*in).cap = -1;
    (*in).dynCap = kDynArrayCount;

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

    sofab::IStreamObject<StringMessage<std::string>> in;
    (*in).dynCap = kDynStringLen;
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

    sofab::IStreamObject<StringMessage<std::string>> in;
    (*in).dynCap = kDynStringLen;
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

        sofab::IStreamObject<StringMessage<std::string>> in;
        (*in).maxlen = 64;
        (*in).dynCap = kDynStringLen;

        REQUIRE(in.feed(wire.data(), wire.size()).ok());
        REQUIRE((*in).value == std::string(32, 'a'));
    }

    SECTION("past the schema bound: INVALID, never LimitExceeded")
    {
        // maxlen 4, field 9 bytes -- also past the cap of 8, so a port that
        // checked the cap first would answer LimitExceeded and promise a limit
        // to raise. The schema decides, and it says these bytes are invalid.
        const auto wire = stringField(9);

        sofab::IStreamObject<StringMessage<std::string>> in;
        (*in).maxlen = 4;
        (*in).dynCap = kDynStringLen;

        auto r = in.feed(wire.data(), wire.size());
        REQUIRE(r.code() == sofab::Error::InvalidMessage);
        REQUIRE(r.invalid());
        REQUIRE_FALSE(r.limitExceeded());
    }

    SECTION("the same holds for an array count")
    {
        const auto wire = arrayField(8);    // array cap is 4

        sofab::IStreamObject<ArrayMessage<std::vector<uint32_t>>> ok;
        (*ok).cap = 16;                     // the schema bounds it: the cap is inert
        (*ok).dynCap = kDynArrayCount;
        REQUIRE(ok.feed(wire.data(), wire.size()).ok());
        REQUIRE((*ok).value.size() == 8);

        sofab::IStreamObject<ArrayMessage<std::vector<uint32_t>>> bad;
        (*bad).cap = 2;                     // and its violation is INVALID
        (*bad).dynCap = kDynArrayCount;
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

    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> in;
    (*in).maxlen = -1;
    (*in).dynCap = kDynStringLen;

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
    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> small;
    (*small).dynCap = kDynStringLen;
    const auto six = stringField(6);        // 4 < 6 <= 8
    REQUIRE(small.feed(six.data(), six.size()).code() == sofab::Error::InvalidArgument);

    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> big;
    (*big).dynCap = kDynStringLen;
    const auto nine = stringField(9);       // 8 < 9
    REQUIRE(big.feed(nine.data(), nine.size()).code() == sofab::Error::LimitExceeded);
}

TEST_CASE("limits: tier 1 still wins over tier 3")
{
    // A schema bound the field breaks is INVALID even when the destination is
    // also too small: the message is malformed, and that is the truer statement.
    const auto wire = stringField(6);

    sofab::IStreamObject<StringMessage<sofab::FixedString<4>>> in;
    (*in).maxlen = 5;

    REQUIRE(in.feed(wire.data(), wire.size()).code() == sofab::Error::InvalidMessage);
}

TEST_CASE("limits: an over-capacity array count is refused, not silently truncated")
{
    // A heap-free container's resize() CLAMPS, so without the tier-3 check the
    // surplus elements would go missing and the decode would report COMPLETE --
    // a short array presented as the whole value.
    const auto wire = arrayField(3);        // array cap is 4: the count clears it

    sofab::IStreamObject<ArrayMessage<sofab::InlineVector<uint32_t, 2>>> in;
    (*in).cap = -1;
    (*in).dynCap = kDynArrayCount;

    auto r = in.feed(wire.data(), wire.size());
    REQUIRE(r.code() == sofab::Error::InvalidArgument);
    REQUIRE((*in).value.size() == 0);
}

TEST_CASE("limits: a growable destination never reaches tier 3")
{
    // It can hold whatever tiers 1 and 2 let through, so the only two answers a
    // std::string can give are ok and LimitExceeded.
    const auto wire = stringField(8);       // exactly at the cap

    sofab::IStreamObject<StringMessage<std::string>> in;
    (*in).dynCap = kDynStringLen;
    REQUIRE(in.feed(wire.data(), wire.size()).ok());
    REQUIRE((*in).value.size() == 8);
}

/* ---------------------------------------------------------------------------
 * a wrapper array has no count header, so the cap binds the element INDEX
 * (§6.2.1, §7.2 item 8) -- and this is the ONE place the corelib applies a cap
 * itself, from the number generated code set on the collector.
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

        sofab::IStreamObject<StringArrayMessage> in;
        (*in).seq.dynCap = kDynArrayCount;
        (*in).seq.dynElemMax = kDynStringLen;
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

        sofab::IStreamObject<StringArrayMessage> in;
        (*in).seq.dynCap = kDynArrayCount;
        (*in).seq.dynElemMax = kDynStringLen;
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

        sofab::IStreamObject<StringArrayMessage> in;
        (*in).seq.cap = 2;                  // the schema says: two elements
        (*in).seq.dynCap = kDynArrayCount;  // and the cap must stay out of it
        REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::InvalidMessage);
    }

    SECTION("an element longer than the string cap is LimitExceeded")
    {
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writeElement(os, 0, std::string(9, 'a'));   // string cap is 8
        os.sequenceEnd();
        os.flush();

        sofab::IStreamObject<StringArrayMessage> in;
        (*in).seq.dynCap = kDynArrayCount;
        (*in).seq.dynElemMax = kDynStringLen;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).code() == sofab::Error::LimitExceeded);
    }

    SECTION("no cap supplied: the collector invents none of its own")
    {
        // §6.2.1: "The codec never invents a limit of its own." A collector left
        // without a dynCap applies nothing -- the omission is generated code's
        // defect to have, and the corelib does not paper over it with a number
        // it made up.
        sofab::OStream os{256};
        os.sequenceBeginLazy(1);
        writeElement(os, 9, "j");           // far past kDynArrayCount
        os.sequenceEnd();
        os.flush();

        sofab::IStreamObject<StringArrayMessage> in;
        REQUIRE(in.feed(os.data(), os.bytesUsed()).ok());
        REQUIRE((*in).out.size() == 10);
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
    const auto wire = stringField(64);      // far past every cap here

    struct SkipEverything final : sofab::IStreamMessage
    {
        void deserialize(sofab::IStreamImpl &, sofab::id, size_t, size_t) noexcept override { }
    };

    sofab::IStreamObject<SkipEverything> in;
    REQUIRE(in.feed(wire.data(), wire.size()).ok());
}

TEST_CASE("limits: a type-contradicting field is skipped, not capped")
{
    // MESSAGE_SPEC §7.3: the read declines before it binds, so the field is
    // walked like an unknown id. The cap must not fire on the way past -- which
    // is why the guard sits inside the id arm and not above the switch.
    const auto wire = blobField(64);

    sofab::IStreamObject<StringMessage<std::string>> in;  // expects a string
    (*in).dynCap = kDynStringLen;
    REQUIRE(in.feed(wire.data(), wire.size()).ok());
    REQUIRE((*in).value.empty());
}

/* ---------------------------------------------------------------------------
 * the codes themselves, and who owns the numbers
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

TEST_CASE("limits: the corelib holds no limit of its own")
{
    // §6.2.1: the numbers "are not the codec's" -- they come from generated code,
    // which knows the schema and the target. A stream therefore takes none and
    // stores none; what this corelib contributes is exceedLimit() and the
    // category it raises. A future refactor that parks a max_dyn_* default on the
    // stream would pass every case above and break this one.
    static_assert(std::is_default_constructible_v<sofab::IStreamObject<StringArrayMessage>>);

    // A read with no cap passed applies none: 64 bytes through a stream that was
    // told nothing decodes, because there is no ceiling anywhere to consult.
    sofab::IStreamObject<StringMessage<std::string>> uncapped;
    const auto wire = stringField(64);
    REQUIRE(uncapped.feed(wire.data(), wire.size()).ok());
    REQUIRE((*uncapped).value.size() == 64);

    // The cap is an argument, defaulted to "none supplied" rather than to a
    // number the library chose -- and data on the collector, per decode.
    static_assert(sofab::StringSeq{}.dynCap == -1);
    static_assert(sofab::StringSeq{}.dynElemMax == -1);
    static_assert(sofab::BlobSeq{}.dynCap == -1);

    SUCCEED("compile-time only");
}
