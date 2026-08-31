/*!
 * @file sofab.hpp
 * @brief SofaBuffers C++ - Input and output stream decoder for Sofab messages.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SOFAB_HPP
#define SOFAB_HPP

/**
 * @defgroup cpp_api C++ API
 * @{
 */

/* includes *******************************************************************/
#include <array>
#include <concepts>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "sofab/istream.h"
#include "sofab/ostream.h"

/* feature-flag consistency with the C core ***********************************/
/*
 * The C core can be built with capabilities removed via its SOFAB_DISABLE_*
 * switches. Because this wrapper is header-only and calls the C API directly,
 * it must honour the same switches or it would emit calls to functions the core
 * no longer declares (an opaque "sofab_... was not declared" error). The
 * capabilities split into two groups:
 *
 *   Type-dispatch capabilities (FP64, INT64, ARRAY) are reachable only through
 *   the templated write()/read() via `if constexpr`. When one is disabled the
 *   matching branch turns into a clear static_assert, so code that never uses
 *   that type still compiles, while code that does gets a readable diagnostic.
 *
 *   Structural capabilities (FIXLEN, SEQUENCE, LAZY_SEQ) underpin concrete
 *   methods and the whole nested-message API (strings, blobs, floats,
 *   sequences, message objects) — i.e. most of the C++ surface. The wrapper
 *   cannot offer a coherent object API without them, so building the C++ layer
 *   with any of them disabled is rejected outright; use the C API directly for
 *   such configs. LAZY_SEQ is structural for the same reason: the wrapper's
 *   nested-message writes are defined in terms of the hold-back framing of
 *   MESSAGE_SPEC §2, and sequenceBeginLazy()/sequenceEndKeep() have no eager
 *   substitute that would produce the same bytes.
 */
#if defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
#  error "sofab C++ wrapper requires FIXLEN support (strings, blobs, floats). Do not define SOFAB_DISABLE_FIXLEN_SUPPORT when building the C++ API; use the C API directly for fixlen-less builds."
#endif
#if defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
#  error "sofab C++ wrapper requires SEQUENCE support (nested messages, variable-length array reads). Do not define SOFAB_DISABLE_SEQUENCE_SUPPORT when building the C++ API; use the C API directly for sequence-less builds."
#endif
#if defined(SOFAB_DISABLE_LAZY_SEQ_SUPPORT)
#  error "sofab C++ wrapper requires LAZY_SEQ support (an all-default nested message is omitted, not framed empty — MESSAGE_SPEC §2). Do not define SOFAB_DISABLE_LAZY_SEQ_SUPPORT when building the C++ API; it is a footprint switch for pure-C consumers, which encode through sofab_object_encode()."
#endif

/*! @brief 1 if the wrapper exposes 64-bit float (double) fields, else 0
 *  (mirrors @c SOFAB_DISABLE_FP64_SUPPORT in the C core). */
#if defined(SOFAB_DISABLE_FP64_SUPPORT)
#  define SOFAB_CPP_HAVE_FP64 0
#else
#  define SOFAB_CPP_HAVE_FP64 1
#endif
/*! @brief 1 if the wrapper exposes 64-bit integer fields, else 0
 *  (mirrors @c SOFAB_DISABLE_INT64_SUPPORT in the C core). */
#if defined(SOFAB_DISABLE_INT64_SUPPORT)
#  define SOFAB_CPP_HAVE_INT64 0
#else
#  define SOFAB_CPP_HAVE_INT64 1
#endif
/*! @brief 1 if the wrapper exposes array/span fields, else 0
 *  (mirrors @c SOFAB_DISABLE_ARRAY_SUPPORT in the C core). */
#if defined(SOFAB_DISABLE_ARRAY_SUPPORT)
#  define SOFAB_CPP_HAVE_ARRAY 0
#else
#  define SOFAB_CPP_HAVE_ARRAY 1
#endif

/* types **********************************************************************/
namespace sofab
{
    /*! @brief SofaBuffers C++ API version (mirrors @ref SOFAB_API_VERSION). */
    inline constexpr int API_VERSION = 1;

    /*! @brief Always-false trait used to trigger a dependent static_assert in a
     *  discarded @c if @c constexpr branch (so it only fires when instantiated). */
    template <typename>
    inline constexpr bool always_false_v = false;

    /*! @brief Result/error code returned by the stream APIs (wraps @ref sofab_ret_t). */
    enum class Error
    {
        // Non-error outcomes first — kept low-numbered and contiguous, ahead of
        // the error codes. Incomplete is a valid (partial) result, not a failure.
        None = SOFAB_RET_OK,                    //!< Success (a complete message boundary).
        Incomplete = SOFAB_RET_INCOMPLETE,      //!< Consumed bytes end inside a field or with an
                                                //!< open sequence: a valid but partial decode. NOT
                                                //!< an error — the caller owns end-of-input and may
                                                //!< feed more bytes. Distinct from @c None (complete)
                                                //!< and @c InvalidMessage (malformed).
        // Error codes follow.
        BufferFull = SOFAB_RET_E_BUFFER_FULL,   //!< Output buffer overflowed during encoding.
        InvalidArgument = SOFAB_RET_E_ARGUMENT, //!< A field id out of range, a scalar width that is
                                                //!< not 1/2/4/8 — or a **destination too short for
                                                //!< the value it was handed** (§6.6.3): the third of
                                                //!< the three ways a value can be refused, and the
                                                //!< one that is a mistake in the *call*. See
                                                //!< @ref IStreamImpl::readString.
        InvalidMessage = SOFAB_RET_E_INVALID_MSG, //!< Malformed message encountered while decoding.
        LimitExceeded = SOFAB_RET_E_LIMIT_EXCEEDED //!< A configured receiver limit (§6.2.1) was
                                                //!< exceeded on a schema-**unbounded** field. The
                                                //!< bytes are well-formed and decode under a looser
                                                //!< limit, so this is deliberately **not**
                                                //!< @c InvalidMessage: it means "raise my limit, or
                                                //!< the sender must send less", where
                                                //!< @c InvalidMessage means "these bytes are
                                                //!< broken". Terminal, like @c InvalidMessage.
                                                //!<
                                                //!< **This corelib never decides it.** §6.2.1 gives
                                                //!< the limits to generated code — "the visitor
                                                //!< decides. The codec never invents a limit of its
                                                //!< own and never clamps to one" — and leaves the
                                                //!< codec "the report and the category". This
                                                //!< enumerator is the category; the report is the
                                                //!< @c size / @c count the field callback already
                                                //!< carries. A handler that finds an announced
                                                //!< length past its configured cap calls
                                                //!< @ref IStreamImpl::exceedLimit and returns
                                                //!< without binding a destination.
    };



    /*! @brief Field identifier type (alias of @ref sofab_id_t). */
    using id = sofab_id_t;

    /*!
     * @brief Wire type of a decoded field (the 3-bit type tag of its header).
     *
     * Part of the public API: returned by @ref IStreamImpl::wire. The enumerator
     * values are the wire tags themselves, so this mirrors @ref sofab_type_t while
     * exposing the same @c sofab::Wire surface as the sibling @c corelib-cpp — a
     * generator emits one @ref IStreamImpl::wire guard shape for both.
     */
    enum class Wire : uint8_t
    {
        Unsigned      = SOFAB_TYPE_VARINT_UNSIGNED,      //!< Unsigned integer encoded as a varint.
        Signed        = SOFAB_TYPE_VARINT_SIGNED,        //!< Signed integer, zig-zag encoded as a varint.
        Fixlen        = SOFAB_TYPE_FIXLEN,               //!< Length-prefixed payload (float, string or blob).
        ArrayUnsigned = SOFAB_TYPE_VARINTARRAY_UNSIGNED, //!< Count-prefixed array of unsigned varints.
        ArraySigned   = SOFAB_TYPE_VARINTARRAY_SIGNED,   //!< Count-prefixed array of zig-zag varints.
        ArrayFixlen   = SOFAB_TYPE_FIXLENARRAY,          //!< Count-prefixed array of fixed-size elements.
        SequenceStart = SOFAB_TYPE_SEQUENCE_START,       //!< Opens a nested sub-message.
        SequenceEnd   = SOFAB_TYPE_SEQUENCE_END,         //!< Closes the most recently opened sub-message.
    };

    /*!
     * @brief Sub-type of a length-prefixed (@ref Wire::Fixlen) payload.
     *
     * Part of the public API: returned by @ref IStreamImpl::fixType. MESSAGE_SPEC
     * §7.3 bounds the decode-side type check at wire type *plus* this subtype,
     * since @c fp32 / @c fp64 / @c string / @c blob all share the
     * @ref Wire::Fixlen wire type. Mirrors @ref sofab_fixlentype_t and matches
     * @c corelib-cpp's @c sofab::Fix.
     */
    enum class Fix : uint8_t
    {
        Fp32   = SOFAB_FIXLENTYPE_FP32,   //!< 32-bit IEEE-754 float.
        Fp64   = SOFAB_FIXLENTYPE_FP64,   //!< 64-bit IEEE-754 double.
        String = SOFAB_FIXLENTYPE_STRING, //!< UTF-8 text.
        Blob   = SOFAB_FIXLENTYPE_BLOB,   //!< Opaque byte string.
    };

    /*******************/
    /*** FixedString ***/
    /*******************/

    /*!
     * @brief Fixed-capacity, heap-free string of up to @p N characters.
     *
     * A drop-in, embedded-friendly stand-in for @c std::string on both the encode
     * and decode paths. The characters live in an inline @c std::array, so an
     * instance allocates nothing, never throws (overflow clamps to @p N), and
     * compiles cleanly under @c -fno-exceptions / @c -fno-rtti. Because the buffer
     * never moves, an instance is a valid address-stable decode target for the
     * deferred @ref IStreamImpl decoder (bytes bound now, filled by a later
     * @ref IStreamImpl::feed pass).
     *
     * The storage is @c N+1 bytes: one extra slot always holds a trailing NUL so
     * @ref c_str and the @c std::string_view encode path remain valid even at full
     * length @p N. The buffer is zero-initialised, so the NUL is present from
     * construction and is re-placed by every length-changing operation.
     *
     * @par Generator integration contract
     * Generated code mirrors the @c std::string path exactly, so this type keeps a
     * stable surface for it:
     *   - decode emits @c s.set_len(_size); if (_size) is.read(s); — @ref set_len
     *     fixes the logical length (and terminating NUL) before the read binds the
     *     buffer, and @ref IStreamImpl::read fills @c data() over @c size() bytes;
     *   - encode emits @c os.write(id, s); — the implicit @ref operator std::string_view
     *     routes it through the existing string encode branch, byte-for-byte
     *     identical to the same-content @c std::string.
     *
     * @tparam N  Maximum number of characters (excluding the reserved NUL slot).
     */
    template <std::size_t N>
    class FixedString
    {
        std::array<char, N + 1> buf_{};     //!< Inline storage (+1 for the NUL).
        std::size_t len_ = 0;               //!< Current logical length (<= N).

    public:
        /*! @brief Character type (mirrors @c std::string). */
        using value_type = char;
        /*! @brief Size type (mirrors @c std::string). */
        using size_type = std::size_t;

        /*! @brief Construct an empty string. */
        FixedString() noexcept = default;

        /*!
         * @brief Construct from a NUL-terminated C string (truncated to @p N).
         * @param s  Source string, or @c nullptr for an empty string.
         */
        FixedString(const char *s) noexcept
        {
            assign(s ? std::string_view{s} : std::string_view{});
        }

        /*!
         * @brief Construct from a string view (truncated to @p N).
         * @param sv  Source characters (may contain embedded NULs).
         */
        FixedString(std::string_view sv) noexcept
        {
            assign(sv);
        }

        /*!
         * @brief Construct from a @c std::string (the easy on-ramp; truncated to @p N).
         * @param s  Source string.
         */
        FixedString(const std::string &s) noexcept
        {
            assign(std::string_view{s});
        }

        /*! @brief Assign from a NUL-terminated C string (truncated to @p N). */
        FixedString &operator=(const char *s) noexcept
        {
            assign(s ? std::string_view{s} : std::string_view{});
            return *this;
        }

        /*! @brief Assign from a string view (truncated to @p N). */
        FixedString &operator=(std::string_view sv) noexcept
        {
            assign(sv);
            return *this;
        }

        /*! @brief Assign from a @c std::string (truncated to @p N). */
        FixedString &operator=(const std::string &s) noexcept
        {
            assign(std::string_view{s});
            return *this;
        }

        /*!
         * @brief Replace the contents with @p sv, truncated to @p N characters.
         * @param sv  Source characters (may contain embedded NULs).
         * @return Reference to @c *this.
         */
        FixedString &assign(std::string_view sv) noexcept
        {
            len_ = sv.size() > N ? N : sv.size();
            for (std::size_t i = 0; i < len_; ++i)
            {
                buf_[i] = sv[i];
            }
            buf_[len_] = '\0';
            return *this;
        }

        /*!
         * @brief Decode hook: set the logical length and (re)place the trailing NUL.
         *
         * Called by generated decode before binding the buffer: it fixes
         * @c size() to the field length (clamped to @p N) and writes the NUL at
         * @c buf_[len_]. The subsequent @ref IStreamImpl::read binds exactly
         * @c size() bytes via @c sofab_istream_read_string_noterm, which fills
         * @c [0, size()) and never touches @c buf_[len_], so the NUL survives and
         * @ref c_str stays valid. Re-decoding a shorter value re-terminates here.
         *
         * @param n  Requested logical length (clamped to @p N).
         */
        void set_len(std::size_t n) noexcept
        {
            len_ = n > N ? N : n;
            buf_[len_] = '\0';
        }

        /*! @brief Mutable pointer to the character buffer (decode target). */
        char *data() noexcept { return buf_.data(); }
        /*! @brief Const pointer to the character buffer. */
        const char *data() const noexcept { return buf_.data(); }
        /*! @brief NUL-terminated view of the contents. */
        const char *c_str() const noexcept { return buf_.data(); }

        /*! @brief Number of characters currently stored. */
        std::size_t size() const noexcept { return len_; }
        /*! @brief Alias of @ref size. */
        std::size_t length() const noexcept { return len_; }
        /*! @brief True if the string is empty. */
        bool empty() const noexcept { return len_ == 0; }
        /*! @brief Maximum number of characters (the template parameter @p N). */
        static constexpr std::size_t capacity() noexcept { return N; }
        /*! @brief Alias of @ref capacity. */
        static constexpr std::size_t max_size() noexcept { return N; }

        /*! @brief Access the character at @p i (no bounds checking). */
        char &operator[](std::size_t i) noexcept { return buf_[i]; }
        /*! @brief Access the character at @p i (no bounds checking). */
        const char &operator[](std::size_t i) const noexcept { return buf_[i]; }

        /*! @brief Iterator to the first character. */
        char *begin() noexcept { return buf_.data(); }
        /*! @brief Iterator past the last character. */
        char *end() noexcept { return buf_.data() + len_; }
        /*! @brief Const iterator to the first character. */
        const char *begin() const noexcept { return buf_.data(); }
        /*! @brief Const iterator past the last character. */
        const char *end() const noexcept { return buf_.data() + len_; }

        /*! @brief Reset to an empty string. */
        void clear() noexcept
        {
            len_ = 0;
            buf_[0] = '\0';
        }

        /*! @brief Non-owning view over the current characters. */
        std::string_view view() const noexcept
        {
            return std::string_view{buf_.data(), len_};
        }

        /*!
         * @brief Implicit conversion to @c std::string_view.
         *
         * Gives a cheap non-owning view and makes the existing string encode
         * branch (@c OStreamImpl::write) match a @c FixedString automatically.
         */
        operator std::string_view() const noexcept
        {
            return view();
        }

