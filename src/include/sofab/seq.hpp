/*!
 * @file seq.hpp
 * @brief SofaBuffers C++ - wrapper-array collectors for object and row elements.
 *
 * This is the **static-helper layer** of the C++ API: code whose shape is the
 * same for every schema, so it is written once here instead of being emitted
 * into every generated header. Nothing here knows a schema. A `count`, a
 * capacity or an element width arrives as a template argument or a member, i.e.
 * as an ordinary runtime value — exactly the way @ref sofab::StringSeq already
 * takes its `cap` and `elemMax`.
 *
 * What lives here is the collector for the two element kinds the four
 * string/blob collectors in `sofab.hpp` do not cover: a struct/union element,
 * and a row of native scalars (a matrix). Their placement rule is the shared
 * one — MESSAGE_SPEC §5.1 makes an element's child id its array index — and it
 * is the rule an appending collector gets wrong, so it is spelled out once, on
 * @ref sofab::FixedMessageSeq, and referred to from the growable twin.
 *
 * Include `sofab/sofab.hpp`; it pulls this in. Including this header alone
 * works too and means the same thing.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_SEQ_HPP
#define SOFAB_SEQ_HPP

/**
 * @defgroup cpp_api C++ API
 * @{
 */

/* includes *******************************************************************/
#include "sofab/sofab.hpp"

#include <cstddef>
#include <type_traits>

/* types **********************************************************************/
namespace sofab
{
    /*!
     * @brief Wire type an element of type @p Elem arrives with, or -1 for none.
     *
     * MESSAGE_SPEC §7.3 bounds an element by the wire type its @b declared type
     * implies, and the collectors below must apply that check before they touch
     * the destination — one step earlier than @ref IStreamImpl::read or
     * @ref IStreamImpl::readArray would settle it for themselves. The two must
     * not be able to disagree, so the choice made here mirrors readArray's
     * exactly: a struct/union element is a nested sequence, and a native-scalar
     * row is the array wire type its element kind selects.
     *
     * -1 means the element type implies no single wire type; the check is then
     * left to the read, which still refuses to bind a contradicting field.
     *
     * @tparam Elem  Element type (an @ref IStreamMessage, or a row container).
     */
    template <typename Elem>
    inline constexpr int seq_elem_wire_v =
        [] () constexpr -> int
        {
            if constexpr (std::is_base_of_v<IStreamMessage, Elem>)
            {
                return static_cast<int>(Wire::SequenceStart);
            }
            else if constexpr (requires { typename Elem::value_type; })
            {
                using RowElem = typename Elem::value_type;
                if constexpr (std::is_same_v<RowElem, float> || std::is_same_v<RowElem, double>)
                {
                    return static_cast<int>(Wire::ArrayFixlen);
                }
                else if constexpr (std::is_integral_v<RowElem>)
                {
                    return static_cast<int>(
                        std::is_signed_v<RowElem> ? Wire::ArraySigned : Wire::ArrayUnsigned);
                }
                else
                {
                    return -1;
                }
            }
            else
            {
                return -1;
            }
        }();

    /*!
     * @brief Fixlen sub-type an element of type @p Elem arrives with, or -1.
     *
     * The companion to @ref seq_elem_wire_v for the one wire type that is not
     * self-describing: @c fp32 and @c fp64 rows share @ref Wire::ArrayFixlen, so
     * §7.3 only separates them at the sub-type. -1 wherever the wire type already
     * settles the element kind.
     *
     * @tparam Elem  Element type (an @ref IStreamMessage, or a row container).
     */
    template <typename Elem>
    inline constexpr int seq_elem_fix_v =
        [] () constexpr -> int
        {
            if constexpr (!std::is_base_of_v<IStreamMessage, Elem>
                          && requires { typename Elem::value_type; })
            {
                using RowElem = typename Elem::value_type;
                if constexpr (std::is_same_v<RowElem, float>)
                {
                    return static_cast<int>(Fix::Fp32);
                }
                else if constexpr (std::is_same_v<RowElem, double>)
                {
                    return static_cast<int>(Fix::Fp64);
                }
                else
                {
                    return -1;
                }
            }
            else
            {
                return -1;
            }
        }();

