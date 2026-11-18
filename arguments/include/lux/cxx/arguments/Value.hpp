#pragma once
// C++20. Value conversion and error vocabulary shared by both result strategies.
// No exceptions, RTTI, virtual interfaces or result allocations in the core.

#include <lux/cxx/arguments/detail/Attributes.hpp>

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <tuple>
#include <utility>

namespace lux::cxx::args
{

    using option_id = std::uint32_t;
    inline constexpr option_id no_option = std::numeric_limits<option_id>::max();
    inline constexpr std::size_t no_position = std::numeric_limits<std::size_t>::max();
    inline constexpr std::size_t unlimited = std::numeric_limits<std::size_t>::max();

    enum class EErrorCode : std::uint8_t
    {
        OK,
        INVALID_INTEGER,
        INVALID_FLOAT,
        INVALID_BOOLEAN,
        INVALID_VALUE,
        UNKNOWN_OPTION,
        MISSING_VALUE,
        MISSING_REQUIRED,
        DUPLICATE_OPTION,
        TOO_MANY_VALUES,
        UNEXPECTED_POSITIONAL,
        UNCLOSED_QUOTE,
        INVALID_ESCAPE,
        VALIDATION_FAILED,
        INVALID_SCHEMA,
        INVALID_INPUT,
        HELP_REQUESTED,
        INVALID_STRUCTURE,
        SIZE_MISMATCH,
        DUPLICATE_KEY,
        DEPTH_LIMIT,
        ELEMENT_LIMIT,
        CAPACITY_EXCEEDED
    };

    enum class ELocationKind : std::uint8_t
    {
        NONE,
        TEXT_OFFSET,
        ARGUMENT_INDEX
    };

    struct [[nodiscard]] Error
    {
        EErrorCode code = EErrorCode::OK;
        option_id option = no_option;
        std::size_t position = no_position;
        ELocationKind location = ELocationKind::NONE;
        // Offset in the decoded value, not necessarily an offset in the source text.
        std::size_t value_offset = no_position;
        constexpr explicit operator bool() const noexcept
        {
            return code != EErrorCode::OK;
        }
        friend constexpr bool operator==(const Error&, const Error&) = default;
    };

    // A conversion can return this richer status instead of errc. No string ownership.
    struct [[nodiscard]] ValueError
    {
        EErrorCode code = EErrorCode::OK;
        std::size_t offset = no_position;
        constexpr ValueError() noexcept = default;
        constexpr ValueError(EErrorCode c, std::size_t at = no_position) noexcept
            : code(c),
              offset(at)
        {
        }
        constexpr explicit operator bool() const noexcept
        {
            return code != EErrorCode::OK;
        }
        friend constexpr bool operator==(const ValueError&, const ValueError&) = default;
    };

    [[nodiscard]] constexpr std::string_view message(EErrorCode e) noexcept
    {
        switch (e)
        {
        case EErrorCode::OK:
            return "success";
        case EErrorCode::INVALID_INTEGER:
            return "invalid or out-of-range integer";
        case EErrorCode::INVALID_FLOAT:
            return "invalid or out-of-range floating-point value";
        case EErrorCode::INVALID_BOOLEAN:
            return "invalid boolean";
        case EErrorCode::INVALID_VALUE:
            return "invalid value";
        case EErrorCode::UNKNOWN_OPTION:
            return "unknown option";
        case EErrorCode::MISSING_VALUE:
            return "missing option value";
        case EErrorCode::MISSING_REQUIRED:
            return "required option absent";
        case EErrorCode::DUPLICATE_OPTION:
            return "duplicate option";
        case EErrorCode::TOO_MANY_VALUES:
            return "too many values";
        case EErrorCode::UNEXPECTED_POSITIONAL:
            return "unexpected positional argument";
        case EErrorCode::UNCLOSED_QUOTE:
            return "unclosed quotation";
        case EErrorCode::INVALID_ESCAPE:
            return "incomplete escape";
        case EErrorCode::VALIDATION_FAILED:
            return "validation failed";
        case EErrorCode::INVALID_SCHEMA:
            return "invalid option declaration";
        case EErrorCode::INVALID_INPUT:
            return "invalid input";
        case EErrorCode::HELP_REQUESTED:
            return "help requested";
        case EErrorCode::INVALID_STRUCTURE:
            return "invalid structured value";
        case EErrorCode::SIZE_MISMATCH:
            return "fixed-size value has the wrong number of elements";
        case EErrorCode::DUPLICATE_KEY:
            return "duplicate map key";
        case EErrorCode::DEPTH_LIMIT:
            return "structured nesting limit exceeded";
        case EErrorCode::ELEMENT_LIMIT:
            return "structured element limit exceeded";
        case EErrorCode::CAPACITY_EXCEEDED:
            return "container capacity exceeded";
        }
        return "unknown error";
    }