        /*! @brief Copy the contents into an owning @c std::string (allocates). */
        std::string str() const
        {
            return std::string{buf_.data(), len_};
        }

        /*! @brief Equality against any string view-like operand. */
        bool operator==(std::string_view rhs) const noexcept
        {
            return view() == rhs;
        }

        /*! @brief Inequality against any string view-like operand. */
        bool operator!=(std::string_view rhs) const noexcept
        {
            return view() != rhs;
        }
    };

    /*! @brief Trait: true only for @ref FixedString specializations. */
    template <typename>
    struct is_fixed_string : std::false_type { };
    template <std::size_t N>
    struct is_fixed_string<FixedString<N>> : std::true_type { };
    /*! @brief Convenience value for @ref is_fixed_string. */
    template <typename T>
    inline constexpr bool is_fixed_string_v =
        is_fixed_string<std::remove_cv_t<T>>::value;

    /******************/
    /*** FixedBytes ***/
    /******************/

    /*!
     * @brief Fixed-capacity, heap-free byte blob of up to @p N bytes.
     *
     * The embedded-friendly counterpart of @c std::vector<std::uint8_t> for blob
     * fields, mirroring @ref FixedString for bytes. The payload lives in an inline
     * @c std::array, so an instance allocates nothing and never throws (overflow
     * clamps to @p N). Because the buffer never moves, an instance is a valid
     * address-stable decode target for the deferred @ref IStreamImpl decoder.
     *
     * A @b logical @b length (@ref size, @c <= @p N) is tracked separately from the
     * capacity @p N: a blob shorter than its schema @c maxlen occupies only
     * @ref size bytes on the wire. This is exactly why the type cannot be a plain
     * @c std::array<std::uint8_t,N> (always length @p N) and must not reintroduce
     * the heap of @c std::vector.
     *
     * @par Generator integration contract
     * Generated code mirrors the @c std::vector<std::uint8_t> path:
     *   - encode passes @ref data / @ref size to the blob write, byte-for-byte
     *     identical to the same-content vector;
     *   - decode emits @c b.set_len(_size); if (_size) is.read(b); — @ref set_len
     *     fixes the logical length (clamped to @p N) before the read binds the
     *     inline buffer over @ref size bytes. Binding the container this way (not a
     *     bare @c is.read(b.data(), _size)) lets the decoder reject an over-@p N
     *     wire length as INVALID per MESSAGE_SPEC §7.1 instead of truncating it.
     *
     * @tparam N  Maximum number of bytes.
     */
    template <std::size_t N>
    class FixedBytes
    {
        std::array<std::uint8_t, N> buf_{};     //!< Inline storage.
        std::size_t len_ = 0;                   //!< Current logical length (<= N).

    public:
        /*! @brief Element type (mirrors @c std::vector). */
        using value_type = std::uint8_t;
        /*! @brief Size type (mirrors @c std::vector). */
        using size_type = std::size_t;

        /*! @brief Construct an empty blob. */
        FixedBytes() noexcept = default;

        /*!
         * @brief Construct from a brace-enclosed list of bytes (truncated to @p N).
         *
         * Providing this constructor makes @c FixedBytes a non-aggregate, so a
         * brace-init such as @c b = {1, 2, 3} routes through here and sets
         * @ref size — it cannot silently fill the buffer while leaving the logical
         * length at zero.
         *
         * @param init  Source bytes (excess beyond @p N is dropped).
         */
        FixedBytes(std::initializer_list<std::uint8_t> init) noexcept
        {
            assign(init);
        }

        /*! @brief Replace the contents from a brace-enclosed list (truncated to @p N). */
        FixedBytes &operator=(std::initializer_list<std::uint8_t> init) noexcept
        {
            return assign(init);
        }

        /*! @brief Replace the contents from a brace-enclosed list (truncated to @p N). */
        FixedBytes &assign(std::initializer_list<std::uint8_t> init) noexcept
        {
            len_ = 0;
            for (std::uint8_t b : init)
            {
                if (len_ >= N)
                {
                    break;
                }
                buf_[len_++] = b;
            }
            return *this;
        }

        /*! @brief Mutable pointer to the byte buffer (decode target). */
        std::uint8_t *data() noexcept { return buf_.data(); }
        /*! @brief Const pointer to the byte buffer. */
        const std::uint8_t *data() const noexcept { return buf_.data(); }

        /*! @brief Number of bytes currently stored. */
        std::size_t size() const noexcept { return len_; }
        /*! @brief True if the blob is empty. */
        bool empty() const noexcept { return len_ == 0; }
        /*! @brief Maximum number of bytes (the template parameter @p N). */
        static constexpr std::size_t capacity() noexcept { return N; }
        /*! @brief Alias of @ref capacity. */
        static constexpr std::size_t max_size() noexcept { return N; }

        /*!
         * @brief Decode hook: set the logical length (clamped to @p N).
         *
         * Called by generated decode before binding the buffer, so @ref size
         * reports the field length and @ref data over that many bytes is the
         * fill target.
         *
         * @param n  Requested logical length (clamped to @p N).
         */
        void set_len(std::size_t n) noexcept { len_ = n < N ? n : N; }

        /*! @brief Reset to an empty blob. */
        void clear() noexcept { len_ = 0; }

        /*! @brief Append one byte (no-op once at capacity @p N). */
        void push_back(std::uint8_t b) noexcept
        {
            if (len_ < N)
            {
                buf_[len_++] = b;
            }
        }

        /*! @brief Access the byte at @p i (no bounds checking). */
        std::uint8_t &operator[](std::size_t i) noexcept { return buf_[i]; }
        /*! @brief Access the byte at @p i (no bounds checking). */
        const std::uint8_t &operator[](std::size_t i) const noexcept { return buf_[i]; }

        /*! @brief Iterator to the first byte. */
        std::uint8_t *begin() noexcept { return buf_.data(); }
        /*! @brief Iterator past the last byte. */
        std::uint8_t *end() noexcept { return buf_.data() + len_; }
        /*! @brief Const iterator to the first byte. */
        const std::uint8_t *begin() const noexcept { return buf_.data(); }
        /*! @brief Const iterator past the last byte. */
        const std::uint8_t *end() const noexcept { return buf_.data() + len_; }

        /*! @brief Content equality (same logical length and bytes). */
        bool operator==(const FixedBytes &o) const noexcept
        {
            if (len_ != o.len_)
            {
                return false;
            }
            for (std::size_t i = 0; i < len_; ++i)
            {
                if (buf_[i] != o.buf_[i])
                {
                    return false;
                }
            }
            return true;
        }

        /*! @brief Negated @ref operator==. */
        bool operator!=(const FixedBytes &o) const noexcept { return !(*this == o); }
    };

    /*! @brief Trait: true only for @ref FixedBytes specializations. */
    template <typename>
    struct is_fixed_bytes : std::false_type { };
    template <std::size_t N>
    struct is_fixed_bytes<FixedBytes<N>> : std::true_type { };
    /*! @brief Convenience value for @ref is_fixed_bytes. */
    template <typename T>
    inline constexpr bool is_fixed_bytes_v =
        is_fixed_bytes<std::remove_cv_t<T>>::value;

    /********************/
    /*** InlineVector ***/
    /********************/

    /*!
     * @brief Fixed-capacity, heap-free sequence of up to @p N elements of type @p T.
     *
     * The embedded-friendly counterpart of @c std::vector<T> for the sequence
     * fields the fixed-capacity profile lowers without a heap: arrays of strings,
     * blobs, structs/unions or nested arrays. Elements live in an inline
     * @c std::array, so the storage never reallocates and a bound-then-filled
     * element (the deferred @ref IStreamImpl decoder) stays address-stable —
     * strictly safer than a @c std::vector + @c reserve.
     *
     * A @b logical @b length (@ref size, @c <= @p N) is tracked separately from the
     * capacity @p N: an array shorter than its schema @c count holds only
     * @ref size elements. This is why the type is neither a plain
     * @c std::array<T,N> (always length @p N) nor a heap-backed @c std::vector.
     *
     * @warning Historically this was an aggregate with a public, default-zero
     * length, so a natural brace-init such as @c v = {a, b, c} silently filled the
     * storage while leaving the logical length at 0 — the field then encoded as
     * empty. The @c initializer_list constructor/assignment below make the type a
     * non-aggregate, so that brace-init now sets @ref size correctly instead of
     * corrupting the wire.
     *
     * @tparam T  Element type.
     * @tparam N  Maximum number of elements.
     */
    template <typename T, std::size_t N>
    class InlineVector
    {
        std::array<T, N> buf_{};    //!< Inline storage.
        std::size_t len_ = 0;       //!< Current logical length (<= N).

    public:
        /*! @brief Element type (mirrors @c std::vector). */
        using value_type = T;
        /*! @brief Size type (mirrors @c std::vector). */
        using size_type = std::size_t;

        /*! @brief Construct an empty sequence. */
        InlineVector() noexcept = default;

        /*!
         * @brief Construct from a brace-enclosed list of elements (truncated to @p N).
         *
         * The presence of this constructor makes @c InlineVector a non-aggregate:
         * @c v = {a, b, c} routes here and sets @ref size, instead of aggregate
         * brace-init filling the storage while leaving the length at 0.
         *
         * @param init  Source elements (excess beyond @p N is dropped).
         */
        InlineVector(std::initializer_list<T> init) noexcept
        {
            assign(init);
        }

        /*! @brief Replace the contents from a brace-enclosed list (truncated to @p N). */
        InlineVector &operator=(std::initializer_list<T> init) noexcept
        {
            return assign(init);
        }

        /*! @brief Replace the contents from a brace-enclosed list (truncated to @p N). */
        InlineVector &assign(std::initializer_list<T> init) noexcept
        {
            len_ = 0;
            for (const T &v : init)
            {
                if (len_ >= N)
                {
                    break;
                }
                buf_[len_++] = v;
            }
            return *this;
        }

        /*! @brief Number of elements currently stored. */
        std::size_t size() const noexcept { return len_; }
        /*! @brief True if the sequence is empty. */
        bool empty() const noexcept { return len_ == 0; }
        /*! @brief Maximum number of elements (the template parameter @p N). */
        static constexpr std::size_t capacity() noexcept { return N; }
        /*! @brief Alias of @ref capacity. */
        static constexpr std::size_t max_size() noexcept { return N; }

        /*! @brief No-op (inline storage never reallocates); present for API parity. */
        void reserve(std::size_t) noexcept {}
        /*! @brief Reset to an empty sequence (logical length only). */
        void clear() noexcept { len_ = 0; }

        /*!
         * @brief Set the logical length to @p n, value-initializing what changes.
         *
         * The @c std::vector member of the container API this type mirrors, and the
         * one @ref IStreamImpl::readArray and the wrapper-array collectors probe
         * for: readArray *resizes* a resizable destination and *value-initializes*
         * a fixed-extent one, and without this method an @c InlineVector matched
         * neither — it fell to the fixed-extent branch, which assigned a
         * default-constructed container and so set the logical length to 0. The
         * decode then bound an empty span and dropped the array silently. With
         * @ref resize present, readArray keeps ownership of the tag / bound /
         * reset / bind order it documents, for inline storage too.
         *
         * Slots that enter or leave the logical range are set to @c T{}, so the
         * elements a shorter value no longer covers cannot be observed through a
         * later grow. @p n above the capacity @p N is clamped to @p N — the callers
         * that can reject an over-capacity count do so before resizing (readArray
         * checks the schema `count` first), and a heap-free container has nowhere
         * to put the excess.
         *
         * @param n  New logical length.
         */
        void resize(std::size_t n) noexcept
        {
            if (n > N)
            {
                n = N;
            }
            for (std::size_t i = n; i < len_; ++i)
            {
                buf_[i] = T{};
            }
            for (std::size_t i = len_; i < n; ++i)
            {
                buf_[i] = T{};
            }
            len_ = n;
        }

        /*!
         * @brief Append a default-constructed element and return a reference to it.
         *
         * The next inline slot is (re)set to @c T{} and bound; once at capacity
         * @p N the last slot is reused so a decode never writes out of bounds.
         * @return Reference to the newly active element.
         */
        T &emplace_back() noexcept
        {
            std::size_t i = len_ < N ? len_++ : N - 1;
            buf_[i] = T{};
            return buf_[i];
        }

        /*! @brief Append a copy of @p v (no-op growth once at capacity @p N). */
        void push_back(const T &v) noexcept { emplace_back() = v; }
        /*! @brief Append @p v by move (no-op growth once at capacity @p N). */
        void push_back(T &&v) noexcept { emplace_back() = static_cast<T &&>(v); }

        /*! @brief Reference to the last element. */
        T &back() noexcept { return buf_[len_ - 1]; }
        /*! @brief Const reference to the last element. */
        const T &back() const noexcept { return buf_[len_ - 1]; }

        /*! @brief Access the element at @p i (no bounds checking). */
        T &operator[](std::size_t i) noexcept { return buf_[i]; }
        /*! @brief Access the element at @p i (no bounds checking). */
        const T &operator[](std::size_t i) const noexcept { return buf_[i]; }

        /*! @brief Mutable pointer to the underlying storage. */
        T *data() noexcept { return buf_.data(); }
        /*! @brief Const pointer to the underlying storage. */
        const T *data() const noexcept { return buf_.data(); }

        /*! @brief Iterator to the first element. */
        T *begin() noexcept { return buf_.data(); }
        /*! @brief Iterator past the last element. */
        T *end() noexcept { return buf_.data() + len_; }
        /*! @brief Const iterator to the first element. */
        const T *begin() const noexcept { return buf_.data(); }
        /*! @brief Const iterator past the last element. */
        const T *end() const noexcept { return buf_.data() + len_; }

        /*! @brief Content equality (same logical length and elements). */
        bool operator==(const InlineVector &o) const noexcept
        {
            if (len_ != o.len_)
            {
                return false;
            }
            for (std::size_t i = 0; i < len_; ++i)
            {
                if (!(buf_[i] == o.buf_[i]))
                {
                    return false;
                }
            }
            return true;
        }

        /*! @brief Negated @ref operator==. */
        bool operator!=(const InlineVector &o) const noexcept { return !(*this == o); }
    };

    /***************/
    /*** OStream ***/
    /***************/

    class OStreamMessage;

    /*!
     * @brief Base output stream: encodes fields into a caller-provided buffer.
     *
     * Thin C++ facade over @ref sofab_ostream_t. The @c write() overloads deduce
     * the wire encoding from the C++ type, and each returns a @ref Result that
     * can be chained fluently. Concrete buffer ownership is added by the derived
     * @ref OStream / @ref OStreamInline classes; this base is not constructed
     * directly.
     */
    class OStreamImpl
    {
    public:
        /*! @brief Callback invoked with the bytes to flush (buffer full or on flush()). */
        using flushCallback = std::function<void(std::span<const uint8_t>)>;

    protected:
        sofab_ostream_t ctx_;           //!< Underlying C output stream context.
        uint8_t *buffer_;               //!< Pointer to the active encode buffer.
        flushCallback flushCallback_;   //!< Optional user flush callback.
        uint8_t failed_ = 0;            //!< Sticky: first failing write's sofab_ret_t
                                        //!< (0 = none). See @ref ok.

        /*! @brief Invoke the user flush callback (if any) with @p len buffered bytes. */
        void onFlushCallback(size_t len) noexcept
        {
            if (flushCallback_)
            {
                flushCallback_(std::span<const uint8_t>(buffer_, len));
            }
        }

