/*!
 * @file floats.hpp
 * @brief SofaBuffers C++ - bit-pattern equality for float arrays.
 *
 * Part of the **static-helper layer** (see @ref seq.hpp): code whose shape is
 * the same for every schema, so it lives here once instead of being emitted
 * into every generated header.
 *
 * A generated encoder omits a field iff its value equals its declared default
 * (MESSAGE_SPEC §2), and floats round-trip bit for bit (CORELIB_PLAN §4.6).
 * "Equals" for a float array is therefore a comparison of IEEE-754 bit
 * patterns, never of values: `-0.0` differs from `+0.0`, and a NaN equals
 * another NaN only when every bit, payload included, is identical. The
 * container `operator==` (an elementwise IEEE `==`) gets both wrong and would
 * drop a `-0.0` element that has to reach the wire.
 *
 * The helper is header-only, inline and stateless: a translation unit that
 * does not call it pays 0 bytes, and the C library is untouched.
 *
 * Include `sofab/sofab.hpp`; it pulls this in. Including this header alone
 * works too and means the same thing.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_FLOATS_HPP
#define SOFAB_FLOATS_HPP

/**
 * @addtogroup cpp_api
 * @{
 */

/* includes *******************************************************************/
#include <cstddef>
#include <cstring>
#include <iterator>
#include <type_traits>

/* functions ******************************************************************/
namespace sofab
{
    /*!
     * @brief True iff both float arrays have the same length and the same bit
     *        pattern at every index.
     *
     * The length is compared first (early exit); the elements are then compared
     * with one block `memcmp` over `n * sizeof(T)` bytes, so there is no IEEE
     * `==` anywhere. Nothing is allocated or modified.
     *
     * @tparam T  `float` (32-bit pattern) or `double` (64-bit pattern).
     * @param a   First array, or any pointer when @p na is 0.
     * @param na  Length of @p a in elements.
     * @param b   Second array, or any pointer when @p nb is 0.
     * @param nb  Length of @p b in elements.
     */
    template <typename T>
    inline bool bitsEqual(const T *a, std::size_t na, const T *b, std::size_t nb) noexcept
    {
        static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                      "bitsEqual compares float or double arrays");
        if (na != nb)
        {
            return false;
        }
        // memcmp with a null pointer is undefined even for a zero length, and an
        // empty std::vector may hand one out.
        return na == 0 || std::memcmp(a, b, na * sizeof(T)) == 0;
    }

    /*!
     * @brief Container form of @ref bitsEqual: anything contiguous that
     *        `std::data` / `std::size` accept and that holds `float` or `double`.
     *
     * Takes a mutable field (`std::vector`, @ref InlineVector, `std::array`)
     * and a constant default (a temporary of the same type, a `std::span`,
     * a `std::initializer_list`) alike, and the two need not be the same
     * container type — only the same element type.
     */
    template <typename A, typename B>
    inline bool bitsEqual(const A &a, const B &b) noexcept
    {
        using T = std::remove_cv_t<std::remove_pointer_t<decltype(std::data(a))>>;
        static_assert(std::is_same_v<T, std::remove_cv_t<std::remove_pointer_t<decltype(std::data(b))>>>,
                      "bitsEqual compares two arrays of the same element type");
        return bitsEqual<T>(std::data(a), static_cast<std::size_t>(std::size(a)),
                            std::data(b), static_cast<std::size_t>(std::size(b)));
    }
}

/** @} */ // end of addtogroup

#endif // SOFAB_FLOATS_HPP