    // Ownership is deliberate: a Result cannot retain a view into temporary token
    // storage. Specialize for custom view-like types; the direct mode also rejects
    // borrowed outputs to keep its contract simple and independent of input lifetime.
    template<class T>
    struct SequenceTraits;
    template<class T>
    struct MapTraits;
    template<class T>
    struct TupleTraits;

    template<class T, class = void>
    struct BorrowsInput : std::false_type
    {
    };
    template<class C, class Tr>
    struct BorrowsInput<std::basic_string_view<C, Tr>> : std::true_type
    {
    };
    template<class T>
    struct BorrowsInput<T*> : std::true_type
    {
    };
    template<class T>
    struct BorrowsInput<std::optional<T>> : BorrowsInput<T>
    {
    };

    // Extend with value_parser<MyType> { static errc parse(string_view, MyType&); }.
    template<class T, class = void>
    struct ValueParser;

    template<std::integral T>
        requires(!std::same_as<T, bool>)
    struct ValueParser<T>
    {
        static EErrorCode parse(std::string_view s, T& out) noexcept
        {
            T value{};
            if (s.empty())
                return EErrorCode::INVALID_INTEGER;
            // from_chars does not accept a leading '+' for integers.
            const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
            if (ec != std::errc{} || p != s.data() + s.size())
                return EErrorCode::INVALID_INTEGER;
            out = value;
            return EErrorCode::OK;
        }
    };

    template<std::floating_point T>
    struct ValueParser<T>
    {
        static EErrorCode parse(std::string_view s, T& out) noexcept
        {
            T value{};
            if (s.empty())
                return EErrorCode::INVALID_FLOAT;
            const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
            if (ec != std::errc{} || p != s.data() + s.size())
                return EErrorCode::INVALID_FLOAT;
            out = value;
            return EErrorCode::OK;
        }
    };

    template<>
    struct ValueParser<bool>
    {
        static EErrorCode parse(std::string_view s, bool& out) noexcept
        {
            if (s == "1" || s == "true" || s == "yes" || s == "on")
                out = true;
            else if (s == "0" || s == "false" || s == "no" || s == "off")
                out = false;
            else
                return EErrorCode::INVALID_BOOLEAN;
            return EErrorCode::OK;
        }
    };

    template<class Traits, class Allocator>
    struct ValueParser<std::basic_string<char, Traits, Allocator>>
    {
        static constexpr bool builtin_string = true;
        static EErrorCode parse(std::string_view s, std::basic_string<char, Traits, Allocator>& out)
        {
            out.assign(s.data(), s.size());
            return EErrorCode::OK;
        }
    };

    template<class T>
        requires std::is_enum_v<T>
    struct ValueParser<T>
    {
        using underlying_type = std::underlying_type_t<T>;
        static ValueError parse(std::string_view s, T& out) noexcept(
            noexcept(ValueParser<underlying_type>::parse(s, std::declval<underlying_type&>()))
        )
        {
            underlying_type v{};
            if (ValueError e = ValueParser<underlying_type>::parse(s, v); e)
                return e;
            out = static_cast<T>(v);
            return {};
        }
    };

    template<class T>
    struct ValueParser<std::optional<T>>
    {
        static constexpr bool builtin_optional = true;
        static constexpr bool structured = [] {
            if constexpr (requires { ValueParser<T>::structured; })
                return bool(ValueParser<T>::structured);
            else
                return false;
        }();
        static ValueError parse(std::string_view s, std::optional<T>& out)
        {
            if (s == "null")
            {
                out.reset();
                return {};
            }
            if (!out)
                out.emplace();
            if (ValueError e = ValueParser<T>::parse(s, *out); e)
                return e;
            return EErrorCode::OK;
        }
    };