    /*!
     * @brief Collects a struct/union or nested-array wrapper sequence into
     *        inline storage.
     *
     * The object counterpart of @ref FixedStringSeq: same placement rule, same
     * bound, filling elements that are messages (a struct or union) or rows of
     * native scalars (a matrix) rather than @ref FixedString slots. Bind it the
     * way its four siblings are bound, through
     * @ref IStreamImpl::readSequence.
     *
     * **An element is placed at its index id, never appended.** MESSAGE_SPEC
     * §5.1 makes the element's child id the array index, and the ids may have
     * gaps: an @b interior element equal to the element default is left off the
     * wire by a conformant encoder (§2), so the container is grown with default
     * elements up to the id and the value stored there. Appending instead would
     * silently @b shorten the array by every gap — elements at id 0 and id 2 are
     * the 3-element array `[a, default, c]`, not `[a, c]` — and would turn a
     * reopened element id into a second element instead of continuing the first
     * (§7.4). The array's last element is always on the wire, so the decoded
     * length (highest present id + 1) is exact; the schema `count` is a
     * @b capacity and never adds an element the wire did not carry.
     *
     * An index at or past the fixed capacity N is a schema-bound violation
     * (§5.1/§7) and is rejected with @ref IStreamImpl::invalidate, @b before the
     * fill loop. That also bounds an over-index amplification: @ref InlineVector
     * reuses its last slot once full, so an unguarded fill loop would spin
     * forever on such an index (issue #126), and no announced index can make the
     * receiver reserve anything.
     *
     * An element whose wire type contradicts the declared one is skipped exactly
     * like an unknown id (§7.3), which means it must leave the container
     * untouched — so that decision, too, comes before the fill.
     *
     * Inline storage never reallocates, so an element bound earlier stays
     * address-stable while later slots grow, which the deferred C decoder relies
     * on.
     *
     * @tparam Container Inline vector of messages, or of rows.
     */
    template <typename Container>
    struct FixedMessageSeq : IStreamMessage
    {
        /*! @brief Element type: an @ref IStreamMessage, or a row container. */
        using Elem = typename Container::value_type;

        /*! @brief Declared element wire type, or -1 (@ref seq_elem_wire_v). */
        static constexpr int elemWire = seq_elem_wire_v<Elem>;
        /*! @brief Declared element fixlen sub-type, or -1 (@ref seq_elem_fix_v). */
        static constexpr int elemFix = seq_elem_fix_v<Elem>;

        static_assert(fixed_capacity_v<Container> >= 0,
            "FixedMessageSeq needs a heap-free container: its capacity IS the "
            "schema bound this collector applies. Use sofab::MessageSeq, with an "
            "explicit cap, for a growable one.");

        Container *out = nullptr;  //!< Destination, bound by @ref IStreamImpl::readSequence.

        void deserialize(IStreamImpl &is, sofab_id_t id, size_t, size_t count) noexcept override
        {
            /* §7.3 first: an element whose wire type contradicts the declared one
             * is not this array's element at all. It is skipped like an unknown
             * id, which means the destination must be left exactly as it was --
             * so this comes before the fill below, and an id that is not an index
             * cannot breach the index bound either. */
            if constexpr (elemWire >= 0)
            {
                if (static_cast<int>(is.wire()) != elemWire) return;
            }
            if constexpr (elemFix >= 0)
            {
                if (static_cast<int>(is.fixType()) != elemFix) return;
            }
            if (static_cast<size_t>(id) >= static_cast<size_t>(fixed_capacity_v<Container>))
            {
                is.invalidate();
                return;
            }
            while (out->size() <= static_cast<size_t>(id)) (void)out->emplace_back();
            Elem &elem = (*out)[static_cast<size_t>(id)];
            if constexpr (std::is_base_of_v<IStreamMessage, Elem>)
            {
                is.read(elem); /* the element's own sub-sequence */
            }
            else
            {
                /* A native-scalar row goes through readArray rather than a bare
                 * resize() + read(): readArray owns the row's tag check, sizes the
                 * row from the wire count and refuses a count the row cannot hold
                 * instead of truncating into it (§7.1). A heap-free row's capacity
                 * IS the schema `count` it was generated for, so it is passed as
                 * the bound. */
                is.readArray(elem, count);
            }
        }
    };

