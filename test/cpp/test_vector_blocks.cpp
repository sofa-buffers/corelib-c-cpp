/*!
 * @file test_vector_blocks.cpp
 * @brief C++ runner for the test_vectors.json blocks the shared C engine cannot
 *        execute.
 *
 * `assets/test_vectors.json` has four top-level blocks. The shared engine in
 * test/shared/ runs two of them -- `vectors` and `invalid_utf8` -- and skips the
 * other two, not because the library lacks the property but because that engine
 * is plain C:
 *
 *  - `header_limits` asserts that a declared length or count past a ceiling is
 *    answered AT the header (LimitExceeded for a receiver cap, §6.2.1; INVALID
 *    for a schema bound, MESSAGE_SPEC §7.1) and never INCOMPLETE. The plain-C API
 *    carries no §6.2.1 receiver cap at all; the C++ wrapper's
 *    readStringCapped()/readBlobCapped()/readArrayCapped() compare exactly there.
 *  - `sequence_growth` asserts the element-index bound of a growing container.
 *    The C API is statically bounded and never grows; sofab::StringSeq and
 *    sofab::MessageSeq over std::vector do.
 *
 * So this repo has been AUTHORING expectations it never executed -- the exact
 * arrangement that lets an authoring mistake travel into eleven ports unchecked.
 * This file closes that: it reads the same file the shared engine reads and runs
 * the two remaining blocks through the C++ wrapper, making this repo a port for
 * them and not only their author.
 *
 * The expectations come from the specs, never from what an implementation
 * answers. A case that fails here is a finding, not a case to adjust.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/sofab.hpp"

#include "sofab_test_json.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace {

/* ---------------------------------------------------------------------------
 * the vector file
 * ------------------------------------------------------------------------- */

/*! Parse assets/test_vectors.json once for the whole binary. */
class VectorFile
{
public:
    VectorFile()
    {
        std::FILE *f = std::fopen(SOFAB_TEST_VECTORS_PATH, "rb");
        if (f == nullptr)
        {
            error_ = "cannot open " SOFAB_TEST_VECTORS_PATH;
            return;
        }

        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        {
            text.append(buf, n);
        }
        std::fclose(f);

        char err[192] = {0};
        root_ = sofab_json_parse(text.data(), text.size(), err, sizeof(err));
        if (root_ == nullptr)
        {
            error_ = err[0] != '\0' ? err : "parse failed";
        }
    }

    ~VectorFile()
    {
        if (root_ != nullptr)
        {
            sofab_json_free(root_);
        }
    }

    VectorFile(const VectorFile &) = delete;
    VectorFile &operator=(const VectorFile &) = delete;

    const sofab_json_t *root() const noexcept { return root_; }
    const std::string &error() const noexcept { return error_; }

private:
    sofab_json_t *root_ = nullptr;
    std::string   error_;
};

const VectorFile &vectorFile()
{
    static VectorFile file;
    return file;
}

/* ---------------------------------------------------------------------------
 * small JSON conveniences
 * ------------------------------------------------------------------------- */

std::string_view jsonString(const sofab_json_t *v)
{
    if (v == nullptr || sofab_json_type(v) != SOFAB_JSON_STRING)
    {
        return {};
    }
    size_t len = 0;
    const char *s = sofab_json_string(v, &len);
    return std::string_view(s, len);
}

/*! A member that must be present and of the expected kind, or the case is
 *  malformed -- which is a failure, not a skip: a case nobody can run is worse
 *  than one that runs and disagrees. */
const sofab_json_t *require(const sofab_json_t *obj, const char *key)
{
    const sofab_json_t *v = sofab_json_get(obj, key);
    REQUIRE(v != nullptr);
    return v;
}