        /*! @brief C-ABI flush trampoline: forwards to onFlushCallback() via @p usrptr. */
        static void static_flush_callback(
            sofab_ostream_t *ctx,
            const uint8_t *data,
            size_t len,
            void *usrptr) noexcept
        {
            (void)ctx;
            (void)data;

            OStreamImpl *self = static_cast<OStreamImpl*>(usrptr);
            self->onFlushCallback(len);
        }

        OStreamImpl() noexcept = default;

    public:
        /*!
         * @brief Outcome of an encode operation, supporting fluent chaining.
         *
         * Returned by every @ref OStreamImpl write/sequence call. The first
         * error is sticky: once set, subsequent chained calls become no-ops and
         * the original error is preserved, so a chain can be checked once at the
         * end. Convertible to @c bool (true on success).
         */
        class Result
        {
        private:
            OStreamImpl &ostream_;
            Error error_ = Error::None;

            friend class OStreamImpl;
            Result(OStreamImpl &ostream, sofab_ret_t retval)
                : ostream_(ostream)
                , error_(static_cast<Error>(retval))
            { }

            sofab_ret_t rawCode() const noexcept
            {
                return static_cast<sofab_ret_t>(error_);
            }

        public:
            /*!
             * @brief Chained write of a field (no-op if a prior call failed).
             * @param id     Field identifier.
             * @param value  Value to encode; the wire type is deduced from @p T.
             * @return This Result, carrying the first error encountered (if any).
             */
            template <typename T>
            Result write(sofab_id_t id, const T &value) noexcept
            {
                if (error_ != Error::None)
                {
                    return *this;
                }

                auto res = ostream_.write(id, value);
                if (!res.ok())
                {
                    error_ = res.code();
                }

                return *this;
            }

            /*!
             * @brief Chained conditional write (no-op if a prior call failed).
             * @param id         Field identifier.
             * @param value      Value to encode; the wire type is deduced from @p T.
             * @param condition  Write the field only when true.
             * @return This Result, carrying the first error encountered (if any).
             */
            template <typename T>
            Result writeIf(sofab_id_t id, const T &value, bool condition) noexcept
            {
                if (error_ != Error::None)
                {
                    return *this;
                }

                auto res = ostream_.writeIf(id, value, condition);
                if (!res.ok())
                {
                    error_ = res.code();
                }

                return *this;
            }

            /*!
             * @brief Chained nested-message field write that is omitted when the
             *        message is all-default (no-op if a prior call failed).
             * @param id     Field identifier.
             * @param value  Nested message to encode.
             * @return This Result, carrying the first error encountered (if any).
             * @see OStreamImpl::writeLazy
             */
            template <typename T>
            Result writeLazy(sofab_id_t id, const T &value) noexcept
            {
                if (error_ != Error::None)
                {
                    return *this;
                }

                auto res = ostream_.writeLazy(id, value);
                if (!res.ok())
                {
                    error_ = res.code();
                }

                return *this;
            }

            /*!
             * @brief Chained frame-keeping sequence-end (no-op if a prior call failed).
             * @return This Result, carrying the first error encountered (if any).
             * @see OStreamImpl::sequenceEndKeep
             */
            Result sequenceEndKeep() noexcept
            {
                if (error_ != Error::None)
                {
                    return *this;
                }

                auto res = ostream_.sequenceEndKeep();
                if (!res.ok())
                {
                    error_ = res.code();
                }

                return *this;
            }

            /*!
             * @brief Chained lazy sequence-begin (no-op if a prior call failed).
             * @param id  Field identifier of the nested sequence.
             * @return This Result, carrying the first error encountered (if any).
             * @see OStreamImpl::sequenceBeginLazy
             */
            Result sequenceBeginLazy(sofab_id_t id) noexcept
            {
                if (error_ != Error::None)
                {
                    return *this;
                }

                auto res = ostream_.sequenceBeginLazy(id);
                if (!res.ok())
                {
                    error_ = res.code();
                }

                return *this;
            }

            /*!
             * @brief Chained sequence-end marker (no-op if a prior call failed).
             * @return This Result, carrying the first error encountered (if any).
             */
            Result sequenceEnd() noexcept
            {
                if (error_ != Error::None)
                {
                    return *this;
                }

                auto res = ostream_.sequenceEnd();
                if (!res.ok())
                {
                    error_ = res.code();
                }

                return *this;
            }

            /*! @brief True if no error occurred (same as ok()). */
            explicit operator bool() const noexcept
            {
                return ok();
            }

            /*! @brief Compare the held error code to @p e. */
            bool operator==(Error e) const noexcept
            {
                return error_ == e;
            }

            /*! @brief Negated operator==(). */
            bool operator!=(Error e) const noexcept
            {
                return !(*this == e);
            }

            /*! @brief True if the operation (and the chain so far) succeeded. */
            bool ok() const noexcept
            {
                return error_ == Error::None;
            }

            /*! @brief The first @ref Error encountered in the chain. */
            Error code() const noexcept
            {
                return error_;
            }
        };

        /*! @brief Copy construction is deleted (the context owns raw buffer pointers). */
        OStreamImpl(const OStreamImpl&) = delete;
        /*! @brief Copy assignment is deleted (the context owns raw buffer pointers). */
        OStreamImpl& operator=(const OStreamImpl&) = delete;

        /*! @brief Move construction (transfers the underlying context). */
        OStreamImpl(OStreamImpl&&) noexcept = default;
        /*! @brief Move assignment (transfers the underlying context). */
        OStreamImpl& operator=(OStreamImpl&&) noexcept = default;

        /*! @brief Destructor; flushes any buffered bytes. */
        virtual ~OStreamImpl() noexcept
        {
            flush();
        }

        /*!
         * @brief Flush buffered bytes through the flush callback (if any).
         * @return Number of bytes flushed.
         */
        size_t flush() noexcept
        {
            return sofab_ostream_flush(&ctx_);
        }

        /*!
         * @brief Number of bytes written to the active buffer since the last flush.
         * @return Bytes currently used in the buffer.
         */
        size_t bytesUsed() noexcept
        {
            return sofab_ostream_bytes_used(&ctx_);
        }

        /*!
         * @brief Pointer to the start of the active encode buffer.
         * @return Read-only pointer to the buffer (valid for @ref bytesUsed bytes).
         */
        const uint8_t* data() const noexcept
        {
            return buffer_;
        }

        /*!
         * @brief Encode a single field, deducing the wire type from @p T.
         *
         * Supports integers, @c bool, @c float, @c double, string-like types
         * (@c std::string / @c std::string_view / C strings), contiguous ranges
         * (arrays/vectors/spans), and nested @ref OStreamMessage objects. Using
         * a type whose capability was compiled out of the C core is a
         * compile-time error (see the @c SOFAB_DISABLE_* notes at the top).
         *
         * @param id     Field identifier.
         * @param value  Value to encode.
         * @return @ref Result for fluent chaining and error inspection.
         */
        template <typename T>
        Result write(sofab_id_t id, const T &value) noexcept
        {
            sofab_ret_t ret;

            if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
            {
#if !SOFAB_CPP_HAVE_INT64
                static_assert(sizeof(T) <= 4,
                    "64-bit integer fields require INT64 support, disabled via "
                    "SOFAB_DISABLE_INT64_SUPPORT");
#endif
                if constexpr (std::is_unsigned_v<T>)
                {
                    ret = sofab_ostream_write_unsigned(
                        &ctx_, id, static_cast<sofab_unsigned_t>(value));
                }
                else
                {
                    ret = sofab_ostream_write_signed(
                        &ctx_, id, static_cast<sofab_signed_t>(value));
                }
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                ret = sofab_ostream_write_boolean(&ctx_, id, value);
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                ret = sofab_ostream_write_fp32(&ctx_, id, value);
            }
            else if constexpr (std::is_same_v<T, double>)
            {
#if SOFAB_CPP_HAVE_FP64
                ret = sofab_ostream_write_fp64(&ctx_, id, value);
#else
                static_assert(always_false_v<T>,
                    "double (FP64) fields require FP64 support, disabled via "
                    "SOFAB_DISABLE_FP64_SUPPORT");
#endif
            }
            else if constexpr (std::is_convertible_v<T, std::string_view>)
            {
                /* The view carries its own length, so use it. Routing this
                 * through sofab_ostream_write_string() would derive the length
                 * with strlen() instead, which is wrong in both directions: a
                 * std::string_view need not be NUL-terminated, so strlen()
                 * reads past the caller's buffer; and a value holding an
                 * embedded U+0000 would be truncated there, though §4.6 frames
                 * a string by length with no terminator and §6.4.3 makes
                 * embedded U+0000 valid. The C convenience wrapper stays what
                 * it is — the entry point for a genuinely NUL-terminated
                 * const char*, which reaches this branch already converted. */
                std::string_view sv{value};
                if (sv.size() > static_cast<size_t>(INT32_MAX))
                {
                    /* Longer than the int32_t the C entry point takes, so the
                     * cast below would wrap and hand it a negative length. A
                     * caller mistake, hence §6.3's InvalidArgument.
                     *
                     * Only this bound is checked here. SOFAB_FIXLEN_MAX is the
                     * C core's and it enforces it (ostream.c), on exactly the
                     * profile where it can bind -- one lowered per §6.2. Where
                     * it is not lowered it equals INT32_MAX and this IS that
                     * comparison; repeating it would be a second implementation
                     * of one bound. */
                    ret = SOFAB_RET_E_ARGUMENT;
                }
                else
                {
                    ret = sofab_ostream_write_fixlen(
                        &ctx_, id, sv.data(),
                        static_cast<int32_t>(sv.size()),
                        SOFAB_FIXLENTYPE_STRING);
                }
            }
            else if constexpr (std::is_base_of_v<OStreamMessage, T>)
            {
                /* The frame-KEEPING form: it survives even when the nested
                 * message writes nothing. That is what a wrapper array's LAST
                 * element needs -- the array carries no length, so *highest present
                 * id + 1* is what recovers it and the final element may never be
                 * elided (MESSAGE_SPEC §2/§5.1). A nested message that MAY vanish
                 * when all-default -- a struct/union FIELD, and equally an INTERIOR
                 * array element, which since §3 made `count` a capacity is omitted
                 * exactly like a default leaf element -- is @ref writeLazy.
                 *
                 * The closer runs even when the nested serialize() fails, so the
                 * sequence this call opened is always the sequence this call
                 * closes. A failing serialize() is reachable on a perfectly
                 * healthy stream -- write_fixlen rejects invalid UTF-8 with
                 * E_ARGUMENT before emitting a byte under SOFAB_ENABLE_STRICT_UTF8
                 * -- and leaving the frame open would put an unterminated sequence
                 * on the wire and strand a slot of the bounded hold-back window.
                 * The first failure is what the caller sees. */
                ret = sequenceBeginLazy(id).rawCode();
                if (ret == SOFAB_RET_OK)
                {
                    ret = value.serialize(static_cast<OStreamImpl&>(*this)).rawCode();
                    sofab_ret_t cret = sequenceEndKeep().rawCode();
                    if (ret == SOFAB_RET_OK)
                    {
                        ret = cret;
                    }
                }
            }
            else if constexpr (
                requires {
                    typename T::value_type;
                    std::span{ std::declval<const T&>() };
                })
            {
#if SOFAB_CPP_HAVE_ARRAY
                using Elem = typename T::value_type;
                std::span<const Elem> span{value};

                if constexpr (std::is_integral_v<Elem> && !std::is_same_v<Elem, bool>)
                {
#if !SOFAB_CPP_HAVE_INT64
                    static_assert(sizeof(Elem) <= 4,
                        "64-bit integer arrays require INT64 support, disabled "
                        "via SOFAB_DISABLE_INT64_SUPPORT");
#endif
                    if constexpr (std::is_unsigned_v<Elem>)
                    {
                        ret = sofab_ostream_write_array_of_unsigned(
                            &ctx_, id,
                            span.data(),
                            static_cast<int32_t>(span.size()),
                            sizeof(Elem));
                    }
                    else
                    {
                        ret = sofab_ostream_write_array_of_signed(
                            &ctx_, id,
                            span.data(),
                            static_cast<int32_t>(span.size()),
                            sizeof(Elem));
                    }
                }
                else if constexpr (std::is_same_v<Elem, float>)
                {
                    ret = sofab_ostream_write_array_of_fp32(
                        &ctx_, id,
                        span.data(),
                        static_cast<int32_t>(span.size()));
                }
                else if constexpr (std::is_same_v<Elem, double>)
                {
#if SOFAB_CPP_HAVE_FP64
                    ret = sofab_ostream_write_array_of_fp64(
                        &ctx_, id,
                        span.data(),
                        static_cast<int32_t>(span.size()));
#else
                    static_assert(always_false_v<T>,
                        "double (FP64) arrays require FP64 support, disabled "
                        "via SOFAB_DISABLE_FP64_SUPPORT");
#endif
                }
                else
                {
                    static_assert(always_false_v<T>,
                        "Unsupported span element type in OStream::write()");
                }
#else
                static_assert(always_false_v<T>,
                    "array/span fields require ARRAY support, disabled via "
                    "SOFAB_DISABLE_ARRAY_SUPPORT");
#endif
            }
            else
            {
                static_assert(always_false_v<T>,
                    "Unsupported type passed to OStream::write()");
            }

            return result(ret);
        }

        /*!
         * @brief Encode a nested message **field**, omitting it when all-default.
         *
         * Same as @ref write for an @ref OStreamMessage, except it closes with
         * @ref sequenceEnd rather than @ref sequenceEndKeep: the nested @c serialize
         * omits every child
         * that equals its default, so "not one child was written" is exactly "the
         * object equals its declared default" -- evaluated per child field,
         * recursively -- and the field is then dropped instead of emitted as an
         * empty frame (MESSAGE_SPEC §2).
         *
         * Use it for a @c struct / @c union **field**. Keep plain @ref write for an
         * array **element**: element presence carries a dynamic array's length
         * (§5.1), so an all-default element stays framed.
         *
         * As in @ref write, the closer runs even when the nested @c serialize
         * fails (a strict-UTF-8 rejection does that on an otherwise healthy
         * stream), so the sequence never stays open and the hold-back window never
         * strands a slot. The first failure is the one returned.
         *
         * @param id     Field identifier.
         * @param value  Nested message to encode.
         * @return @ref Result for fluent chaining and error inspection.
         */
        template <typename T>
        Result writeLazy(sofab_id_t id, const T &value) noexcept
        {
            static_assert(std::is_base_of_v<OStreamMessage, T>,
                "writeLazy() takes a nested message; plain write() covers every other type");
            sofab_ret_t ret = sequenceBeginLazy(id).rawCode();
            if (ret == SOFAB_RET_OK)
            {
                ret = value.serialize(static_cast<OStreamImpl&>(*this)).rawCode();
                sofab_ret_t cret = sequenceEnd().rawCode();
                if (ret == SOFAB_RET_OK)
                {
                    ret = cret;
                }
            }

            return result(ret);
        }

        /*!
         * @brief Encode a raw binary blob field.
         * @param id     Field identifier.
         * @param value  Pointer to the bytes to write.
         * @param size   Number of bytes at @p value.
         * @return @ref Result for fluent chaining and error inspection.
         */
        Result write(sofab_id_t id, const void *value, int32_t size) noexcept
        {
            return result(sofab_ostream_write_blob(&ctx_, id, value, size));
        }

