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

#if defined(SOFAB_DISABLE_FP64_SUPPORT)
#  define SOFAB_CPP_HAVE_FP64 0
#else
#  define SOFAB_CPP_HAVE_FP64 1
#endif
#if defined(SOFAB_DISABLE_INT64_SUPPORT)
#  define SOFAB_CPP_HAVE_INT64 0
#else
#  define SOFAB_CPP_HAVE_INT64 1
#endif
#if defined(SOFAB_DISABLE_ARRAY_SUPPORT)
#  define SOFAB_CPP_HAVE_ARRAY 0
#else
#  define SOFAB_CPP_HAVE_ARRAY 1
#endif

/* types **********************************************************************/
namespace sofab
{
    inline constexpr int API_VERSION = 1;

    template <typename>
    inline constexpr bool always_false_v = false;

    enum class Error
    {
        None = SOFAB_RET_OK,
        UsageError = SOFAB_RET_E_USAGE,
        BufferFull = SOFAB_RET_E_BUFFER_FULL,
        InvalidArgument = SOFAB_RET_E_ARGUMENT,
        InvalidMessage = SOFAB_RET_E_INVALID_MSG
    };


    using id = sofab_id_t;

    /***************/
    /*** OStream ***/
    /***************/

    class OStreamMessage;
    class OStreamImpl
    {
    public:
        using flushCallback = std::function<void(std::span<const uint8_t>)>;

    protected:
        sofab_ostream_t ctx_;
        uint8_t *buffer_;
        flushCallback flushCallback_;

        void onFlushCallback(size_t len) noexcept
        {
            if (flushCallback_)
            {
                flushCallback_(std::span<const uint8_t>(buffer_, len));
            }
        }

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

            explicit operator bool() const noexcept
            {
                return ok();
            }

            bool operator==(Error e) const noexcept
            {
                return error_ == e;
            }

            bool operator!=(Error e) const noexcept
            {
                return !(*this == e);
            }

            bool ok() const noexcept
            {
                return error_ == Error::None;
            }

            Error code() const noexcept
            {
                return error_;
            }
        };

        // disable copying due to pointers in ctx_
        OStreamImpl(const OStreamImpl&) = delete;
        OStreamImpl& operator=(const OStreamImpl&) = delete;

        // allow moving
        OStreamImpl(OStreamImpl&&) noexcept = default;
        OStreamImpl& operator=(OStreamImpl&&) noexcept = default;

        virtual ~OStreamImpl() noexcept
        {
            flush();
        }

        size_t flush() noexcept
        {
            return sofab_ostream_flush(&ctx_);
        }

        size_t bytesUsed() noexcept
        {
            return sofab_ostream_bytes_used(&ctx_);
        }

        const uint8_t* data() const noexcept
        {
            return buffer_;
        }

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

        Result write(sofab_id_t id, const void *value, int32_t size) noexcept
        {
            return result(sofab_ostream_write_blob(&ctx_, id, value, size));
        }

        template <typename T>
        Result writeIf(sofab_id_t id, const T &value, bool condition) noexcept
        {
            if (condition)
            {
                return write(id, value);
            }

            return result(SOFAB_RET_OK);
        }

        Result sequenceBegin(sofab_id_t id) noexcept
        {
            return result(sofab_ostream_write_sequence_begin(&ctx_, id));
        }

        Result sequenceEnd() noexcept
        {
            return result(sofab_ostream_write_sequence_end(&ctx_));
        }