std::vector<uint8_t> fromHex(std::string_view hex)
{
    auto nibble = [](char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    std::vector<uint8_t> out;
    REQUIRE(hex.size() % 2 == 0);
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        const int hi = nibble(hex[i]);
        const int lo = nibble(hex[i + 1]);
        REQUIRE(hi >= 0);
        REQUIRE(lo >= 0);
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

/* ---------------------------------------------------------------------------
 * capability gating
 * ------------------------------------------------------------------------- */

/*! Does this build satisfy the tag?
 *
 *  Same contract as the shared C engine's: an UNKNOWN tag is satisfied, so a
 *  newer vector file stays loadable by an older runner. The two tags the C
 *  engine does not know are exactly the two this runner exists for, and the C++
 *  wrapper carries both. */
bool haveCapability(std::string_view tag)
{
    if (tag == "fixlen")
    {
#if defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
        return false;
#else
        return true;
#endif
    }
    if (tag == "array")
    {
#if defined(SOFAB_DISABLE_ARRAY_SUPPORT)
        return false;
#else
        return true;
#endif
    }
    if (tag == "sequence")
    {
#if defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
        return false;
#else
        return true;
#endif
    }
    if (tag == "int64")
    {
#if defined(SOFAB_DISABLE_INT64_SUPPORT)
        return false;
#else
        return true;
#endif
    }
    /* receiver_caps: readStringCapped/readBlobCapped/readArrayCapped.
     * dynamic_arrays: StringSeq/MessageSeq over std::vector.
     * Both are properties of this wrapper, and both are why the block is here. */
    return true;
}

bool satisfied(const sofab_json_t *caseObj)
{
    const sofab_json_t *req = sofab_json_get(caseObj, "requires");
    if (req == nullptr || sofab_json_type(req) != SOFAB_JSON_ARRAY)
    {
        return true;
    }
    for (size_t i = 0; i < sofab_json_array_size(req); i++)
    {
        if (!haveCapability(jsonString(sofab_json_array_at(req, i))))
        {
            return false;
        }
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * outcomes
 * ------------------------------------------------------------------------- */

sofab::Error outcomeOf(std::string_view name)
{
    if (name == "complete")       return sofab::Error::None;
    if (name == "incomplete")     return sofab::Error::Incomplete;
    if (name == "invalid")        return sofab::Error::InvalidMessage;
    if (name == "limit_exceeded") return sofab::Error::LimitExceeded;
    FAIL("unknown expect.outcome: " << std::string(name));
    return sofab::Error::InvalidMessage;
}

} // namespace

/* ===========================================================================
 * header_limits -- the ceiling answers at the length word (§6.2.1, §6.3)
 * ========================================================================= */

namespace {

/*! One field at the case's `field_id`, routed by the wire type the header
 *  carries -- which is how generated code decides too, and what keeps the §7.3
 *  tag test inside the read where §6.2.1 requires it. */
struct HeaderLimitMessage final : sofab::IStreamMessage
{
    sofab::id field  = 0;
    long      bound  = -1;  //!< what the SCHEMA declares; -1 = it declares nothing
    long      dynCap = -1;  //!< what the DEPLOYMENT configures; -1 = none

    std::string           str{};
    std::vector<uint8_t>  blob{};
    std::vector<uint32_t> arr{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t size, size_t count) noexcept override
    {
        if (id != field)
        {
            return;
        }

        switch (is.wire())
        {
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            case sofab::Wire::Fixlen:
                if (is.fixType() == sofab::Fix::String)
                {
                    if (bound >= 0)       is.readString(str, size, static_cast<size_t>(bound));
                    else if (dynCap >= 0) is.readStringCapped(str, size, static_cast<size_t>(dynCap));
                }
                else if (is.fixType() == sofab::Fix::Blob)
                {
                    if (bound >= 0)       is.readBlob(blob, size, static_cast<size_t>(bound));
                    else if (dynCap >= 0) is.readBlobCapped(blob, size, static_cast<size_t>(dynCap));
                }
                break;
#endif
#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            case sofab::Wire::ArrayUnsigned:
                if (bound >= 0)       is.readArray(arr, count, static_cast<size_t>(bound));
                else if (dynCap >= 0) is.readArrayCapped(arr, count, static_cast<size_t>(dynCap));
                break;
#endif
            default:
                break;
        }
    }

    /*! Nothing was materialized: "rejected, never clamped" (§6.2.1). */
    bool empty() const noexcept
    {
        return str.empty() && blob.empty() && arr.empty();
    }
};

/*! Read the one `limits` entry a case carries, whichever of the three it is. */
long dynCapOf(const sofab_json_t *caseObj)
{
    const sofab_json_t *limits = sofab_json_get(caseObj, "limits");
    if (limits == nullptr)
    {
        return -1;
    }
    for (const char *key : {"max_dyn_string_len", "max_dyn_blob_len", "max_dyn_array_count"})
    {
        const sofab_json_t *v = sofab_json_get(limits, key);
        if (v != nullptr)
        {
            return static_cast<long>(sofab_json_u64(v));
        }
    }
    return -1;
}

/*! Read the one `schema` bound a case carries. */
long schemaBoundOf(const sofab_json_t *caseObj)
{
    const sofab_json_t *schema = sofab_json_get(caseObj, "schema");
    if (schema == nullptr)
    {
        return -1;
    }
    for (const char *key : {"maxlen", "count"})
    {
        const sofab_json_t *v = sofab_json_get(schema, key);
        if (v != nullptr)
        {
            return static_cast<long>(sofab_json_u64(v));
        }
    }
    return -1;
}

/*! Feed `chunks` if the case states them, else `serialized` in one go.
 *
 *  A split case exists to pin that the answer does not depend on where the
 *  chunk boundary falls, so the intermediate results matter: everything before
 *  the last chunk may only be INCOMPLETE -- an early verdict would mean the
 *  decoder answered on bytes it had not seen. */
sofab::Error feedCase(sofab::IStreamObject<HeaderLimitMessage> &in, const sofab_json_t *caseObj)
{
    const sofab_json_t *chunks = sofab_json_get(caseObj, "chunks");
    if (chunks == nullptr || sofab_json_type(chunks) != SOFAB_JSON_ARRAY)
    {
        const auto bytes = fromHex(jsonString(require(caseObj, "serialized")));
        return in.feed(bytes.data(), bytes.size()).code();
    }

    const size_t n = sofab_json_array_size(chunks);
    sofab::Error last = sofab::Error::Incomplete;
    for (size_t i = 0; i < n; i++)
    {
        const auto bytes = fromHex(jsonString(sofab_json_array_at(chunks, i)));
        last = in.feed(bytes.data(), bytes.size()).code();
        if (i + 1 < n)
        {
            REQUIRE(last == sofab::Error::Incomplete);
        }
    }
    return last;
}

} // namespace

TEST_CASE("vectors: header_limits -- the ceiling answers at the length word")
{
    REQUIRE(vectorFile().error().empty());
    const sofab_json_t *block = sofab_json_get(vectorFile().root(), "header_limits");
    REQUIRE(block != nullptr);

    const size_t total = sofab_json_array_size(block);
    REQUIRE(total > 0);

    size_t ran = 0;
    for (size_t i = 0; i < total; i++)
    {
        const sofab_json_t *c = sofab_json_array_at(block, i);
        const std::string name{jsonString(require(c, "name"))};

        /* An unsatisfied tag is a SKIP here, not the reduced-build rejection a
         * vector gets: a case this build cannot express says nothing about it. */
        if (!satisfied(c))
        {
            continue;
        }

        INFO("header_limits case: " << name);
        ran++;

        sofab::IStreamObject<HeaderLimitMessage> in;
        (*in).field  = static_cast<sofab::id>(sofab_json_u64(require(c, "field_id")));
        (*in).bound  = schemaBoundOf(c);
        (*in).dynCap = dynCapOf(c);

        const sofab_json_t *expect = require(c, "expect");
        const sofab::Error wanted  = outcomeOf(jsonString(require(expect, "outcome")));

        const sofab::Error got = feedCase(in, c);
        REQUIRE(got == wanted);

        const sofab_json_t *terminal = sofab_json_get(expect, "terminal");
        if (terminal != nullptr && sofab_json_bool(terminal))
        {
            /* Terminal means further input cannot lift it. Feeding the payload
             * the header promised is the strongest form of that: those bytes
             * would complete the field if anything could. */
            const std::vector<uint8_t> more(8, 0x61);
            REQUIRE(in.feed(more.data(), more.size()).code() == wanted);

            /* ... and nothing was materialized on the way (§6.2.1: rejected,
             * never clamped). */
            REQUIRE((*in).empty());
        }
    }

    /* The block is not allowed to silently gate itself out of existence: the
     * point of this runner is that these cases RUN here. */
    REQUIRE(ran == total);
}

/* The negative control. A runner that answers correctly for an unrelated reason
 * is worse than no runner, because it reports coverage it does not have -- and
 * these cases are short, so INCOMPLETE is the answer lying in wait for every one
 * of them. Lifting the ceiling must therefore CHANGE the answer: if it does not,
 * the guard under test was never what produced the rejection.
 *
 * #164 records this from the throwaway probe that preceded this file, and it is
 * the half worth carrying over. */
TEST_CASE("vectors: header_limits -- lifting the ceiling changes the answer")
{
    REQUIRE(vectorFile().error().empty());
    const sofab_json_t *block = sofab_json_get(vectorFile().root(), "header_limits");
    REQUIRE(block != nullptr);

    /* A ceiling high enough that no case in the block reaches it, but low enough
     * that lifting it cannot ask for a real allocation of the declared size. */
    constexpr long kLifted = 1 << 16;

    size_t checked = 0;
    for (size_t i = 0; i < sofab_json_array_size(block); i++)
    {
        const sofab_json_t *c = sofab_json_array_at(block, i);
        if (!satisfied(c))
        {
            continue;
        }

        const sofab::Error wanted =
            outcomeOf(jsonString(require(sofab_json_get(c, "expect"), "outcome")));
        if (wanted != sofab::Error::LimitExceeded && wanted != sofab::Error::InvalidMessage)
        {
            continue;   /* only a rejection can be shown to depend on its ceiling */
        }

        const std::string name{jsonString(require(c, "name"))};
        INFO("header_limits case, ceiling lifted: " << name);

        /* The amplification case declares a gigabyte. Lifting its ceiling past
         * that is exactly the allocation §6.2.1 exists to prevent, so it is the
         * one case whose control cannot be run this way -- which is itself the
         * point the case makes. */
        const long declared = static_cast<long>(sofab_json_u64(require(c, "declared")));
        if (declared > kLifted)
        {
            continue;
        }

        sofab::IStreamObject<HeaderLimitMessage> in;
        (*in).field = static_cast<sofab::id>(sofab_json_u64(require(c, "field_id")));
        if (sofab_json_get(c, "schema") != nullptr)
        {
            (*in).bound = kLifted;
        }
        else
        {
            (*in).dynCap = kLifted;
        }

        checked++;
        REQUIRE(feedCase(in, c) != wanted);
    }

    /* Five of the six rejections; the sixth is the amplification case above. */
    REQUIRE(checked == 5);
}

/* ===========================================================================
 * sequence_growth -- the element-index bound of a container that grows
 * ========================================================================= */

namespace {

/*! The block's `struct` element: a framed sub-sequence carrying one `unsigned`
 *  at id 0 (test_vectors_README, "Growth cases"). */
struct GrowthStruct final : sofab::IStreamMessage
{
    uint32_t value = 0;

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        if (id == 0)
        {
            is.read(value);
        }
    }
};

/*! A `string` wrapper array into growable storage, capped on the index. */
struct GrowthStrings final : sofab::IStreamMessage
{
    sofab::id                field = 0;
    sofab::StringSeq         seq{};
    std::vector<std::string> out{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        if (id == field)
        {
            is.readSequence(seq, out);
        }
    }
};

/*! The `struct` counterpart. */
struct GrowthStructs final : sofab::IStreamMessage
{
    sofab::id                                  field = 0;
    sofab::MessageSeq<std::vector<GrowthStruct>> seq{};
    std::vector<GrowthStruct>                  out{};

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        if (id == field)
        {
            is.readSequence(seq, out);
        }
    }
};

/*! The receiver cap this runner configures for the block.
 *
 *  The cases name no absolute boundary -- `id_from_cap` and `length_from_cap`
 *  are offsets added to whatever the port configures, because §6.2.1 fixes no
 *  family-wide number. Four is the minimum the block assumes. */
constexpr size_t kGrowthCap = 4;

/*! An element length cap, so the element read has a ceiling of its own. The
 *  block's values are single characters; this is not what any case tests. */
constexpr size_t kGrowthElemMax = 64;

/*! Resolve an element's index: `id` is absolute, `id_from_cap` is cap-relative. */
uint32_t deliverIndex(const sofab_json_t *elem)
{
    const sofab_json_t *abs = sofab_json_get(elem, "id");
    if (abs != nullptr)
    {
        return static_cast<uint32_t>(sofab_json_u64(abs));
    }
    const sofab_json_t *rel = require(elem, "id_from_cap");
    return static_cast<uint32_t>(static_cast<long>(kGrowthCap) + static_cast<long>(sofab_json_i64(rel)));
}

/*! Resolve `length` / `length_from_cap` the same way. */
size_t expectedLength(const sofab_json_t *expect)
{
    const sofab_json_t *abs = sofab_json_get(expect, "length");
    if (abs != nullptr)
    {
        return static_cast<size_t>(sofab_json_u64(abs));
    }
    const sofab_json_t *rel = require(expect, "length_from_cap");
    return static_cast<size_t>(static_cast<long>(kGrowthCap) + static_cast<long>(sofab_json_i64(rel)));
}

/*! Frame the case's `deliver` list as a wrapper array at `field_id`.
 *
 *  The port builds the message itself: the positive suite is structurally blind
 *  to growth, so there is no `serialized.hex` that could tell two collectors
 *  apart, and the block carries element intents instead of bytes. */
void buildGrowthMessage(sofab::OStream &os, const sofab_json_t *caseObj, bool asStruct)
{
    const auto field = static_cast<sofab::id>(sofab_json_u64(require(caseObj, "field_id")));
    const sofab_json_t *deliver = require(caseObj, "deliver");

    os.sequenceBeginLazy(field);
    for (size_t i = 0; i < sofab_json_array_size(deliver); i++)
    {
        const sofab_json_t *elem = sofab_json_array_at(deliver, i);
        const uint32_t index = deliverIndex(elem);
        const sofab_json_t *value = require(elem, "value");

        if (asStruct)
        {
            os.sequenceBeginLazy(index);
            os.write(0, static_cast<uint32_t>(sofab_json_u64(value)));
            os.sequenceEnd();
        }
        else
        {
            os.write(index, std::string(jsonString(value)));
        }
    }
    os.sequenceEnd();
    os.flush();
}

} // namespace

TEST_CASE("vectors: sequence_growth -- the index bound of a growing container")
{
    REQUIRE(vectorFile().error().empty());
    const sofab_json_t *block = sofab_json_get(vectorFile().root(), "sequence_growth");
    REQUIRE(block != nullptr);

    const size_t total = sofab_json_array_size(block);
    REQUIRE(total > 0);

    size_t ran = 0;
    for (size_t i = 0; i < total; i++)
    {
        const sofab_json_t *c = sofab_json_array_at(block, i);
        const std::string name{jsonString(require(c, "name"))};

        if (!satisfied(c))
        {
            continue;
        }

        INFO("sequence_growth case: " << name);
        ran++;

        const bool asStruct = jsonString(require(c, "element_type")) == "struct";
        const auto field = static_cast<sofab::id>(sofab_json_u64(require(c, "field_id")));

        sofab::OStream os{512};
        buildGrowthMessage(os, c, asStruct);

        const sofab_json_t *expect = require(c, "expect");
        const sofab::Error wanted  = outcomeOf(jsonString(require(expect, "outcome")));

        /* Assert the container LENGTH and the OUTCOME, and nothing else -- no
         * allocator instrumentation. That is what makes the cases portable
         * across eleven languages (CORELIB_PLAN §7.2 item 8). */
        sofab::Error got   = sofab::Error::None;
        size_t       length = 0;
        std::vector<uint32_t> defaults{};

        if (asStruct)
        {
            sofab::IStreamObject<GrowthStructs> in;
            (*in).field        = field;
            (*in).seq.dynCap   = sofab::DynCap{kGrowthCap};
            got    = in.feed(os.data(), os.bytesUsed()).code();
            length = (*in).out.size();
            for (size_t k = 0; k < length; k++)
            {
                if ((*in).out[k].value == 0) defaults.push_back(static_cast<uint32_t>(k));
            }
            if (wanted == sofab::Error::LimitExceeded)
            {
                const uint8_t more[] = {0x00, 0x01};
                REQUIRE(in.feed(more, sizeof(more)).code() == wanted);
            }
        }
        else
        {
            sofab::IStreamObject<GrowthStrings> in;
            (*in).field           = field;
            (*in).seq.dynCap      = sofab::DynCap{kGrowthCap};
            (*in).seq.dynElemMax  = sofab::DynCap{kGrowthElemMax};
            got    = in.feed(os.data(), os.bytesUsed()).code();
            length = (*in).out.size();
            for (size_t k = 0; k < length; k++)
            {
                if ((*in).out[k].empty()) defaults.push_back(static_cast<uint32_t>(k));
            }
            if (wanted == sofab::Error::LimitExceeded)
            {
                const uint8_t more[] = {0x00, 0x01};
                REQUIRE(in.feed(more, sizeof(more)).code() == wanted);
            }
        }

        REQUIRE(got == wanted);

        if (wanted == sofab::Error::None)
        {
            REQUIRE(length == expectedLength(expect));

            const sofab_json_t *defaultIds = sofab_json_get(expect, "default_ids");
            if (defaultIds != nullptr)
            {
                for (size_t k = 0; k < sofab_json_array_size(defaultIds); k++)
                {
                    const auto want = static_cast<uint32_t>(
                        sofab_json_u64(sofab_json_array_at(defaultIds, k)));
                    INFO("element " << want << " must hold the element default");
                    REQUIRE(std::find(defaults.begin(), defaults.end(), want) != defaults.end());
                }
            }
        }
        else
        {
            /* Rejected, never clamped: the container must not have been extended
             * past what the case allows. */
            const sofab_json_t *maxLength = sofab_json_get(expect, "max_length");
            if (maxLength != nullptr)
            {
                REQUIRE(length <= static_cast<size_t>(sofab_json_u64(maxLength)));
            }
        }
    }

    REQUIRE(ran == total);
}