        /*!
         * @brief Encode a field only when @p condition is true.
         * @param id         Field identifier.
         * @param value      Value to encode; the wire type is deduced from @p T.
         * @param condition  Write the field only when true; otherwise a success no-op.
         * @return @ref Result for fluent chaining and error inspection.
         */
        template <typename T>
        Result writeIf(sofab_id_t id, const T &value, bool condition) noexcept
        {
            if (condition)
            {
                return write(id, value);
            }

            return result(SOFAB_RET_OK);
        }


        /*!
         * @brief Open a nested sequence whose header is held back until it turns
         *        out to have content.
         *
         * MESSAGE_SPEC §2 omits a sequence-typed field whose value equals its
         * declared default, and "not one child was written" is exactly that
         * condition -- evaluated per child field, recursively, for free. Closing
         * such a sequence with nothing in it emits **nothing** instead of a
         * two-byte empty frame, so an all-default message becomes the empty byte
         * string. The predicate never touches a byte image, so struct padding
         * cannot influence it.
         *
         * Where an empty frame carries meaning, close with @ref sequenceEndKeep
         * instead: the **last element** of a wrapper array (§5.1 — the interior is
         * sparse and drops like any default field), the §4.9 empty-sequence
         * primitive itself, and the explicitly empty array of a field that declares
         * a non-empty default (§2, §3). The wrapper deliberately exposes no eager
         * opener — the frame-or-not decision belongs to the closer, which is the
         * one call site that knows the position in the schema. (The C core keeps
         * @c sofab_ostream_write_sequence_begin() for @c sofab_object_encode().)
         *
         * **Bounded**: at most @c SOFAB_LAZY_SEQ_DEPTH (default 8) headers are
         * held back at once; a sequence opened deeper than that is framed eagerly
         * and keeps an empty frame it could have dropped — well-formed and
         * value-identical, but not canonical. See @c SOFAB_LAZY_SEQ_DEPTH in
         * @c sofab/ostream.h.
         *
         * @param id  Field identifier of the sequence.
         * @return @ref Result for fluent chaining and error inspection.
         */
        Result sequenceBeginLazy(sofab_id_t id) noexcept
        {
            return result(sofab_ostream_write_sequence_begin_lazy(&ctx_, id));
        }

        /*!
         * @brief Close the current nested sequence, letting it vanish if it got no
         *        content (MESSAGE_SPEC §2).
         * @return @ref Result for fluent chaining and error inspection.
         */
        Result sequenceEnd() noexcept
        {
            return result(sofab_ostream_write_sequence_end(&ctx_));
        }

        /*!
         * @brief Close the current nested sequence, keeping its frame even without
         *        content.
         *
         * Emits the held-back headers -- this frame's and every enclosing one's --
         * and then the end marker, so an empty sequence reaches the wire as
         * begin + end. Required wherever the frame carries information beyond its
         * contents: a wrapper-array ELEMENT (presence carries a dynamic array's
         * length, MESSAGE_SPEC §5.1), and an array field already known to differ
         * from a non-empty declared default (§2, §3).
         *
         * @return @ref Result for fluent chaining and error inspection.
         */
        Result sequenceEndKeep() noexcept
        {
            return result(sofab_ostream_write_sequence_end_keep(&ctx_));
        }

    private:
        /*! @brief Wrap a C return code in a @ref Result bound to this stream. */
        inline Result result(sofab_ret_t ret) noexcept
        {
            // Remember the first failure. A caller may chain writes and read the
            // verdict off the returned Result, but generated serialize() bodies
            // issue each write on its own and discard the result, so without this
            // a full buffer would leave no trace anywhere.
            if (failed_ == 0 && ret != SOFAB_RET_OK)
            {
                failed_ = (uint8_t)ret;
            }

            return Result{*this, ret};
        }

    public:
        /*!
         * @brief Whether every write on this stream has succeeded.
         *
         * Sticky and independent of how the writes were issued — chained or one
         * at a time with the result discarded. The usual reason it turns false is
         * @ref Error::BufferFull on a stream over caller storage
         * (@ref OStreamView), where the destination can be smaller than the
         * message.
         *
         * @return true while no write has failed.
         */
        [[nodiscard]] bool ok() const noexcept { return failed_ == 0; }

        /*!
         * @brief The first failure this stream saw, or @ref Error::None.
         * @return The error code of the first failing write.
         */
        [[nodiscard]] Error error() const noexcept
        {
            return static_cast<Error>(failed_);
        }

    protected:
    };

    /*!
     * @brief Output stream backed by a heap buffer (@c std::shared_ptr).
     *
     * Owns (or shares) a dynamically allocated buffer. With a flush callback the
     * buffer can be swapped mid-encoding via @ref setBuffer to stream in chunks.
     */
    class OStream : public OStreamImpl
    {
    protected:
        std::shared_ptr<uint8_t[]> bufferOwner_;    //!< Shared owner of the encode buffer.

        OStream() noexcept = default;

    public:
        /*!
         * @brief Construct with a freshly allocated buffer.
         * @param buflen  Buffer size in bytes.
         * @param offset  Initial write offset within the buffer (default 0).
         */
        OStream(size_t buflen, size_t offset = 0) noexcept
        {
            bufferOwner_ = std::make_shared<uint8_t[]>(buflen);
            buffer_ = bufferOwner_.get();
            sofab_ostream_init(&ctx_, buffer_, buflen, offset, nullptr, nullptr);
        }

