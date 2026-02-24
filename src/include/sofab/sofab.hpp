/*!
 * @file sofab.hpp
 * @brief SofaBuffers C++ - Input and output stream decoder for Sofab messages.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _SOFAB_HPP
#define _SOFAB_HPP

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

#include "sofab/istream.h"
#include "sofab/ostream.h"

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
                ret = sofab_ostream_write_fp64(&ctx_, id, value);
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
                using Elem = typename T::value_type;
                std::span<const Elem> span{value};

                if constexpr (std::is_integral_v<Elem> && !std::is_same_v<Elem, bool>)
                {
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
                    ret = sofab_ostream_write_array_of_fp64(
                        &ctx_, id,
                        span.data(),
                        static_cast<int32_t>(span.size()));
                }
                else
                {
                    static_assert(always_false_v<T>,
                        "Unsupported span element type in OStream::write()");
                }
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

    class IStreamImpl
    {
    protected:
        sofab_istream_t ctx_;

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
                    sofab_istream_read_fp64(&ctx_, &value);
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    // std::string doesn't need a null terminator, so we use read_noterm
                    sofab_istream_read_string_noterm(&ctx_, value.data(), value.size());
                }
                else if constexpr (
                    requires {
                        typename T::value_type;
                        std::span{ std::declval<const T&>() };
                    } &&
                    !std::is_const_v<typename T::value_type>)
                {
                    using Elem = typename T::value_type;
                    std::span<Elem> span{value};

                    if constexpr (std::is_integral_v<Elem> && !std::is_same_v<Elem, bool>)
                    {
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
                        sofab_istream_read_array_of_fp64(
                            &ctx_,
                            span.data(),
                            static_cast<int32_t>(span.size()));
                    }
                    else
                    {
                        static_assert(always_false_v<T>,
                            "Unsupported span element type in IStream::read()");
                    }
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

    class IStreamMessage;
    template <typename T>
    concept InputMessage = std::derived_from<T, IStreamMessage>;

    class IStreamMessage
    {
    private:
        sofab_istream_decoder_t decoder_;

        template <InputMessage MessageType>
        friend class IStreamObject;

        struct Context
        {
            IStreamImpl &istream;
            IStreamMessage &message;
        };

        static void field_callback_(
            sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
        {
            (void)ctx;

            auto context = static_cast<Context*>(usrptr);
            context->message.deserialize(context->istream, id, size, count);
        }

    public:
        virtual void deserialize(sofab::IStreamImpl &_istream, sofab::id _id, size_t _size, size_t _count) noexcept = 0;
    };

    template <InputMessage MessageType>
    class IStreamObject : public IStreamImpl
    {
        MessageType data_;
        IStreamMessage::Context context_;

    public:
        IStreamObject() noexcept
            : context_{*this, data_}
        {
            sofab_istream_init(&ctx_, MessageType::field_callback_, &context_);
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

#endif // _SOFAB_HPP