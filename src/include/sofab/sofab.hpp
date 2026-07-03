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
 *   Structural capabilities (FIXLEN, SEQUENCE) underpin concrete methods and
 *   the whole nested-message API (strings, blobs, floats, sequences, message
 *   objects) — i.e. most of the C++ surface. The wrapper cannot offer a
 *   coherent object API without them, so building the C++ layer with either
 *   disabled is rejected outright; use the C API directly for such configs.
 */
#if defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
#  error "sofab C++ wrapper requires FIXLEN support (strings, blobs, floats). Do not define SOFAB_DISABLE_FIXLEN_SUPPORT when building the C++ API; use the C API directly for fixlen-less builds."
#endif
#if defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
#  error "sofab C++ wrapper requires SEQUENCE support (nested messages, variable-length array reads). Do not define SOFAB_DISABLE_SEQUENCE_SUPPORT when building the C++ API; use the C API directly for sequence-less builds."
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
        None = SOFAB_RET_OK,                    //!< Success.
        UsageError = SOFAB_RET_E_USAGE,         //!< Invalid usage (e.g. type mismatch on read).
        BufferFull = SOFAB_RET_E_BUFFER_FULL,   //!< Output buffer overflowed during encoding.
        InvalidArgument = SOFAB_RET_E_ARGUMENT, //!< Invalid argument (e.g. field id out of range).
        InvalidMessage = SOFAB_RET_E_INVALID_MSG//!< Malformed message encountered while decoding.
    };


    /*! @brief Field identifier type (alias of @ref sofab_id_t). */
    using id = sofab_id_t;

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
             * @brief Chained sequence-begin marker (no-op if a prior call failed).
             * @param id  Field identifier of the nested sequence.
             * @return This Result, carrying the first error encountered (if any).
             */
            Result sequenceBegin(sofab_id_t id) noexcept
            {
                if (error_ != Error::None)
                {
                    return *this;
                }

                auto res = ostream_.sequenceBegin(id);
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
                std::string_view sv{value};
                ret = sofab_ostream_write_string(&ctx_, id, sv.data());
            }
            else if constexpr (std::is_base_of_v<OStreamMessage, T>)
            {
                ret = sequenceBegin(id).rawCode();
                if (ret == SOFAB_RET_OK)
                {
                    ret = value.serialize(static_cast<OStreamImpl&>(*this)).rawCode();
                    if (ret == SOFAB_RET_OK)
                    {
                        ret = sequenceEnd().rawCode();
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
         * @brief Open a nested sequence; subsequent writes use a fresh id scope
         *        until the matching @ref sequenceEnd.
         * @param id  Field identifier of the sequence.
         * @return @ref Result for fluent chaining and error inspection.
         */
        Result sequenceBegin(sofab_id_t id) noexcept
        {
            return result(sofab_ostream_write_sequence_begin(&ctx_, id));
        }

        /*!
         * @brief Close the most recently opened nested sequence.
         * @return @ref Result for fluent chaining and error inspection.
         */
        Result sequenceEnd() noexcept
        {
            return result(sofab_ostream_write_sequence_end(&ctx_));
        }

    private:
        /*! @brief Wrap a C return code in a @ref Result bound to this stream. */
        inline Result result(sofab_ret_t ret) noexcept
        {
            return Result{*this, ret};
        }
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

    class OStreamMessage;
    /*! @brief Concept: a serializable message type usable with @ref OStreamObject
     *  (derives from @ref OStreamMessage and exposes a @c _maxSize constant). */
    template <class T>
    concept OutputMessage =
        std::derived_from<T, OStreamMessage> &&
        requires {
            { T::_maxSize } -> std::convertible_to<std::size_t>;
        } &&
        std::is_same_v<decltype(T::_maxSize), const std::size_t>;

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
     * sized from the message's @c _maxSize, so a message can be populated and
     * encoded in one object.
     *
     * @tparam MessageType  An @ref OutputMessage type.
     * @tparam N            Buffer size in bytes (defaults to @c MessageType::_maxSize).
     * @tparam Offset       Initial write offset within the buffer.
     */
    template <OutputMessage MessageType, size_t N = MessageType::_maxSize, size_t Offset = 0>
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
     * Convenience alias-like class fixing @c N to @c MessageType::_maxSize while
     * letting the caller choose a leading @p Offset (e.g. to reserve a header).
     *
     * @tparam MessageType  An @ref OutputMessage type.
     * @tparam Offset       Initial write offset within the buffer.
     */
    template <OutputMessage MessageType, size_t Offset = 0>
    class OStreamObjectOffset : public OStreamObject<MessageType, MessageType::_maxSize, Offset>
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

        IStreamImpl() noexcept = default;

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
            /*! @brief True if decoding succeeded (same as ok()). */
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

            /*! @brief True if no decode error occurred. */
            bool ok() const noexcept
            {
                return error_ == Error::None;
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
         * @param buffer  Pointer to the bytes to decode.
         * @param buflen  Number of bytes at @p buffer.
         * @return @ref Result indicating success or a decode error.
         */
        Result feed(const uint8_t *buffer, size_t buflen) noexcept
        {
            return Result{sofab_istream_feed(&ctx_, buffer, buflen)};
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
};

/** @} */ // end of defgroup

#endif // SOFAB_HPP