        /*!
         * @brief Construct over a caller-provided shared buffer.
         * @param buffer  Shared buffer to encode into.
         * @param buflen  Usable size of @p buffer in bytes.
         * @param offset  Initial write offset within the buffer (default 0).
         */
        OStream(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
            : bufferOwner_{buffer}
        {
            buffer_ = bufferOwner_.get();
            sofab_ostream_init(&ctx_, buffer_, buflen, offset, nullptr, nullptr);
        }

        /*!
         * @brief Construct with a flush callback for chunked streaming.
         * @param callback  Invoked with buffered bytes when the buffer fills or on flush().
         * @param buffer    Shared buffer to encode into.
         * @param buflen    Usable size of @p buffer in bytes.
         * @param offset    Initial write offset within the buffer (default 0).
         */
        OStream(
            flushCallback callback,
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
            : bufferOwner_{buffer}
        {
            flushCallback_ = callback;
            buffer_ = bufferOwner_.get();
            sofab_ostream_init(&ctx_, buffer_, buflen, offset, static_flush_callback, this);
        }

        /*!
         * @brief Replace the active buffer (typically from within a flush callback).
         * @param buffer  New shared buffer to continue encoding into.
         * @param buflen  Usable size of @p buffer in bytes.
         * @param offset  Initial write offset within the new buffer (default 0).
         */
        void setBuffer(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
        {
            bufferOwner_ = buffer;
            buffer_ = bufferOwner_.get();
            sofab_ostream_buffer_set(&ctx_, buffer_, buflen, offset);
        }

        /*!
         * @brief Access the currently owned buffer.
         * @return Shared pointer to the active buffer.
         */
        std::shared_ptr<uint8_t[]> getBuffer() noexcept
        {
            return bufferOwner_;
        }
    };

    /*!
     * @brief Output stream backed by an inline, fixed-size buffer.
     *
     * Stores the @c N-byte buffer inside the object (no heap allocation), making
     * it well suited to embedded use.
     *
     * @tparam N       Total buffer size in bytes (must be > 0).
     * @tparam Offset  Initial write offset within the buffer (must be < @c N).
     */
    template <size_t N, size_t Offset = 0>
    class OStreamInline : public OStreamImpl
    {
        static_assert(N > 0, "Buffer size N must be greater than zero");
        static_assert(Offset < N, "Offset must be less than buffer size N");
        std::array<uint8_t, N> bufferOwner_ = {};   //!< Inline encode buffer.

    public:
        /*! @brief Construct with no flush callback. */
        OStreamInline() noexcept
        {
            buffer_ = bufferOwner_.data();
            sofab_ostream_init(&ctx_, buffer_, N, Offset, nullptr, nullptr);
        }

        /*!
         * @brief Construct with a flush callback.
         * @param callback  Invoked with buffered bytes when the buffer fills or on flush().
         */
        OStreamInline(flushCallback callback) noexcept
        {
            buffer_ = bufferOwner_.data();
            flushCallback_ = callback;
            sofab_ostream_init(&ctx_, buffer_, N, Offset, static_flush_callback, this);
        }
    };

    /*!
     * @brief Output stream over a buffer the caller already owns.
     *
     * Neither allocates nor copies: encoding writes straight into @p buffer. The
     * counterpart to @ref OStreamInline (buffer inside the object) and @ref
     * OStream (buffer held by a @c shared_ptr) — this is the one to use when the
     * destination already exists: a DMA or radio frame, a slot in a ring buffer,
     * or the @c dst of a generated @c encodeTo.
     *
     * The buffer must outlive the stream, and it is @b not restored if encoding
     * fails: a write past @p buflen ends in @ref Error::BufferFull with the bytes
     * written so far already in place. That is the price of not staging the
     * output elsewhere first — a caller needing all-or-nothing encodes into
     * scratch storage and copies on success.
     */
    class OStreamView : public OStreamImpl
    {
    public:
        /*!
         * @brief Construct over caller storage.
         * @param buffer  Destination; must outlive this stream.
         * @param buflen  Usable size of @p buffer in bytes.
         * @param offset  Initial write offset within the buffer (default 0).
         */
        OStreamView(uint8_t *buffer, size_t buflen, size_t offset = 0) noexcept
        {
            buffer_ = buffer;
            sofab_ostream_init(&ctx_, buffer_, buflen, offset, nullptr, nullptr);
        }

        /*!
         * @brief Construct over caller storage with a flush callback.
         * @param callback  Invoked with buffered bytes when the buffer fills or on flush().
         * @param buffer    Destination; must outlive this stream.
         * @param buflen    Usable size of @p buffer in bytes.
         * @param offset    Initial write offset within the buffer (default 0).
         */
        OStreamView(
            flushCallback callback, uint8_t *buffer, size_t buflen,
            size_t offset = 0) noexcept
        {
            buffer_ = buffer;
            flushCallback_ = callback;
            sofab_ostream_init(&ctx_, buffer_, buflen, offset, static_flush_callback, this);
        }
    };

    class OStreamMessage;

    /*! @brief Detects the CORELIB_PLAN §6.1.1 spelling of the worst-case size. */
    template <class T>
    concept HasMaxSize =
        requires { { T::MAX_SIZE } -> std::convertible_to<std::size_t>; } &&
        std::is_same_v<decltype(T::MAX_SIZE), const std::size_t>;

    /*! @brief Detects the pre-0.11 spelling. Deprecated; see @ref OutputMessage. */
    template <class T>
    concept HasLegacyMaxSize =
        requires { { T::_maxSize } -> std::convertible_to<std::size_t>; } &&
        std::is_same_v<decltype(T::_maxSize), const std::size_t>;

    /*!
     * @brief The worst-case encoded size of a generated message, whichever
     *        spelling it declares. Prefers `MAX_SIZE`.
     */
    template <class T>
    inline constexpr size_t max_size_v = [] () constexpr -> size_t
    {
        if constexpr (HasMaxSize<T>) { return T::MAX_SIZE; }
        else                         { return T::_maxSize; }
    }();

    /*!
     * @brief Concept: a serializable message type usable with @ref OStreamObject.
     *
     * Derives from @ref OStreamMessage and declares its worst-case encoded size.
     *
     * CORELIB_PLAN §6.1.1 closes the generated-object name set and puts
     * **`MAX_SIZE`** in it, permitting "only casing/idiom …, never the words".
     * This concept required `_maxSize`, an underscore-prefixed private-marker
     * spelling that is past casing — and because the concept is a hard
     * requirement, it fixed that name for every generated type built against this
     * corelib. (The C side was never affected: generated C emits
     * `MESSAGE_<NAME>_MAX_SIZE`, an idiomatic C macro spelling.)
     *
     * Both spellings are accepted so the rename is not a flag day: a generated
     * tree built against 0.10 keeps compiling while the `cpp` backend switches
     * over. `_maxSize` is deprecated and will be dropped once no emitter uses it.
     */
    template <class T>
    concept OutputMessage =
        std::derived_from<T, OStreamMessage> &&
        (HasMaxSize<T> || HasLegacyMaxSize<T>);

    /*!
     * @brief Base class for serializable message objects.
     *
     * Derive from this and implement @ref serialize to define how the message's
     * fields are written. Use with @ref OStreamObject to encode instances.
     */
    class OStreamMessage
    {
    protected:
        /*!
         * @brief Serialize this message's fields into @p _ostream.
         * @param _ostream  Output stream to write the fields to.
         * @return The encode @ref OStreamImpl::Result.
         */
        virtual OStream::Result
        serialize(OStreamImpl &_ostream) const noexcept = 0;
    };

    /*!
     * @brief Self-contained encoder that owns a message and an inline buffer.
     *
     * Bundles a @ref OStreamMessage instance with an @ref OStreamInline buffer
     * sized from the message's worst-case size, so a message can be populated and
     * encoded in one object.
     *
     * @tparam MessageType  An @ref OutputMessage type.
     * @tparam N            Buffer size in bytes (defaults to the message's
     *                      @c MAX_SIZE, or its deprecated @c _maxSize).
     * @tparam Offset       Initial write offset within the buffer.
     */
    template <OutputMessage MessageType, size_t N = max_size_v<MessageType>, size_t Offset = 0>
    class OStreamObject : public OStreamInline<N + Offset, Offset>
    {
        MessageType message_;   //!< The owned message instance.

    public:
        /*! @brief Construct with no flush callback. */
        OStreamObject() noexcept = default;
        /*!
         * @brief Construct with a flush callback.
         * @param callback  Invoked with buffered bytes when the buffer fills or on flush().
         */
        OStreamObject(typename OStream::flushCallback callback) noexcept
            : OStreamInline<N + Offset, Offset>{callback}
        { };

        /*! @brief Member-access to the owned message (e.g. @c obj->field = ...). */
        MessageType& operator->() noexcept
        {
            return message_;
        }

        /*!
         * @brief Serialize the owned message and flush.
         * @return The encode @ref OStreamImpl::Result.
         */
        OStream::Result serialize() noexcept
        {
            auto result =  message_.serialize(static_cast<OStreamImpl&>(*this));
            OStreamImpl::flush();

            return result;
        }
    };

    /*!
     * @brief @ref OStreamObject variant parameterized by offset only.
     *
     * Convenience alias-like class fixing @c N to the message's worst-case size while
     * letting the caller choose a leading @p Offset (e.g. to reserve a header).
     *
     * @tparam MessageType  An @ref OutputMessage type.
     * @tparam Offset       Initial write offset within the buffer.
     */
    template <OutputMessage MessageType, size_t Offset = 0>
    class OStreamObjectOffset : public OStreamObject<MessageType, max_size_v<MessageType>, Offset>
    {
    };


    /***************/
    /*** IStream ***/
    /***************/

    class IStreamMessage;
    /*! @brief Concept: a decodable message type usable with @ref IStreamObject
     *  and the nested-message @c IStreamImpl::read() overload. */
    template <typename T>
    concept InputMessage = std::derived_from<T, IStreamMessage>;

    /*!
     * @brief Compile-time capacity a heap-free container publishes, or -1.
     *
     * @ref InlineVector, @ref FixedString and @ref FixedBytes all declare
     * @c capacity() @c static @c constexpr, which is what tells them apart from
     * @c std::vector — whose @c capacity() is a per-object, runtime quantity and
     * so cannot be called on the type.
     *
     * @par Two readings, and which one applies is decided by the caller
     * A heap-free container is usually generated for a schema `count` /
     * `maxlen`, so its capacity is numerically that bound. Whether it is *read*
     * as that bound depends on whether the caller was told the bound some other
     * way, and the two callers in this header differ:
     *
     *  - **the reads** — @ref IStreamImpl::readString, @ref IStreamImpl::readBlob
     *    and @ref IStreamImpl::readArray take the schema bound as a parameter, so
     *    the capacity is *not* what states it to them. What the capacity means to
     *    a read is how much storage the caller offered — §6.3's **third** tier,
     *    and a value the schema and the cap both admit but the destination cannot
     *    hold is @ref Error::InvalidArgument, never @ref Error::InvalidMessage.
     *    A read passed no bound is told nothing about the schema and can only
     *    apply that third tier — which is why a capacity of -1 there, with no
     *    bound and no cap, leaves the read with **no** ceiling at all and is
     *    refused outright (@ref IStreamImpl::refuseUnbounded);
     *  - **the fixed-storage collectors** — @ref FixedMessageSeq and the fixed
     *    string/blob sequences take no `cap` at all, by design: their
     *    @c static_assert says the container's capacity *is* the schema bound
     *    they were generated for, because there is no other channel to state it
     *    on. For them it is §6.3's **first** tier, and an element past it is
     *    @ref Error::InvalidMessage (MESSAGE_SPEC §7.1).
     *
     * Both are correct, and the difference is not about the number — it is about
     * which channel states it. A collector generated *for* a bound carries it in
     * the only place it has; a read is told it outright, and applies it through
     * @ref IStreamImpl::refuseSchema (or @ref IStreamImpl::refuseCap, on the
     * `…Capped` entry point) before it sizes anything.
     *
     * @tparam C  Container type.
     */
    template <typename C>
    inline constexpr long fixed_capacity_v =
        [] () constexpr -> long
        {
            if constexpr (requires { { C::capacity() } -> std::convertible_to<std::size_t>; })
            {
                return static_cast<long>(C::capacity());
            }
            else
            {
                return -1;
            }
        }();


    /*!
     * @brief A CORELIB_PLAN §6.2.1 receiver cap, exactly as the caller states it.
     *
     * §6.2.1 draws a line this type exists to make unforgeable: the **provenance**
     * of a receiver limit is generated code's, and "there is no unset state and no
     * unlimited mode". A codec "**MUST NOT** hold a limit of its own, **MUST NOT**
     * supply a default for one it was not given, **MUST NOT** read an omitted
     * argument as *unlimited*".
     *
     * A `long dynCap = -1` parameter breaks the last of those by construction: the
     * sentinel *is* an omitted argument, and reading it as "no ceiling" is the
     * unlimited mode the clause forbids. This type has no such spelling.
     *
     *  - a value is stated by constructing from an unsigned count — @c DynCap{64};
     *  - a default-constructed @c DynCap is **unstated**, which is not "unlimited"
     *    but *missing*. Reaching it on a schema-unbounded field is an error in the
     *    **call** — the caller stated neither a schema bound nor a cap — and is
     *    reported as @ref Error::InvalidArgument (§6.3), never decoded uncapped;
     *  - a signed argument does not compile, so the old `-1` cannot be smuggled in
     *    as @c SIZE_MAX, which would be the unlimited mode wearing a new type.
     *
     * The number is still never held past the comparison it was passed for: a
     * @c DynCap lives in the caller's collector or on the caller's stack, and this
     * header only reads it.
     */
    class DynCap
    {
        std::size_t value_  = 0;
        bool        stated_ = false;

    public:
        /*! @brief The **unstated** cap: missing, not unlimited. */
        constexpr DynCap() noexcept = default;

        /*! @brief State the cap. @p v is a maximum: @c n @c > @c v is refused. */
        constexpr explicit DynCap(std::size_t v) noexcept : value_(v), stated_(true) {}

        /*! @brief Deleted: a signed cap would let `-1` re-enter as an unlimited mode. */
        template <typename T>
            requires std::is_signed_v<T>
        constexpr explicit DynCap(T) = delete;

        /*! @brief Whether the caller stated a number at all. */
        [[nodiscard]] constexpr bool stated() const noexcept { return stated_; }

        /*! @brief The stated maximum. Meaningless unless @ref stated. */
        [[nodiscard]] constexpr std::size_t value() const noexcept { return value_; }
    };


    /*!
     * @brief Base input stream: incrementally decodes fed bytes into bound targets.
     *
     * Thin C++ facade over @ref sofab_istream_t. Bytes are supplied via
     * @ref feed; inside a field callback (or a message's @c deserialize) the
     * matching @c read() overload binds the destination for the current field.
     * Decoding is lazy — a bound target is filled by subsequent @ref feed calls,
     * so targets must stay alive and address-stable until decoding completes.
     */
    class IStreamImpl
    {
    protected:
        sofab_istream_t ctx_;   //!< Underlying C input stream context.

        // Persistent decoder for variable-length-element array reads (see the
        // std::vector read() overloads below). The C decoder is deferred: a
        // read_sequence() only registers this decoder, and its per-element
        // callbacks fire later as feed() advances. It must therefore outlive the
        // field callback that started the array, so it lives in the stream object
        // rather than on the caller's stack.
        //
        // One member supports one active variable-length array at a time, which
        // covers all realistic schemas: array elements here are scalars (string /
        // blob), and sibling or nested-message array fields decode sequentially.
        // Genuinely nested variable-length arrays (e.g. vector<vector<string>>)
        // would need a decoder stack and are intentionally not supported.
        sofab_istream_decoder_t arrayDecoder_;

        // The category of a refusal a *callback* made, latched at the first one
        // and held until the stream is re-initialized.
        //
        // The C core has one sticky verdict, SOFAB_RET_E_INVALID_MSG, and that is
        // the whole of what it needs: it cannot be handed an unbounded field
        // (every C read carries its destination's size), so it can neither be
        // told of a breached receiver cap nor be handed a destination the caller
        // sized wrong. The wrapper can be both, so it keeps its own category
        // beside the core's flag: sofab_istream_invalidate() stops the decode and
        // makes it terminal, and this says which of §6.3's three refusals it was.
        // SOFAB_RET_OK means "no callback refusal" — then feed() reports whatever
        // the core decided.
        //
        // First-wins, because a decode is sequential: the first field to be
        // refused is the one that ended the message, and a later field in the same
        // fed chunk must not overwrite that verdict (the core keeps dispatching
        // within a feed after a callback rejects).
        uint8_t refusal_ = SOFAB_RET_OK;

        IStreamImpl() noexcept = default;

        // Latch a callback-side refusal and stop the decode. The category is
        // recorded here; the stop is the core's own sticky INVALID flag, which is
        // what keeps the rejection terminal across feeds (§5.2 / §6.3).
        void latchRefusal_(sofab_ret_t category) noexcept
        {
            if (refusal_ == SOFAB_RET_OK)
            {
                refusal_ = static_cast<uint8_t>(category);
            }
            sofab_istream_invalidate(&ctx_);
        }

        /*!
         * @brief Clear a latched refusal, for a subclass that re-initialises the
         *        stream itself.
         *
         * @ref invalidate documents the verdict as cleared by re-initialising the
         * stream, and the C core's own sticky flag is. This one is not: it is a
         * member, so it survives a bare `sofab_istream_init()` on the embedded
         * context and would report a stale category on the next message. The
         * library never hits that — `sofab_istream_init` is called from the
         * constructors — but a subclass that re-inits to reuse one object (as
         * `bench/cpp` does) must call this in the same breath.
         */
        void resetRefusal_() noexcept
        {
            refusal_ = SOFAB_RET_OK;
        }

        // Count a §7.3 skip the C core cannot see. The core counts a skip when a
        // bound read contradicts the wire; the reads that must check the type
        // *before* they touch their destination (readString, readBlob,
        // readSequence) decline without ever binding, so from the core's side
        // they are indistinguishable from a callback that simply was not
        // interested. Counting here keeps sofab_istream_skipped meaning the same
        // thing whichever side detected the contradiction. Saturating, like the
        // core's own increment.
        void noteSkip_() noexcept
        {
#if SOFAB_SKIP_COUNTER
            if (ctx_.skipped < 0xFF)
            {
                ctx_.skipped++;
            }
#endif
        }

        // ---- the bodies the capped and uncapped read entry points share ------
        //
        // §6.2.1 asks for ONE implementation of the rule wherever it runs, and the
        // two entry points differ only in WHICH ceiling they were handed: the
        // schema's, or the receiver's. Everything after that verdict -- the third
        // tier, the sizing, the bind -- is identical, so it lives here once rather
        // than being copied into the *Capped twin, where the two could drift.

        // Third tier, then size and bind a string/blob destination.
        template <typename T>
        void bindString_(T &value, size_t size) noexcept
        {
            if (refuse(size, fixed_capacity_v<T>))
            {
                return;
            }

            if constexpr (requires { value.set_len(size); })
            {
                value.set_len(size);
            }
            else
            {
                value.resize(size);
            }

            // a zero-length string binds no target: there is nothing to fill
            if (value.size())
            {
                read(value);
            }
        }

        template <typename T>
        void bindBlob_(T &value, size_t size) noexcept
        {
            if (refuse(size, fixed_capacity_v<T>))
            {
                return;
            }

            if constexpr (requires { value.set_len(size); })
            {
                value.set_len(size);
            }
            else
            {
                value.resize(size);
            }

            // Bind the destination's own size, not the wire length: a wire length
            // beyond the declared bound is then rejected as INVALID by the C
            // decoder rather than silently truncated (MESSAGE_SPEC §7.1).
            read(value.data(), value.size());
        }

        // MESSAGE_SPEC §7.3 for an array: does the delivered tag match the
        // destination's element type?
        template <typename C>
        [[nodiscard]] bool arrayTagOk_() noexcept
        {
            using Elem = typename C::value_type;

            if constexpr (std::is_same_v<Elem, float>)
            {
                return wire() == Wire::ArrayFixlen && fixType() == Fix::Fp32;
            }
            else if constexpr (std::is_same_v<Elem, double>)
            {
                return wire() == Wire::ArrayFixlen && fixType() == Fix::Fp64;
            }
            else if constexpr (std::is_signed_v<Elem>)
            {
                return wire() == Wire::ArraySigned;
            }
            else
            {
                return wire() == Wire::ArrayUnsigned;
            }
        }

        // What the destination can hold. A heap-free container publishes it as a
        // static capacity(); a std::array publishes it as its (fixed) size(); a
        // resizable container publishes none and takes whatever the message-side
        // ceiling admits.
        template <typename C>
        [[nodiscard]] static long arrayRoom_(C &out, size_t wireCount) noexcept
        {
            if constexpr (fixed_capacity_v<C> < 0 && !requires { out.resize(wireCount); })
            {
                (void)wireCount;
                return static_cast<long>(out.size());
            }
            else
            {
                (void)out;
                (void)wireCount;
                return fixed_capacity_v<C>;
            }
        }

        // Third tier, then size and bind an array destination.
        template <typename C>
        void bindArray_(C &out, size_t wireCount, long room) noexcept
        {
            if (refuse(wireCount, room))
            {
                return;
            }

            if constexpr (requires { out.resize(wireCount); })
            {
                out.resize(wireCount);
            }
            else
            {
                out = C{};
            }

            read(out);
        }


    public:
        /*!
         * @brief Outcome of a @ref feed call.
         *
         * Convertible to @c bool (true on success) and comparable to @ref Error.
         */
        class Result
        {
        private:
            Error error_ = Error::None;

            friend class IStreamImpl;
            Result(sofab_ret_t retval)
                : error_(static_cast<Error>(retval))
            { }

            sofab_ret_t rawCode() const noexcept
            {
                return static_cast<sofab_ret_t>(error_);
            }

        public:
            /*! @brief True if the feed reached a complete message boundary
             *  (same as ok()). An @ref Error::Incomplete partial decode is
             *  neither complete nor an error, so this is false — use
             *  @ref incomplete() to tell it apart from a genuine error. */
            explicit operator bool() const noexcept
            {
                return ok();
            }

            /*! @brief Compare the held error code to @p e. */
            bool operator==(Error e) const noexcept
            {
                return error_ == e;
            }

            /*! @brief Negated operator==(). */
            bool operator!=(Error e) const noexcept
            {
                return !(*this == e);
            }

            /*! @brief True if the consumed bytes end exactly on a field boundary
             *  (a complete message so far). False for both a partial decode
             *  (@ref incomplete()) and a genuine decode error. */
            bool ok() const noexcept
            {
                return error_ == Error::None;
            }

            /*! @brief True if the consumed bytes end mid-field or with an open
             *  sequence: a valid but partial decode (@ref Error::Incomplete).
             *  Not an error — the caller owns end-of-input and may feed more
             *  bytes to continue. Distinct from both ok() and a decode error. */
            bool incomplete() const noexcept
            {
                return error_ == Error::Incomplete;
            }

            /*! @brief True if the bytes are malformed (@ref Error::InvalidMessage).
             *  Deliberately false for @ref limitExceeded(): a receiver limit is a
             *  policy rejection on well-formed bytes (§6.2.1/§6.3), and the two
             *  must stay distinguishable to the caller. */
            bool invalid() const noexcept
            {
                return error_ == Error::InvalidMessage;
            }

            /*! @brief True if a configured receiver limit was exceeded on a
             *  schema-unbounded field (@ref Error::LimitExceeded, §6.2.1).
             *
             *  Means *"raise my limit, or the sender must send less"* — the same
             *  bytes decode on a receiver configured more loosely. Terminal, like
             *  @ref invalid(), but never the same answer. */
            bool limitExceeded() const noexcept
            {
                return error_ == Error::LimitExceeded;
            }

            /*! @brief The @ref Error result of the feed. */
            Error code() const noexcept
            {
                return error_;
            }
        };

        /*! @brief Copy construction is deleted (the context owns raw pointers). */
        IStreamImpl(const IStreamImpl&) = delete;
        /*! @brief Copy assignment is deleted (the context owns raw pointers). */
        IStreamImpl& operator=(const IStreamImpl&) = delete;

        /*! @brief Move construction (transfers the underlying context). */
        IStreamImpl(IStreamImpl&&) noexcept = default;
        /*! @brief Move assignment (transfers the underlying context). */
        IStreamImpl& operator=(IStreamImpl&&) noexcept = default;

        /*!
         * @brief Feed raw encoded bytes to the decoder.
         *
         * May be called repeatedly with arbitrary chunk boundaries; field
         * callbacks fire as complete field headers are parsed.
         *
         * The outcome is three-valued (no separate finalize step): the returned
         * @ref Result is @c ok() when the consumed bytes end on a field boundary
         * (a complete message), @ref Result::incomplete() when they end mid-field
         * or with an open sequence (a valid but partial decode — feed more bytes
         * to continue), and carries @ref Error::InvalidMessage when malformed.
         *
         * A callback may end the decode itself — @ref invalidate,
         * @ref exceedLimit or @ref refuseArgument — and the category it chose is
         * what this reports, in place of the core's plain
         * @ref Error::InvalidMessage. All three are terminal: every later feed
         * repeats the same answer until the stream is re-initialized.
         *
         * @param buffer  Pointer to the bytes to decode.
         * @param buflen  Number of bytes at @p buffer.
         * @return @ref Result: complete (ok), incomplete, or a decode error.
         */
        Result feed(const uint8_t *buffer, size_t buflen) noexcept
        {
            sofab_ret_t ret = sofab_istream_feed(&ctx_, buffer, buflen);
            if (refusal_ != SOFAB_RET_OK)
            {
                ret = static_cast<sofab_ret_t>(refusal_);
            }
            return Result{ret};
        }

        /*!
         * @brief Reject the message in progress as **malformed**, from within a
         *        field callback (§6.3's first tier).
         *
         * The bytes broke a bound the *schema* declares — a `maxlen`, a `count`,
         * an element index at or beyond a fixed-count array's capacity — which is
         * a statement about the message's *validity* (MESSAGE_SPEC §7.1), so it is
         * `INVALID`. Sticky: this @ref feed and every subsequent one report
         * @ref Error::InvalidMessage until the stream is re-initialized.
         *
         * Not for a receiver limit (@ref exceedLimit) and not for a destination
         * that is merely too small (@ref refuseArgument): §6.2.1 forbids folding
         * either into the `INVALID` outcome.
         */
        void invalidate() noexcept
        {
            latchRefusal_(SOFAB_RET_E_INVALID_MSG);
        }

        /*!
         * @brief Reject the message because a configured receiver limit was
         *        exceeded (§6.3's second tier, §6.2.1).
         *
         * For a **schema-unbounded** field only: the bytes are well-formed and the
         * same message decodes on a receiver configured more loosely, so this
         * reports @ref Error::LimitExceeded and never @ref Error::InvalidMessage.
         * Terminal, like @ref invalidate.
         *
         * @ref readStringCapped, @ref readBlobCapped, @ref readArrayCapped and the
         * growable wrapper-array collectors call it for you, at the length/count
         * header and before the destination is sized. Call it directly when a
         * callback enforces a limit those cannot see.
         */
        void exceedLimit() noexcept
        {
            latchRefusal_(SOFAB_RET_E_LIMIT_EXCEEDED);
        }

        /*!
         * @brief Reject the message because the **destination this caller handed
         *        over** cannot hold the value (§6.3's third tier, §6.6.3).
         *
         * Neither bound that speaks about the *message* has anything left to
         * object to — the schema admits the value and so does the configured
         * limit — and what does not fit is the storage. That is a mistake in the
         * *call*, so it is @c Error::InvalidArgument. @ref Error::InvalidMessage
         * would mark a well-formed message malformed, and @ref Error::LimitExceeded
         * would promise a limit to raise that was never configured. Terminal, like
         * @ref invalidate.
         */
        void refuseArgument() noexcept
        {
            latchRefusal_(SOFAB_RET_E_ARGUMENT);
        }

        /*!
         * @brief Refuse an announced length or element count the caller's own
         *        destination cannot hold.
         *
         * This is the **only** bound the codec judges. CORELIB_PLAN §6.2.1 gives
         * the receiver caps to generated code — "the visitor decides. The codec
         * never invents a limit of its own and never clamps to one" — and
         * MESSAGE_SPEC §7 gives it the schema bounds, because "the corelib cannot
         * know the schema". Both are measured by the handler, at the `size` /
         * `count` this corelib reports to it in the field callback, before it
         * binds a destination at all.
         *
         * What is left here is not a limit but the caller's storage: §6.6.3
         * requires "the codec refusing a destination too short rather than growing
         * it. That refusal is **`InvalidArgument`**: the message is well-formed and
         * within every bound it declares — what does not fit is the storage this
         * caller offered."
         *
         * Called before the destination is written, and terminal. Rejected, never
         * clamped — a short write with a `COMPLETE` verdict is the data corruption
         * §6.2.1 calls "a safety jacket".
         *
         * @param n     The announced length in bytes, or element count.
         * @param room  What the destination can actually hold, or -1 for a growable
         *              one, which this corelib never bounds.
         * @return `true` when the value was refused, in which case the caller must
         *         return without touching the destination.
         */
        [[nodiscard]] bool refuse(size_t n, long room) noexcept
        {
            if (room >= 0 && n > static_cast<size_t>(room))
            {
                refuseArgument();
                return true;
            }

            return false;
        }

        /*!
         * @brief §6.3's **first** tier: the bound the *schema* declares.
         *
         * A value past a declared `maxlen` / `count` is a statement about the
         * message's validity, so it is @ref Error::InvalidMessage (MESSAGE_SPEC
         * §7.1). The number is the **caller's**, passed in for this one comparison
         * and not kept: §6.2.1 leaves the codec "the report and the category" and
         * forbids it to *invent* or *store* a bound, which is not the same as
         * forbidding it to compare against one it was handed. Passing it here
         * rather than testing it before the call is what keeps a §7.3-skipped
         * field unjudged — every caller of this runs the tag test first, so a
         * header contradicting the declared type has already returned.
         *
         * @param n            The announced length in bytes, or element count.
         * @param schemaBound  Schema `maxlen`/`count`, or -1 when the schema
         *                     declares none, in which case nothing is refused here
         *                     and the field is the receiver cap's business instead.
         * @return `true` when the value was refused, in which case the caller
         *         must return without touching the destination.
         */
        [[nodiscard]] bool refuseSchema(size_t n, long schemaBound) noexcept
        {
            if (schemaBound >= 0 && n > static_cast<size_t>(schemaBound))
            {
                invalidate();
                return true;
            }
            return false;
        }

        /*!
         * @brief §6.3's **second** tier: the receiver cap on a schema-unbounded
         *        field (§6.2.1).
         *
         * The bytes are well-formed — the same message decodes on a receiver
         * configured more loosely — so a breach is @ref Error::LimitExceeded and
         * never @ref Error::InvalidMessage. Rejected, never clamped.
         *
         * @ref DynCap is the parameter type on purpose: a cap this corelib was not
         * given has no spelling here, so there is no omitted-argument path that
         * could be read as *unlimited* (§6.2.1). The value is used for this one
         * comparison and not retained.
         *
         * @param n       The announced length in bytes, or element count.
         * @param dynCap  The caller's cap. A maximum: `n > dynCap` is refused.
         * @return `true` when the value was refused.
         */
        [[nodiscard]] bool refuseCap(size_t n, size_t dynCap) noexcept
        {
            if (n > dynCap) { exceedLimit(); return true; }
            return false;
        }

        /*!
         * @brief Refuse a read that was given **no ceiling of any kind**.
         *
         * The gap corelib-c-cpp#152 was filed for, and the one a `long dynCap = -1`
         * parameter used to swallow. A read of a field the schema does not bound,
         * into a destination that publishes no capacity, with no receiver cap
         * stated, sizes the caller's storage from a number the **sender** chose.
         * §6.2.1 forbids treating that as *unlimited*: "there is no unset state and
         * no unlimited mode", and a format ceiling reached because no cap was
         * stated "is the FORMAT's bound and MUST NOT be presented as a receiver
         * cap".
         *
         * So it is diagnosed instead. Nothing about the message is wrong and no
         * limit was configured to raise, which rules out the other two categories:
         * the mistake is in the **call**, and §6.3 gives that
         * @ref Error::InvalidArgument. The fix at the call site is to state one of
         * the two ceilings — pass the schema bound, or use the `…Capped` entry
         * point with the receiver cap.
         *
         * A heap-free destination needs neither: its capacity is a ceiling the
         * sender cannot move, so `room >= 0` passes here and an over-capacity value
         * is refused by @ref refuse as the third tier.
         *
         * @param schemaBound  Schema `maxlen`/`count`, or -1 for none.
         * @param room         The destination's static capacity, or -1 for a
         *                     growable one.
         * @return `true` when the read must not proceed.
         */
        [[nodiscard]] bool refuseUnbounded(long schemaBound, long room) noexcept
        {
            if (schemaBound < 0 && room < 0) { refuseArgument(); return true; }
            return false;
        }

        /*!
         * @brief The two **message-side** refusals of §6.3 in one call, in the
         *        order §6.2.1 fixes: the schema first, the receiver cap only where
         *        the schema declared nothing — and a **diagnosis** where neither
         *        was stated.
         *
         * @ref refuse is the third tier and answers a different question — that one
         * is about the caller's storage, these two are about the message.
         *
         * This is the collectors' entry point (@ref seqRefuse). A growable wrapper
         * array has no capacity to fall back on, so "the schema declared nothing
         * and no cap was stated" is not a decode without a ceiling: it is
         * @ref Error::InvalidArgument, exactly as in @ref refuseUnbounded.
         *
         * @param n            The announced length in bytes, or element index + 1.
         * @param schemaBound  Schema `maxlen`/`count`, or -1 when the schema
         *                     declares none — which is what arms @p dynCap.
         * @param dynCap       The §6.2.1 receiver cap for a schema-unbounded field.
         *                     An unstated one is refused, never read as unlimited.
         * @return `true` when the value was refused, in which case the caller
         *         must return without touching the destination.
         */
        [[nodiscard]] bool refuseBound(size_t n, long schemaBound, DynCap dynCap) noexcept
        {
            if (schemaBound >= 0)
            {
                // Schema-bounded: its violation is INVALID (MESSAGE_SPEC §7.1),
                // and §6.2.1 forbids the cap from reaching this field at all.
                return refuseSchema(n, schemaBound);
            }

            if (!dynCap.stated()) { refuseArgument(); return true; }
            return refuseCap(n, dynCap.value());
        }

        /*!
         * @brief Wire type of the field currently being delivered.
         *
         * Valid inside a field callback / @c deserialize, before the field's
         * @ref read binds a destination. MESSAGE_SPEC §7.3: compare it against
         * the wire type the declared field maps to and, on a mismatch, return
         * without calling @ref read — the field is then skipped automatically,
         * exactly like an unknown id (a framing mismatch, not a malformed
         * message). Reads no bytes and does not consume the field.
         *
         * @return The delivered field's @ref Wire.
         */
        [[nodiscard]] Wire wire() const noexcept
        {
            // ctx_.target_opt holds the delivered wire opt on callback entry
            // (low 3 bits = wire type); a later read() overwrites it with the
            // schema opt, so this must be read before binding a destination.
            return static_cast<Wire>(ctx_.target_opt & 0x07);
        }

        /*!
         * @brief Fixlen sub-type of the field currently being delivered.
         *
         * Only meaningful when @ref wire is @ref Wire::Fixlen or
         * @ref Wire::ArrayFixlen; §7.3 bounds the type check at wire type *plus*
         * this subtype, since @c fp32 / @c fp64 / @c string / @c blob share the
         * fixlen wire type. Valid inside a field callback / @c deserialize before
         * @ref read; reads no bytes and does not consume the field.
         *
         * @return The delivered field's @ref Fix.
         */
        [[nodiscard]] Fix fixType() const noexcept
        {
            // The fixlen subtype is merged into bits 3..5 of target_opt before
            // the callback fires (istream.c: target_opt |= fixlen_type << 3).
            return static_cast<Fix>((ctx_.target_opt >> 3) & 0x07);
        }

        /*!
         * @brief Bind the current field to a typed destination, deducing the
         *        wire type from @p T.
         *
         * Call from within a field callback / @c deserialize for the active
         * field. Supports integers, @c bool, @c float, @c double, @c std::string
         * (pre-sized to the field length), fixed contiguous ranges
         * (arrays/vectors/spans), and nested @ref IStreamMessage objects. The
         * destination must outlive decoding (filled by later @ref feed calls).
         *
         * @param value  Destination to decode into.
         */
        template <typename T>
        void read(T &value) noexcept
        {
            if constexpr (!std::is_const_v<T>)
            {
                if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
                {
#if !SOFAB_CPP_HAVE_INT64
                    static_assert(sizeof(T) <= 4,
                        "64-bit integer fields require INT64 support, disabled "
                        "via SOFAB_DISABLE_INT64_SUPPORT");
#endif
                    if constexpr (std::is_unsigned_v<T>)
                    {
                        sofab_istream_read_field(
                            &ctx_, reinterpret_cast<void*>(&value), sizeof(T),
                            SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_UNSIGNED));
                    }
                    else
                    {
                        sofab_istream_read_field(
                            &ctx_, reinterpret_cast<void*>(&value), sizeof(T),
                            SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINT_SIGNED));
                    }
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    sofab_istream_read_bool(&ctx_, &value);
                }
                else if constexpr (std::is_same_v<T, float>)
                {
                    sofab_istream_read_fp32(&ctx_, &value);
                }
                else if constexpr (std::is_same_v<T, double>)
                {
#if SOFAB_CPP_HAVE_FP64
                    sofab_istream_read_fp64(&ctx_, &value);
#else
                    static_assert(always_false_v<T>,
                        "double (FP64) fields require FP64 support, disabled "
                        "via SOFAB_DISABLE_FP64_SUPPORT");
#endif
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    // std::string doesn't need a null terminator, so we use read_noterm
                    sofab_istream_read_string_noterm(&ctx_, value.data(), value.size());
                }
                else if constexpr (is_fixed_string_v<T>)
                {
                    // FixedString<N>: the heap-free counterpart of std::string.
                    // The caller has already fixed the logical length (and the
                    // terminating NUL) via set_len(_size), so value.size() is the
                    // field length and value.data() the inline buffer. read_noterm
                    // fills [0, size()) and never touches the pre-placed NUL, so
                    // c_str()/string_view stay valid. Matched before the span
                    // branch so a char buffer is never treated as an array.
                    sofab_istream_read_string_noterm(&ctx_, value.data(), value.size());
                }
                else if constexpr (is_fixed_bytes_v<T>)
                {
                    // FixedBytes<N>: the heap-free counterpart of a blob field.
                    // The caller has already fixed the logical length via
                    // set_len(_size), so value.size() is the field length clamped
                    // to the capacity N. Binding that (rather than the raw wire
                    // _size a bare read(data(), _size) would pass) lets the C
                    // decoder enforce the schema bound: a wire length > N exceeds
                    // the bound and is rejected as INVALID - never silently
                    // truncated - per MESSAGE_SPEC §7.1. Matched before the span
                    // branch so the byte buffer is never treated as an array.
                    sofab_istream_read_blob(&ctx_, value.data(), value.size());
                }
                else if constexpr (InputMessage<T>)
                {
                    // descend into a nested sequence: bind this message's own
                    // decoder + field callback to the active stream context, so
                    // its child fields dispatch to value.deserialize(...). Mirrors
                    // the C object API's sequence handling (object.c).
                    value.readNested_(&ctx_, this);
                }
                else if constexpr (
                    requires {
                        typename T::value_type;
                        std::span{ std::declval<const T&>() };
                    } &&
                    !std::is_const_v<typename T::value_type>)
                {
#if SOFAB_CPP_HAVE_ARRAY
                    using Elem = typename T::value_type;
                    std::span<Elem> span{value};

                    if constexpr (std::is_integral_v<Elem> && !std::is_same_v<Elem, bool>)
                    {
#if !SOFAB_CPP_HAVE_INT64
                        static_assert(sizeof(Elem) <= 4,
                            "64-bit integer arrays require INT64 support, "
                            "disabled via SOFAB_DISABLE_INT64_SUPPORT");
#endif
                        if constexpr (std::is_unsigned_v<Elem>)
                        {
                            sofab_istream_read_array(
                                &ctx_,
                                span.data(),
                                static_cast<int32_t>(span.size()),
                                sizeof(Elem),
                                SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_UNSIGNED));
                        }
                        else
                        {
                            sofab_istream_read_array(
                                &ctx_,
                                span.data(),
                                static_cast<int32_t>(span.size()),
                                sizeof(Elem),
                                SOFAB_ISTREAM_OPT_FIELDTYPE(SOFAB_TYPE_VARINTARRAY_SIGNED));
                        }
                    }
                    else if constexpr (std::is_same_v<Elem, float>)
                    {
                        sofab_istream_read_array_of_fp32(
                            &ctx_,
                            span.data(),
                            static_cast<int32_t>(span.size()));
                    }
                    else if constexpr (std::is_same_v<Elem, double>)
                    {
#if SOFAB_CPP_HAVE_FP64
                        sofab_istream_read_array_of_fp64(
                            &ctx_,
                            span.data(),
                            static_cast<int32_t>(span.size()));
#else
                        static_assert(always_false_v<T>,
                            "double (FP64) arrays require FP64 support, "
                            "disabled via SOFAB_DISABLE_FP64_SUPPORT");
#endif
                    }
                    else
                    {
                        static_assert(always_false_v<T>,
                            "Unsupported span element type in IStream::read()");
                    }
#else
                    static_assert(always_false_v<T>,
                        "array/span fields require ARRAY support, disabled via "
                        "SOFAB_DISABLE_ARRAY_SUPPORT");
#endif
                }
                else
                {
                    static_assert(always_false_v<T>,
                        "Unsupported type passed to IStream::read()");
                }
            }
            else
            {
                static_assert(always_false_v<T>,
                    "Cannot read into const variable in IStream::read()");
            }
        }