    private:
        inline Result result(sofab_ret_t ret) noexcept
        {
            return Result{*this, ret};
        }
    };

    class OStream : public OStreamImpl
    {
    protected:
        std::shared_ptr<uint8_t[]> bufferOwner_;

        OStream() noexcept = default;

    public:
        OStream(size_t buflen, size_t offset = 0) noexcept
        {
            bufferOwner_ = std::make_shared<uint8_t[]>(buflen);
            buffer_ = bufferOwner_.get();
            sofab_ostream_init(&ctx_, buffer_, buflen, offset, nullptr, nullptr);
        }

        OStream(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
            : bufferOwner_{buffer}
        {
            buffer_ = bufferOwner_.get();
            sofab_ostream_init(&ctx_, buffer_, buflen, offset, nullptr, nullptr);
        }

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

        void setBuffer(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
        {
            bufferOwner_ = buffer;
            buffer_ = bufferOwner_.get();
            sofab_ostream_buffer_set(&ctx_, buffer_, buflen, offset);
        }

        std::shared_ptr<uint8_t[]> getBuffer() noexcept
        {
            return bufferOwner_;
        }
    };

    template <size_t N, size_t Offset = 0>
    class OStreamInline : public OStreamImpl
    {
        static_assert(N > 0, "Buffer size N must be greater than zero");
        static_assert(Offset < N, "Offset must be less than buffer size N");
        std::array<uint8_t, N> bufferOwner_ = {};

    public:
        OStreamInline() noexcept
        {
            buffer_ = bufferOwner_.data();
            sofab_ostream_init(&ctx_, buffer_, N, Offset, nullptr, nullptr);
        }

        OStreamInline(flushCallback callback) noexcept
        {
            buffer_ = bufferOwner_.data();
            flushCallback_ = callback;
            sofab_ostream_init(&ctx_, buffer_, N, Offset, static_flush_callback, this);
        }
    };

    class OStreamMessage;
    template <class T>
    concept OutputMessage =
        std::derived_from<T, OStreamMessage> &&
        requires {
            { T::_maxSize } -> std::convertible_to<std::size_t>;
        } &&
        std::is_same_v<decltype(T::_maxSize), const std::size_t>;

    class OStreamMessage
    {
    protected:
        virtual OStream::Result
        serialize(OStreamImpl &_ostream) const noexcept = 0;
    };

    template <OutputMessage MessageType, size_t N = MessageType::_maxSize, size_t Offset = 0>
    class OStreamObject : public OStreamInline<N + Offset, Offset>
    {
        MessageType message_;

    public:
        OStreamObject() noexcept = default;
        OStreamObject(typename OStream::flushCallback callback) noexcept
            : OStreamInline<N + Offset, Offset>{callback}
        { };

        MessageType& operator->() noexcept
        {
            return message_;
        }

        OStream::Result serialize() noexcept
        {
            auto result =  message_.serialize(static_cast<OStreamImpl&>(*this));
            OStreamImpl::flush();

            return result;
        }
    };

    template <OutputMessage MessageType, size_t Offset = 0>
    class OStreamObjectOffset : public OStreamObject<MessageType, MessageType::_maxSize, Offset>
    {
    };


    /***************/
    /*** IStream ***/
    /***************/

    class IStreamMessage;
    template <typename T>
    concept InputMessage = std::derived_from<T, IStreamMessage>;

    class IStreamImpl
    {
    protected:
        sofab_istream_t ctx_;

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
            explicit operator bool() const noexcept
            {
                return ok();
            }

            bool operator==(Error e) const noexcept
            {
                return error_ == e;
            }

            bool operator!=(Error e) const noexcept
            {
                return !(*this == e);
            }

            bool ok() const noexcept
            {
                return error_ == Error::None;
            }

            Error code() const noexcept
            {
                return error_;
            }
        };

        // disable copying due to pointers in ctx_
        IStreamImpl(const IStreamImpl&) = delete;
        IStreamImpl& operator=(const IStreamImpl&) = delete;

        // allow moving
        IStreamImpl(IStreamImpl&&) noexcept = default;
        IStreamImpl& operator=(IStreamImpl&&) noexcept = default;

        Result feed(const uint8_t *buffer, size_t buflen) noexcept
        {
            return Result{sofab_istream_feed(&ctx_, buffer, buflen)};
        }

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

        // Decode a blob field into a caller-owned, address-stable buffer.
        //
        // The templated read() above routes std::string to a STRING-tagged read
        // and a std::vector<uint8_t> span to a VARINTARRAY-tagged read; neither
        // matches a BLOB field, so the C type check rejects them and the bytes are
        // dropped. This overload binds with the BLOB tag (via read_blob) so blob
        // fields decode correctly. The caller must size the destination to the
        // field length (provided as `size` in the field callback); the bytes are
        // filled by a subsequent feed() pass into this buffer, which must stay
        // alive and unmoved until decoding of the field completes.
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

        // Decode a sequence of variable-length string elements into a vector.
        //
        // Each element is emplaced into `out` and the C read target is bound to
        // that persistent slot, so feed() fills it in place before the next
        // element's callback fires. A later emplace_back may reallocate `out`, but
        // it only moves already-filled elements (heap-stable, or SSO bytes copied
        // intact), never an unfilled bound target. This is the safe counterpart to
        // the transient read-into-local-then-move pattern, which dangles under the
        // deferred decoder.
        void read(std::vector<std::string> &out) noexcept
        {
            sofab_istream_read_sequence(
                &ctx_, &arrayDecoder_, &strArrayElem_, &out);
        }

        // Decode a sequence of variable-length blob elements into a vector.
        void read(std::vector<std::vector<uint8_t>> &out) noexcept
        {
            sofab_istream_read_sequence(
                &ctx_, &arrayDecoder_, &blobArrayElem_, &out);
        }

    private:
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

    class IStreamInline : public IStreamImpl
    {
    public:
        using fieldCallback = std::function<void(sofab::id _id, size_t _size, size_t _count)>;

    private:
        fieldCallback callback_;

        static void field_callback_(
            sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
        {
            (void)ctx;

            auto *self = static_cast<IStreamInline*>(usrptr);
            self->callback_(id, size, count);
        }

    public:
        IStreamInline(fieldCallback callback) noexcept
            : callback_{callback}
        {
            sofab_istream_init(&ctx_, field_callback_, this);
        }
    };

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
        virtual void deserialize(sofab::IStreamImpl &_istream, sofab::id _id, size_t _size, size_t _count) noexcept = 0;
    };

    template <InputMessage MessageType>
    class IStreamObject : public IStreamImpl
    {
        MessageType data_;

    public:
        IStreamObject() noexcept
        {
            data_.context_ = IStreamMessage::Context{this, &data_};
            sofab_istream_init(&ctx_, MessageType::field_callback_, &data_.context_);
        }

        MessageType& operator->() noexcept
        {
            return data_;
        }

        const MessageType& operator->() const noexcept
        {
            return data_;
        }

        MessageType& operator*() noexcept
        {
            return data_;
        }

        const MessageType& operator*() const noexcept
        {
            return data_;
        }
    };
};

/** @} */ // end of defgroup

#endif // SOFAB_HPP