    namespace detail
    {
        // Traits are opt-in; looking like a container never changes scalar semantics.
        template<class T>
        concept sequence = requires {
            typename SequenceTraits<T>::value_type;
            SequenceTraits<T>::extent;
        };
        template<class T>
        concept appendable_sequence =
            sequence<T> && requires(T& out, typename SequenceTraits<T>::value_type v) {
                SequenceTraits<T>::clear(out);
                { SequenceTraits<T>::append(out, std::move(v)) } -> std::same_as<EErrorCode>;
            };
        template<class T, bool Multiple>
        struct Element
        {
            using type = T;
        };
        template<class T>
        struct Element<T, true>
        {
            using type = typename SequenceTraits<T>::value_type;
        };
        template<class R>
        concept conversion_status = std::same_as<R, EErrorCode> || std::same_as<R, ValueError>;
        template<class C, class T>
        concept converter_for = requires(const C& c, std::string_view s, T& v) {
            { c(s, v) } -> conversion_status;
        };
        template<class C, class T>
        concept validator_for = requires(const C& c, const T& v) {
            { c(v) } -> std::convertible_to<bool>;
        };

        struct DefaultConverter
        {
            template<class T>
                requires requires(std::string_view s, T& out) {
                    { ValueParser<T>::parse(s, out) } -> conversion_status;
                }
            auto operator()(std::string_view s, T& out) const
                noexcept(noexcept(ValueParser<T>::parse(s, out)))
            {
                return ValueParser<T>::parse(s, out);
            }
        };
        template<class T>
        inline constexpr bool structured_parser = [] {
            if constexpr (requires { ValueParser<T>::structured; })
                return bool(ValueParser<T>::structured);
            else
                return false;
        }();
        template<class C, class T>
        inline constexpr bool structured_conversion = [] {
            if constexpr (requires { C::structured; })
                return bool(C::structured);
            else if constexpr (std::same_as<C, DefaultConverter>)
                return structured_parser<T>;
            else
                return false;
        }();
        struct AcceptAll
        {
            template<class T>
            constexpr bool operator()(const T&) const noexcept
            {
                return true;
            }
        };
        template<class First, class Second>
        struct AllOf
        {
            LUX_CXX_ARGS_NO_UNIQUE_ADDRESS First first;
            LUX_CXX_ARGS_NO_UNIQUE_ADDRESS Second second;
            template<class T>
            constexpr bool operator()(const T& v) const
            {
                return first(v) && second(v);
            }
        };

        [[nodiscard]] constexpr bool space(char c) noexcept
        {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
        }
        [[nodiscard]] constexpr bool negativeNumber(std::string_view s) noexcept
        {
            return s.size() > 1 && s[0] == '-' &&
                   ((s[1] >= '0' && s[1] <= '9') ||
                    (s[1] == '.' && s.size() > 2 && s[2] >= '0' && s[2] <= '9') || s == "-inf" ||
                    s == "-infinity" || s == "-nan");
        }
        [[nodiscard]] constexpr bool validName(std::string_view s) noexcept
        {
            if (s.empty() || s.front() == '-')
                return false;
            for (char c : s)
            {
                const auto uc = static_cast<unsigned char>(c);
                if (uc <= 32 || uc >= 127 || c == '=' || c == '"' || c == '\'' || c == '\\')
                    return false;
            }
            return true;
        }
        [[nodiscard]] constexpr std::uint64_t hash(std::string_view s) noexcept
        {
            std::uint64_t h = 14695981039346656037ull;
            for (char ch : s)
            {
                const auto c = static_cast<unsigned char>(ch);
                h = (h ^ c) * 1099511628211ull;
            }
            return h;
        }
    } // namespace detail

    template<class T>
    struct InRangeCheck
    {
        T min, max;
        template<class U>
        constexpr bool operator()(const U& v) const
        {
            return v >= min && v <= max;
        }
    };
    template<class T>
    [[nodiscard]] constexpr auto inRange(T a, T b)
    {
        return InRangeCheck<T>{a, b};
    }

    template<class... T>
    struct OneOfCheck
    {
        std::tuple<T...> values;
        template<class U>
        constexpr bool operator()(const U& value) const
        {
            return std::apply([&](const auto&... v) { return ((value == v) || ...); }, values);
        }
    };
    template<class... T>
    [[nodiscard]] constexpr auto oneOf(T... values)
    {
        return OneOfCheck<T...>{{std::move(values)...}};
    }

} // namespace lux::cxx::args