        /*!
         * @brief Decode a blob field into a caller-owned, address-stable buffer.
         *
         * The templated read() above routes std::string to a STRING-tagged read
         * and a std::vector<uint8_t> span to a VARINTARRAY-tagged read; neither
         * matches a BLOB field, so the C type check rejects them and the bytes are
         * dropped. This overload binds with the BLOB tag (via read_blob) so blob
         * fields decode correctly. The caller must size the destination to the
         * field length (provided as `size` in the field callback); the bytes are
         * filled by a subsequent feed() pass into this buffer, which must stay
         * alive and unmoved until decoding of the field completes.
         *
         * @param dst     Destination buffer of at least @p maxlen bytes.
         * @param maxlen  Field length to read (a zero length is a no-op).
         * @return Number of bytes bound to be read (@p maxlen, or 0 if @p maxlen is 0).
         */
        size_t read(void *dst, size_t maxlen) noexcept
        {
            // read_field() asserts varlen > 0; a zero-length blob binds no target
            // and is simply skipped (0 payload bytes), leaving dst untouched.
            if (maxlen == 0)
            {
                return 0;
            }

            sofab_istream_read_blob(&ctx_, dst, maxlen);
            return maxlen;
        }

        /*!
         * @brief Bind a string field, sizing the destination first.
         *
         * A string destination has to be given its logical length before the read
         * binds it, and that is a change to the destination — so unlike a scalar
         * read it cannot be issued unconditionally and left to the decoder to
         * unbind. This checks the delivered type first (MESSAGE_SPEC §7.3) and
         * touches @p value only if the field really is a string: a contradicting
         * field leaves the destination exactly as it was and is skipped like an
         * unknown id.
         *
         * Accepts both storage shapes: a @ref FixedString (inline, @c set_len)
         * and a @c std::string (@c resize), so the same call works in the no-heap
         * and the dynamic profile.
         *
         * @par The four ways a read refuses (§6.3)
         * All of them are decided on the length header, after the §7.3 type check
         * and before @p value is sized — and they are four different answers, not
         * one:
         *  1. @p maxlen is declared and the field is longer → @ref Error::InvalidMessage.
         *     The schema says these bytes are invalid (MESSAGE_SPEC §7.1).
         *  2. the schema declares no bound and the field is longer than the
         *     receiver cap → @ref Error::LimitExceeded. The bytes are well-formed;
         *     this receiver declines to hold that much (§6.2.1). That is
         *     @ref readStringCapped's answer, and only its: a cap must never be
         *     applied to a field the schema already bounds.
         *  3. Both admit the field, but @p value is a @ref FixedString too small
         *     for it → @ref Error::InvalidArgument (§6.6.3). Nothing is wrong with
         *     the message or with the deployment; the storage this caller offered
         *     is too short, and it is refused rather than filled part-way.
         *  4. No ceiling was stated at all — no @p maxlen, no cap, and a growable
         *     @p value → @ref Error::InvalidArgument (§6.2.1, @ref refuseUnbounded).
         *     The message and the deployment are again blameless; what is missing
         *     is part of the call.
         * A `std::string` destination never reaches 3: it can hold whatever 1 and
         * 2 let through. A @ref FixedString never reaches 4: its capacity is a
         * ceiling of its own.
         *
         * @par This is the schema-bounded entry point
         * §6.2.1 gives the receiver caps to generated code and leaves this corelib
         * "the report and the category", so there is no limit on the stream to fall
         * back on — and, since the clause also forbids reading an omitted argument
         * as *unlimited*, there is no `dynCap = -1` here to omit. A field the schema
         * leaves unbounded goes through @ref readStringCapped instead, which takes
         * the cap and cannot be called without one. The two are never both in play:
         * §6.2.1 forbids a receiver cap on a field the schema already bounds.
         *
         * A read given neither ceiling — no @p maxlen, and a growable @p value that
         * publishes no capacity — is refused as @ref Error::InvalidArgument
         * (@ref refuseUnbounded), not decoded. A heap-free @p value needs no
         * @p maxlen: its capacity is a ceiling of its own.
         *
         * This is what generated code emits for the embedded profile, where every
         * `string` carries a `maxlen`:
         * ```cpp
         * is.readString(name, _size, 32);
         * ```
         *
         * @param value   Destination string.
         * @param size    Field length, as delivered to the field callback.
         * @param maxlen  Schema `maxlen`, or -1 when the schema declares none — in
         *                which case @p value must publish a capacity.
         */
        template <typename T>
        void readString(T &value, size_t size, long maxlen = -1) noexcept
        {
            if (wire() != Wire::Fixlen || fixType() != Fix::String)
            {
                noteSkip_();
                return;
            }

            if (refuseUnbounded(maxlen, fixed_capacity_v<T>) || refuseSchema(size, maxlen))
            {
                return;
            }

            bindString_(value, size);
        }