    /*!
     * @brief Collects a struct/union or nested-array wrapper sequence into a
     *        growable container.
     *
     * The heap counterpart of @ref FixedMessageSeq, for the `allow_dynamic`
     * storage mode, and the object counterpart of @ref StringSeq: the schema's
     * `count` still binds, it just is not the container's capacity any more, so
     * it is carried in @ref cap. Placement, gap fill and the §7.3 skip are
     * @ref FixedMessageSeq's, unchanged and documented there; only the bound
     * moves. Named to match `sofab::MessageSeq` in corelib-cpp so both C++
     * outputs read alike — the generator emits one call site for the two.
     *
     * A growable row is bounded by the wire count alone: unlike an inline row it
     * publishes no capacity, and the schema `count` of the inner array is not
     * something the outer collector is told.
     *
     * Growing the container may reallocate it, which the deferred decoder would
     * not survive if it moved a destination already bound. It cannot: an element
     * is placed only when its own child field opens, i.e. once the element before
     * it has closed and every target inside it has been filled. That is the same
     * reasoning @c read(std::vector<std::string>&) relies on; @ref
     * FixedMessageSeq does not need it at all, since inline storage never moves.
     *
     * @tparam Container Growable container of messages, or of rows.
     */
    template <typename Container>
    struct MessageSeq : IStreamMessage
    {
        /*! @brief Element type: an @ref IStreamMessage, or a row container. */
        using Elem = typename Container::value_type;

        /*! @brief Declared element wire type, or -1 (@ref seq_elem_wire_v). */
        static constexpr int elemWire = seq_elem_wire_v<Elem>;
        /*! @brief Declared element fixlen sub-type, or -1 (@ref seq_elem_fix_v). */
        static constexpr int elemFix = seq_elem_fix_v<Elem>;

        Container *out = nullptr;  //!< Destination, bound by @ref IStreamImpl::readSequence.
        long cap = -1;             //!< Schema `count` N, or -1 when the schema declares none.
        long dynCap = -1;          //!< §6.2.1 receiver cap on the index; only where @ref cap is -1.

        void deserialize(IStreamImpl &is, sofab_id_t id, size_t, size_t count) noexcept override
        {
            if constexpr (elemWire >= 0)
            {
                if (static_cast<int>(is.wire()) != elemWire) return; /* §7.3 */
            }
            if constexpr (elemFix >= 0)
            {
                if (static_cast<int>(is.fixType()) != elemFix) return; /* §7.3 */
            }
            /* §5.1/§7, decided from the id alone and before the container grows --
             * which is what keeps an announced index near 2^31 from becoming an
             * allocation. A wrapper array's length is highest present id + 1
             * (MESSAGE_SPEC §5.1), so that is what the bound is applied to. Where
             * the schema declares no `count`, dynCap does (§6.2.1) -- there is no
             * capacity here to refuse with, exactly as in @ref StringSeq. */
            if (seqRefuse(is, static_cast<size_t>(id) + 1, cap, dynCap))
            {
                return;
            }
            while (out->size() <= static_cast<size_t>(id)) (void)out->emplace_back();
            Elem &elem = (*out)[static_cast<size_t>(id)];
            if constexpr (std::is_base_of_v<IStreamMessage, Elem>)
            {
                is.read(elem);
            }
            else
            {
                /* A growable row publishes no capacity and the outer collector is
                 * not told the inner array's `count`, so the wire count is the only
                 * bound left. A cap on it is generated code's to apply. */
                is.readArray(elem, count);
            }
        }
    };
};

/** @} */ // end of defgroup

#endif // SOFAB_SEQ_HPP
