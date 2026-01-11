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

    using flushCallback = std::function<void(std::span<const uint8_t>)>;

    using id = sofab_id_t;

    class OStreamMessage;
    class OStream
    {
    protected:
        sofab_ostream_t ctx_;
        std::shared_ptr<uint8_t[]> buffer_;
        flushCallback flushCallback_;

        void onFlushCallback(size_t len) noexcept
        {
            if (flushCallback_)
            {
                flushCallback_(std::span<const uint8_t>(data(), len));
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

            OStream *self = static_cast<OStream*>(usrptr);
            self->onFlushCallback(len);
        }

        OStream() noexcept = default;

    public:
        class Result
        {
        private:
            OStream &ostream_;
            Error error_ = Error::None;

            friend class OStream;
            Result(OStream &ostream, sofab_ret_t retval)
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
        OStream(const OStream&) = delete;
        OStream& operator=(const OStream&) = delete;

        // allow moving
        OStream(OStream&&) noexcept = default;
        OStream& operator=(OStream&&) noexcept = default;

        OStream(size_t buflen, size_t offset = 0) noexcept
        {
            buffer_ = std::make_shared<uint8_t[]>(buflen);
            sofab_ostream_init(&ctx_, buffer_.get(), buflen, offset, nullptr, nullptr);
        }

        OStream(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
            : buffer_{buffer}
        {
            sofab_ostream_init(&ctx_, buffer_.get(), buflen, offset, nullptr, nullptr);
        }

        OStream(
            flushCallback callback,
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
            : buffer_{buffer}
            , flushCallback_{callback}
        {
            sofab_ostream_init(&ctx_, buffer_.get(), buflen, offset, static_flush_callback, this);
        }

        virtual ~OStream() noexcept
        {
            flush();
        }

        // set buffer in OStreamInline blöd!!!!
        void setBuffer(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
        {
            buffer_ = buffer;
            sofab_ostream_buffer_set(&ctx_, buffer.get(), buflen, offset);
        }

        // auch blöd!!!!
        std::shared_ptr<uint8_t[]> getBuffer() noexcept
        {
            return buffer_;
        }

        size_t flush() noexcept
        {
            return sofab_ostream_flush(&ctx_);
        }

        size_t bytesUsed() noexcept
        {
            return sofab_ostream_bytes_used(&ctx_);
        }

        virtual const uint8_t*
        data() const noexcept
        {
            return buffer_.get();
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
                    ret = value.serialize(static_cast<OStream&>(*this)).rawCode();
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

#if 0
    class OStreamExternal : public OStream
    {
    private:
        std::shared_ptr<uint8_t[]> buffer_;

    public:
        OStreamExternal(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
        {
            sofab_ostream_init(&ctx_, buffer.get(), buflen, offset, nullptr, nullptr);
        }

        OStreamExternal(
            flushCallback callback,
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
            : buffer_{buffer}
            , flushCallback_{callback}
        {
            sofab_ostream_init(&ctx_, buffer_.get(), buflen, offset, _flush_callback, this);
        }

        void setBuffer(
            std::shared_ptr<uint8_t[]> buffer, size_t buflen,
            size_t offset = 0) noexcept
        {
            sofab_ostream_buffer_set(&ctx_, buffer.get(), buflen, offset);
        }

        const uint8_t* data() const noexcept
        {
            return buffer_.get();
        }
    };
#endif

    template <size_t N, size_t Offset = 0>
    class OStreamInline : public OStream
    {
        static_assert(N > 0, "Buffer size N must be greater than zero");
        static_assert(Offset < N, "Offset must be less than buffer size N");
        std::array<uint8_t, N> buffer_ = {};

    public:
        OStreamInline() noexcept
        {
            sofab_ostream_init(&ctx_, buffer_.data(), buffer_.size(), Offset, nullptr, nullptr);
        }

        OStreamInline(flushCallback callback) noexcept
        {
            flushCallback_ = callback;
            sofab_ostream_init(&ctx_, buffer_.data(), buffer_.size(), Offset, static_flush_callback, this);
        }

        const uint8_t* data() const noexcept
        {
            return buffer_.data();
        }

        size_t size() const noexcept
        {
            return buffer_.size();
        }
    };

    class OStreamMessage;
    template <class T>
    concept HasConstexprMaxSize =
        std::derived_from<T, OStreamMessage> &&
        requires {
            { T::_maxSize } -> std::convertible_to<std::size_t>;
        } &&
        std::is_same_v<decltype(T::_maxSize), const std::size_t>;

    class OStreamMessage
    {
    protected:
        virtual OStream::Result
        _serialize(OStream &_ostream) const noexcept = 0;
    };

    template <HasConstexprMaxSize MessageType, size_t N = MessageType::_maxSize, size_t Offset = 0>
    class OStreamObject : public OStreamInline<N + Offset, Offset>
    {
        MessageType message_;

    public:
        OStreamObject() noexcept = default;
        OStreamObject(flushCallback callback) noexcept
            : OStreamInline<N + Offset, Offset>{callback}
        { };

        MessageType* operator->() noexcept
        {
            return &message_;
        }

        OStream::Result serialize() noexcept
        {
            auto result =  message_._serialize(static_cast<OStream&>(*this));
            OStream::flush();

            return result;
        }
    };

    /// ist das clever ???s
    template <HasConstexprMaxSize MessageType, size_t Offset = 0>
    class OStreamObjectOffset : public OStreamObject<MessageType, MessageType::_maxSize, Offset>
    {
    };

    /* IStream */

    class IStream
    {
    protected:
        sofab_istream_t ctx_;

        IStream() noexcept = default;

    public:
        class Result
        {
        private:
            Error error_ = Error::None;

            friend class IStream;
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
        IStream(const IStream&) = delete;
        IStream& operator=(const IStream&) = delete;

        // allow moving
        IStream(IStream&&) noexcept = default;
        IStream& operator=(IStream&&) noexcept = default;

        Result feed(const uint8_t *buffer, size_t buflen) noexcept
        {
            return Result(sofab_istream_feed(&ctx_, buffer, buflen));
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

    class IStreamInline : public IStream
    {
        using fieldCallback = std::function<void(IStream& _istream, sofab::id _id, size_t _size)>;

    private:
        fieldCallback callback_;

        static void _field_callback(
            sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usrptr)
        {
            (void)ctx;
            (void)count;

            auto *self = static_cast<IStreamInline*>(usrptr);
            self->callback_(*self, id, size);
        }

    public:
        IStreamInline(fieldCallback callback) noexcept
            : callback_{callback}
        {
            sofab_istream_init(&ctx_, _field_callback, this);
        }
    };

    class IStreamMessage
    {
    private:
        sofab_decoder_t decoder_;

        template <class MessageType>
        friend class IStreamObject;

        struct Context
        {
            IStream &istream;
            IStreamMessage &message;
        };

        static void _field_callback(
            sofab_istream_t *ctx, sofab_id_t id, size_t size, void *usrptr)
        {
            (void)ctx;

            auto context = static_cast<Context*>(usrptr);
            context->message._onFieldCallback(context->istream, id, size);
        }

    public:
        virtual void _onFieldCallback(sofab::IStream &_istream, sofab::id _id, size_t _size) noexcept = 0;
    };

    template <class MessageType>
    class IStreamObject : public IStream
    {
        MessageType data_;
        IStreamMessage::Context context_;

    public:
        IStreamObject() noexcept
            : context_{*this, data_}
        {
            sofab_istream_init(&ctx_, MessageType::_field_callback, &context_);
        }

        MessageType* operator->() noexcept
        {
            return &data_;
        }
    };
};

/** @} */ // end of defgroup

#endif // _SOFAB_HPP