        /*!
         * @brief @ref readString for a **schema-unbounded** `string`, bounded by the
         *        receiver cap the caller supplies (§6.2.1).
         *
         * The separate entry point is the point. @p dynCap is required and unsigned,
         * so "no cap" has no spelling: §6.2.1's "no unset state and no unlimited
         * mode" becomes a property of the signature rather than a rule every caller
         * has to remember. A breach is @ref Error::LimitExceeded — a **policy**
         * rejection of well-formed bytes, never @ref Error::InvalidMessage and never
         * a truncating read.
         *
         * The cap is applied at the length header, after the MESSAGE_SPEC §7.3 tag
         * test and before @p value is sized. Both halves matter: checking in front
         * of the call would cap a field this read is required to *skip*, which
         * §6.2.1 forbids in as many words, and checking after the resize would be
         * the allocation the cap exists to prevent.
         *
         * @p dynCap is used for this one comparison and not retained — the number is
         * the caller's, and this corelib neither defaults it nor remembers it.
         *
         * ```cpp
         * is.readStringCapped(name, _size, SOFAB_MAX_DYN_STRING_LEN);
         * ```
         *
         * @param value   Destination string.
         * @param size    Field length, as delivered to the field callback.
         * @param dynCap  The §6.2.1 receiver cap. A maximum, not an exclusive bound.
         */
        template <typename T>
        void readStringCapped(T &value, size_t size, size_t dynCap) noexcept
        {
            if (wire() != Wire::Fixlen || fixType() != Fix::String)
            {
                noteSkip_();
                return;
            }

            if (refuseCap(size, dynCap))
            {
                return;
            }

            bindString_(value, size);
        }

        /*!
         * @brief Bind a blob field, sizing the destination first.
         *
         * The blob counterpart of @ref readString, and for the same reason: the
         * destination must be sized before it is bound, so the delivered type is
         * checked before @p value is touched (MESSAGE_SPEC §7.3).
         *
         * The refusals of @ref readString apply unchanged, and so does the split
         * into two entry points: this one takes the schema `maxlen`,
         * @ref readBlobCapped takes `SOFAB_MAX_DYN_BLOB_LEN`. `blob` and `string`
         * are separate limits in §6.2.1 because a deployment may well accept a
         * megabyte of opaque bytes and no such quantity of text.
         *
         * @param value   Destination byte buffer (@ref FixedBytes or
         *                @c std::vector<uint8_t>).
         * @param size    Field length, as delivered to the field callback.
         * @param maxlen  Schema `maxlen`, or -1 when the schema declares none — in
         *                which case @p value must publish a capacity.
         */
        template <typename T>
        void readBlob(T &value, size_t size, long maxlen = -1) noexcept
        {
            if (wire() != Wire::Fixlen || fixType() != Fix::Blob)
            {
                noteSkip_();
                return;
            }

            if (refuseUnbounded(maxlen, fixed_capacity_v<T>) || refuseSchema(size, maxlen))
            {
                return;
            }

            bindBlob_(value, size);
        }

        /*!
         * @brief @ref readBlob for a **schema-unbounded** `blob`, bounded by the
         *        receiver cap the caller supplies (§6.2.1).
         *
         * The blob counterpart of @ref readStringCapped, with the same required,
         * unsigned @p dynCap and the same ordering guarantees.
         *
         * @param value   Destination byte buffer.
         * @param size    Field length, as delivered to the field callback.
         * @param dynCap  The §6.2.1 receiver cap. A maximum, not an exclusive bound.
         */
        template <typename T>
        void readBlobCapped(T &value, size_t size, size_t dynCap) noexcept
        {
            if (wire() != Wire::Fixlen || fixType() != Fix::Blob)
            {
                noteSkip_();
                return;
            }

            if (refuseCap(size, dynCap))
            {
                return;
            }

            bindBlob_(value, size);
        }

        /*!
         * @brief Bind a native scalar array, preparing the destination first.
         *
         * Like @ref readString, this exists because the destination is touched
         * before it is bound — an inline @c std::array is reset so elements the
         * encoder trimmed off the tail decode as the element default rather than
         * as a schema default (MESSAGE_SPEC §3), and a @c std::vector is sized to
         * the wire count. Both must wait until the delivered field is known to be
         * an array of this element type (§7.3), and the count must be checked
         * against the schema bound before a resize, so an over-count message
         * cannot make the receiver allocate what the bound exists to prevent.
         *
         * The refusals of @ref readString apply unchanged, counting elements
         * instead of bytes, and so does the split into two entry points: this one
         * takes the schema `count`, @ref readArrayCapped takes
         * `SOFAB_MAX_DYN_ARRAY_COUNT`. Tier 3 matters more here than it does for a
         * string: a heap-free destination's @c resize *clamps*, so without the
         * check an over-capacity count would decode as a silently short array
         * rather than as a refusal.
         *
         * @param out        Destination array or vector.
         * @param wireCount  Element count delivered to the field callback.
         * @param cap        Schema `count`, or -1 when the schema declares none —
         *                   in which case @p out must publish a capacity.
         */
        template <typename C>
        void readArray(C &out, size_t wireCount = 0, long cap = -1) noexcept
        {
            if (!arrayTagOk_<C>())
            {
                noteSkip_();
                return;
            }

            const long room = arrayRoom_(out, wireCount);

            if (refuseUnbounded(cap, room) || refuseSchema(wireCount, cap))
            {
                return;
            }

            bindArray_(out, wireCount, room);
        }

        /*!
         * @brief @ref readArray for a **schema-unbounded** array, bounded by the
         *        receiver cap the caller supplies (§6.2.1).
         *
         * The array counterpart of @ref readStringCapped: @p dynCap is required and
         * unsigned, it is compared against the wire count at the count header —
         * behind the §7.3 tag test, before anything is sized from it — and a breach
         * is @ref Error::LimitExceeded.
         *
         * @param out        Destination array or vector.
         * @param wireCount  Element count delivered to the field callback.
         * @param dynCap     The §6.2.1 receiver cap on the element count.
         */
        template <typename C>
        void readArrayCapped(C &out, size_t wireCount, size_t dynCap) noexcept
        {
            if (!arrayTagOk_<C>())
            {
                noteSkip_();
                return;
            }

            if (refuseCap(wireCount, dynCap))
            {
                return;
            }

            bindArray_(out, wireCount, arrayRoom_(out, wireCount));
        }

        /*!
         * @brief Bind a wrapper-array field through a collector, clearing the
         *        destination first.
         *
         * A wrapper array arrives as a sequence whose element index is the field
         * id (MESSAGE_SPEC §5.1); @p collector turns those elements into entries
         * of @p out. The destination is emptied first — a wrapper array replaces
         * rather than merges (§7.4) — and that, again, is a change that must not
         * happen for a field which turns out not to be a sequence at all.
         *
         * @param collector  Collector for the element type (e.g. @ref FixedStringSeq).
         * @param out        Destination container; must outlive decoding.
         */
        template <typename C, typename Out>
        void readSequence(C &collector, Out &out) noexcept
        {
            if (wire() != Wire::SequenceStart)
            {
                noteSkip_();
                return;
            }

            out.clear();
            collector.out = &out;
            read(collector);
        }

        /*!
         * @brief Number of fields skipped because their wire type contradicted
         *        the destination bound for them.
         *
         * Facade over @ref sofab_istream_skipped. Not an error count: a skip is
         * how a reader and a writer built from different revisions of a schema
         * keep talking (MESSAGE_SPEC §7.3). A non-zero value after a successful
         * decode means the two sides disagree about what an id means, which is
         * worth a log line or a health metric. Saturates at 255.
         *
         * @return Number of type-contradicting fields skipped.
         */
#if SOFAB_SKIP_COUNTER
        [[nodiscard]] uint8_t skipped() const noexcept
        {
            return sofab_istream_skipped(&ctx_);
        }
#endif

        /*!
         * @brief Decode a sequence of variable-length string elements into a vector.
         *
         * Each element is emplaced into `out` and the C read target is bound to
         * that persistent slot, so feed() fills it in place before the next
         * element's callback fires. A later emplace_back may reallocate `out`, but
         * it only moves already-filled elements (heap-stable, or SSO bytes copied
         * intact), never an unfilled bound target. This is the safe counterpart to
         * the transient read-into-local-then-move pattern, which dangles under the
         * deferred decoder.
         *
         * @param out  Vector that receives one element per string in the sequence;
         *             it must outlive decoding of the field.
         */
        void read(std::vector<std::string> &out) noexcept
        {
            sofab_istream_read_sequence(
                &ctx_, &arrayDecoder_, &strArrayElem_, &out);
        }

        /*!
         * @brief Decode a sequence of variable-length blob elements into a vector.
         * @param out  Vector that receives one byte-vector per blob in the sequence;
         *             it must outlive decoding of the field.
         */
        void read(std::vector<std::vector<uint8_t>> &out) noexcept
        {
            sofab_istream_read_sequence(
                &ctx_, &arrayDecoder_, &blobArrayElem_, &out);
        }

    private:
        /*! @brief Per-element callback: emplace and bind one string element. */
        static void strArrayElem_(
            sofab_istream_t *ctx, sofab_id_t, size_t size, size_t, void *usrptr)
        {
            auto *out = static_cast<std::vector<std::string>*>(usrptr);
            out->emplace_back(size, '\0');
            // read_field() asserts varlen > 0; an empty element binds no target
            // and the (zero-length) payload is skipped, leaving "" in place.
            if (size > 0)
            {
                sofab_istream_read_string_noterm(ctx, out->back().data(), size);
            }
        }

        /*! @brief Per-element callback: emplace and bind one blob element. */
        static void blobArrayElem_(
            sofab_istream_t *ctx, sofab_id_t, size_t size, size_t, void *usrptr)
        {
            auto *out = static_cast<std::vector<std::vector<uint8_t>>*>(usrptr);
            out->emplace_back(size);
            // read_field() asserts varlen > 0; an empty element binds no target
            // and the (zero-length) payload is skipped, leaving {} in place.
            if (size > 0)
            {
                sofab_istream_read_blob(ctx, out->back().data(), size);
            }
        }
    };

    /*!
     * @brief Input stream driven by a user-supplied per-field callback.
     *
     * The callback receives each decoded field's id, size and element count, and
     * binds a destination by calling one of the @c IStreamImpl::read() overloads
     * for the fields it is interested in; unhandled fields are skipped.
     */
    class IStreamInline : public IStreamImpl
    {
    public:
        /*! @brief Per-field callback signature: (field id, value size, element count). */
        using fieldCallback = std::function<void(sofab::id _id, size_t _size, size_t _count)>;

    private:
        fieldCallback callback_;

        /*! @brief C-ABI field-callback trampoline forwarding to the std::function. */
        static void field_callback_(
            sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
        {
            (void)ctx;

            auto *self = static_cast<IStreamInline*>(usrptr);
            self->callback_(id, size, count);
        }

    public:
        /*!
         * @brief Construct with the per-field callback.
         * @param callback  Invoked for each decoded field to bind a destination.
         */
        IStreamInline(fieldCallback callback) noexcept
            : callback_{callback}
        {
            sofab_istream_init(&ctx_, field_callback_, this);
        }
    };

    /*!
     * @brief Base class for decodable message objects.
     *
     * Derive from this and implement @ref deserialize to dispatch each field to
     * the matching @c IStreamImpl::read(). Instances are decoded standalone via
     * @ref IStreamObject, or as a nested field through @c IStreamImpl::read().
     */
    class IStreamMessage
    {
    private:
        sofab_istream_decoder_t decoder_;

        friend class IStreamImpl;
        template <InputMessage MessageType>
        friend class IStreamObject;

        struct Context
        {
            IStreamImpl *istream;
            IStreamMessage *message;
        };

        Context context_{nullptr, nullptr};

        static void field_callback_(
            sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
        {
            (void)ctx;

            auto context = static_cast<Context*>(usrptr);
            context->message->deserialize(*context->istream, id, size, count);
        }

        // Wire this message up as a nested sequence decoded on the active stream:
        // bind its own decoder + field callback so each child field is dispatched
        // to this->deserialize(...). `ctx`/`istream` are passed in by IStreamImpl
        // so all stream-internal access stays inside IStreamImpl::read().
        void readNested_(sofab_istream_t *ctx, IStreamImpl *istream) noexcept
        {
            context_ = Context{istream, this};
            sofab_istream_read_sequence(ctx, &decoder_, field_callback_, &context_);
        }

    public:
        /*!
         * @brief Dispatch one decoded field to the appropriate read.
         *
         * Called for each field of this message; implementations typically
         * switch on @p _id and call @c _istream.read(member).
         *
         * @param _istream  Stream to bind the field's destination on.
         * @param _id       Field identifier.
         * @param _size     Field value size in bytes (e.g. string/blob length).
         * @param _count    Number of array elements (for array fields).
         */
        virtual void deserialize(sofab::IStreamImpl &_istream, sofab::id _id, size_t _size, size_t _count) noexcept = 0;
    };

    /*!
     * @brief A message that both encodes and decodes: exactly
     *        @ref OStreamMessage + @ref IStreamMessage.
     *
     * Almost every message is both, so spelling out the pair at every declaration
     * is noise. This is an empty intermediate base — no storage, no vtable slot of
     * its own, identical layout to inheriting the two directly — so it is a naming
     * convenience and nothing else:
     *
     * ```cpp
     * struct Telemetry : sofab::Message {
     *     sofab::OStreamImpl::Result serialize(sofab::OStreamImpl &os) const noexcept override;
     *     void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override;
     * };
     * ```
     *
     * Both @ref sofab::InputMessage and @ref sofab::OutputMessage are satisfied
     * through it. Inherit a single side directly when a type really is one-way.
     */
    struct Message : OStreamMessage, IStreamMessage
    {
    };

    /*!
     * @brief Self-contained decoder that owns a message instance.
     *
     * Wires a @ref IStreamMessage subclass to an input stream so fed bytes are
     * dispatched to its @c deserialize. Access the decoded message via @c -> or
     * @c *.
     *
     * @tparam MessageType  An @ref InputMessage type.
     */
    template <InputMessage MessageType>
    class IStreamObject : public IStreamImpl
    {
        MessageType data_;  //!< The owned message instance being decoded into.

    public:
        /*! @brief Construct and bind the owned message to this stream. */
        IStreamObject() noexcept
        {
            data_.context_ = IStreamMessage::Context{this, &data_};
            sofab_istream_init(&ctx_, MessageType::field_callback_, &data_.context_);
        }

        /*! @brief Member-access to the owned message (e.g. @c obj->field). */
        MessageType& operator->() noexcept
        {
            return data_;
        }

        /*! @brief Const member-access to the owned message. */
        const MessageType& operator->() const noexcept
        {
            return data_;
        }

        /*! @brief Reference to the owned message. */
        MessageType& operator*() noexcept
        {
            return data_;
        }

        /*! @brief Const reference to the owned message. */
        const MessageType& operator*() const noexcept
        {
            return data_;
        }
    };

    /* ---------------------------------------------------------------------- */
    /* Wrapper-sequence collectors and encode helpers                         */
    /*                                                                        */
    /* MESSAGE_SPEC §5 lowers an array of strings, blobs, structs or nested   */
    /* arrays to a sequence whose child ids are the element indices. These    */
    /* collect such a sequence into this profile's heap-free containers. They  */
    /* mirror sofab::StringSeq / BlobSeq in corelib-cpp so both C++ outputs    */
    /* read the same; the difference is the storage they fill —                */
    /* InlineVector<FixedString<M>, N> here, std::vector<std::string> there.   */
    /*                                                                        */
    /* The MessageSeq/FixedMessageSeq counterpart for struct, union and row    */
    /* elements is in seq.hpp, which this header includes. It used to be       */
    /* missing: an earlier pair appended elements in ARRIVAL ORDER and grew    */
    /* the container before deciding the child's wire type, so an interior id  */
    /* gap shifted every later element down by one (§5.1) and a §7.3           */
    /* mismatched child left a phantom (generator#249), and it was removed     */
    /* rather than shipped as a parity API that behaves unlike the one it      */
    /* claims to mirror. The pair in seq.hpp places at the element id, like    */
    /* the four below and like corelib-cpp's MessageSeq.                       */
    /* ---------------------------------------------------------------------- */

    /**
     * @brief Collects a `string` wrapper sequence into inline storage.
     *
     * An element is *placed* at its index id rather than appended: a default
     * (empty) element is omitted on the wire (§2), so the vector is grown with
     * empty slots up to the id and the value stored there. Inline storage never
     * reallocates, so an element bound earlier stays address-stable while later
     * slots grow — which the deferred C decoder relies on.
     *
     * An index at or past the fixed capacity N is a schema-bound violation
     * (§5.1/§7) and is rejected with @ref IStreamImpl::invalidate, before the fill
     * loop. That also bounds an over-index amplification: InlineVector's
     * emplace_back is a no-op once full, so an unguarded loop would spin forever
     * on such an index (issue #126).
     *
     * @tparam Container Inline vector of @ref FixedString.
     */
    template <typename Container>
    struct FixedStringSeq : IStreamMessage
    {
        Container *out = nullptr;

        void deserialize(IStreamImpl &is, sofab_id_t id, size_t size, size_t) noexcept override
        {
            if (is.wire() != Wire::Fixlen || is.fixType() != Fix::String) return; /* §7.3 */
            if (static_cast<size_t>(id) >= out->capacity())
            {
                is.invalidate();
                return;
            }
            while (out->size() <= static_cast<size_t>(id)) out->emplace_back();
            auto &s = (*out)[id];
            s.set_len(size);
            if (size) is.read(s);
        }
    };

    /*!
     * The `blob` counterpart of @ref FixedStringSeq; same placement and bound
     * rules, filling @ref FixedBytes slots.
     */
    template <typename Container>
    struct FixedBlobSeq : IStreamMessage
    {
        Container *out = nullptr;

        void deserialize(IStreamImpl &is, sofab_id_t id, size_t size, size_t) noexcept override
        {
            if (is.wire() != Wire::Fixlen || is.fixType() != Fix::Blob) return; /* §7.3 */
            if (static_cast<size_t>(id) >= out->capacity())
            {
                is.invalidate();
                return;
            }
            while (out->size() <= static_cast<size_t>(id)) out->emplace_back();
            auto &b = (*out)[id];
            b.set_len(size);
            if (size) is.read(b.data(), b.size());
        }
    };

    /*!
     * @brief Apply a collector's message-side bounds and report the verdict.
     *
     * The collectors below are the **static helper layer** (§6.6.1): they ship
     * here for reuse, the generated layer owns them, and no codec path calls
     * them — @ref IStreamImpl::readSequence hands control to the caller's
     * collector exactly as it hands control to any other handler. So a bound
     * applied here is generated code's bound, not the codec's, and §6.2.1's
     * "the codec never invents a limit of its own" is kept: every number below
     * arrives in a field the generated layer sets.
     *
     * A wrapper array is the one shape that has nowhere else to put it. It
     * announces no count and fires no callback per element, so the caller has no
     * point at which to check, and §6.2.1 puts the enforcement point on the
     * element **index**, before the container is extended.
     *
     * @param is           The stream, for the verdict.
     * @param n            The index + 1, or the element length.
     * @param schemaBound  The schema's `count` / `maxlen`, or -1. Exceeding it is
     *                     @ref Error::InvalidMessage.
     * @param dynCap       The §6.2.1 receiver cap. Consulted **only** where
     *                     @p schemaBound is -1. Exceeding it is
     *                     @ref Error::LimitExceeded; leaving it **unstated** there
     *                     is @ref Error::InvalidArgument, because §6.2.1 has no
     *                     unlimited mode for it to mean instead.
     * @return `true` when the element was refused.
     */
    [[nodiscard]] inline bool seqRefuse(
        IStreamImpl &is, size_t n, long schemaBound, DynCap dynCap) noexcept
    {
        // The collectors' spelling of IStreamImpl::refuseBound. One implementation
        // of the two message-side tiers, reached from both surfaces (§5.3.1).
        return is.refuseBound(n, schemaBound, dynCap);
    }

    /**
     * @brief Collects a `string` wrapper sequence into a `std::vector<std::string>`.
     *
     * The heap counterpart of @ref FixedStringSeq, for the `allow_dynamic`
     * storage mode: the schema's `count` and element `maxlen` still bind, they
     * just are not the container's capacity any more, so both are checked here.
     * An index at or past @ref cap is a schema-bound violation (§5.1/§7), as is
     * an element longer than @ref elemMax. Named to match `sofab::StringSeq` in
     * corelib-cpp so both C++ outputs read alike.
     *
     * Where the schema declares neither, @ref dynCap and @ref dynElemMax do — the
     * §6.2.1 receiver caps, set by generated code. This is the one collector that
     * has to apply them itself: a wrapper array announces no count and fires no
     * callback per element, so the caller has no point at which to check, and
     * §6.2.1 puts the enforcement point on the element **index**, before the
     * container is extended. Breaching one is @ref Error::LimitExceeded, never
     * `INVALID`: this is the growable profile's whole exposure, since the
     * container itself has no capacity to refuse with.
     */
    struct StringSeq : IStreamMessage
    {
        std::vector<std::string> *out = nullptr;
        long cap = -1;         //!< Schema `count` N, or -1 when the schema declares none.
        long elemMax = -1;     //!< Element `maxlen`, or -1 when the schema declares none.
        DynCap dynCap{};       //!< §6.2.1 receiver cap on the index; required where @ref cap is -1.
        DynCap dynElemMax{};   //!< §6.2.1 receiver cap on the element length; required where @ref elemMax is -1.

        void deserialize(IStreamImpl &is, sofab_id_t id, size_t size, size_t) noexcept override
        {
            if (is.wire() != Wire::Fixlen || is.fixType() != Fix::String)
            {
                return; /* §7.3 */
            }
            /* The index, decided from the id alone and before the container grows
             * -- which is what keeps an announced index near 2^31 from becoming an
             * allocation. A wrapper array's length is highest present id + 1
             * (MESSAGE_SPEC §5.1), so that is what the bound is applied to. */
            if (seqRefuse(is, static_cast<size_t>(id) + 1, cap, dynCap))
            {
                return;
            }
            if (seqRefuse(is, size, elemMax, dynElemMax))
            {
                return;
            }
            while (out->size() <= static_cast<size_t>(id)) out->emplace_back();
            auto &s = (*out)[id];
            s.assign(size, '\0');
            if (size) is.read(s);
        }
    };

    /*!
     * The `blob` counterpart of @ref StringSeq; same placement and bound rules,
     * filling `std::vector<std::uint8_t>` slots.
     */
    struct BlobSeq : IStreamMessage
    {
        std::vector<std::vector<uint8_t>> *out = nullptr;
        long cap = -1;         //!< Schema `count` N, or -1 when the schema declares none.
        long elemMax = -1;     //!< Element `maxlen`, or -1 when the schema declares none.
        DynCap dynCap{};       //!< §6.2.1 receiver cap on the index; required where @ref cap is -1.
        DynCap dynElemMax{};   //!< §6.2.1 receiver cap on the element length; required where @ref elemMax is -1.

        void deserialize(IStreamImpl &is, sofab_id_t id, size_t size, size_t) noexcept override
        {
            if (is.wire() != Wire::Fixlen || is.fixType() != Fix::Blob)
            {
                return; /* §7.3 */
            }
            if (seqRefuse(is, static_cast<size_t>(id) + 1, cap, dynCap))
            {
                return;
            }
            if (seqRefuse(is, size, elemMax, dynElemMax))
            {
                return;
            }
            while (out->size() <= static_cast<size_t>(id)) out->emplace_back();
            auto &b = (*out)[id];
            b.resize(size);
            if (size) is.read(b.data(), b.size());
        }
    };

    /**
     * @brief Narrow a fixed-count array to its non-default prefix, for encode.
     *
     * @deprecated **Superseded and non-conformant.** MESSAGE_SPEC §3 now defines
     * `count: N` as a **capacity** and the wire count `M` as the array's
     * **length**, so nothing may be elided: `[1,2,3,0,0]` and `[1,2,3]` are
     * different values, and trimming the tail silently shortens the array. There
     * is no fill-back on decode either. This helper is retained only so generated
     * code emitted before the change still compiles; new code must not call it,
     * and it goes away with the generator that emits it. (Its C counterpart,
     * `_array_trim_count` in object.c, is already gone.)
     *
     * The superseded contract it implements: a `count: N` array's canonical
     * encoding carries `M` = one past the last element that differs from the
     * element default, and the decoder refills `[M, N)`.
     *
     * Elements compare by **byte image**, never `operator==`: `-0.0 == 0.0` holds
     * in C++, but `-0.0` is a distinct value that must survive the round-trip, and
     * a NaN payload likewise never matches the default.
     *
     * @param a Contiguous container of trivially-copyable elements.
     * @return A span over `[0, M)`.
     */
    template <typename C>
    std::span<const typename C::value_type> trimTail(const C &a) noexcept
    {
        using Elem = typename C::value_type;
        const Elem zero{};
        size_t n = a.size();
        while (n > 0 && std::memcmp(&a[n - 1], &zero, sizeof(Elem)) == 0) --n;
        return std::span<const Elem>(a.data(), n);
    }
};

/** @} */ // end of defgroup

/* The wrapper-array collectors for object and row elements live in their own
 * header (the static-helper layer), but they are part of the same API: including
 * sofab.hpp gives you all six. seq.hpp includes this file back, which the
 * include guards resolve either way round. */
#include "sofab/seq.hpp"

#endif // SOFAB_HPP