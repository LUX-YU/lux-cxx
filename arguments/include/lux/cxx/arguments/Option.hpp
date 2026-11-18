#pragma once
// A description of a typed option, independent of result storage or target objects.
// Names, element mode, converters, validators and the presence of a default are
// type information. Merely declaring Option<LargeType> does NOT store LargeType.
#include <lux/cxx/arguments/Value.hpp>
#include <lux/cxx/compile_time/fixed_string.hpp>
#include <variant>

namespace lux::cxx::args
{

    enum class ERepeat : std::uint8_t
    {
        AUTOMATIC,
        REJECT,
        FIRST,
        LAST,
        APPEND
    };
    struct OptionSettings
    {
        bool required = false;
        bool positional = false;
        std::size_t min_values = 1;
        std::size_t max_values = unlimited;
        char delimiter = 0;
        ERepeat repeats = ERepeat::AUTOMATIC;
    };
    namespace detail
    {
        struct NoDefault
        {
        };
        template<class T>
        struct StoredDefault
        {
            T value;
        };
    } // namespace detail

    template<
        class T, ::lux::cxx::fixed_string Name = ::lux::cxx::fixed_string{""},
        ::lux::cxx::fixed_string Short = ::lux::cxx::fixed_string{""},
        class Converter = detail::DefaultConverter, class Validator = detail::AcceptAll,
        bool Multiple = false, class Default = detail::NoDefault>
    class Option
    {
        static constexpr bool static_named_ = Name.size() != 0;
        using names_storage =
            std::conditional_t<static_named_, std::monostate, std::pair<std::string, std::string>>;

    public:
        using value_type = T;
        using element_type = typename detail::Element<T, Multiple>::type;
        using converter_type = Converter;
        using validator_type = Validator;
        static constexpr auto static_name = Name;
        static constexpr auto static_short = Short;
        static constexpr bool named_at_compile_time = static_named_;
        static constexpr bool element_mode = Multiple;
        static constexpr bool structured_syntax = detail::structured_conversion<Converter, element_type>;
        static constexpr bool is_flag = std::same_as<T, bool>;
        static constexpr bool has_default = !std::same_as<Default, detail::NoDefault>;

        static_assert(Short.size() <= 1, "short name must have one character");
        static_assert(static_named_ || Short.size() == 0, "runtime short name belongs in the constructor");
        static_assert(!static_named_ || detail::validName(Name.view()), "invalid static option name");
        static_assert(Short.size() == 0 || detail::validName(Short.view()), "invalid static short name");
        static_assert(
            !BorrowsInput<T>::value && !BorrowsInput<element_type>::value,
            "value storage must own strings: use std::string, not std::string_view"
        );
        static_assert(
            std::default_initializable<T> && std::default_initializable<element_type>,
            "parsed values must be default-constructible"
        );
        static_assert(
            !Multiple || detail::appendable_sequence<T>, "multi() needs appendable SequenceTraits"
        );

        constexpr Option()
            requires(static_named_ && std::default_initializable<Converter> &&
                     std::default_initializable<Validator> && !has_default)
        = default;
        explicit Option(std::string_view name, std::string_view short_name = {})
            requires(
                !static_named_ && std::default_initializable<Converter> &&
                std::default_initializable<Validator> && !has_default
            )
            : names_(std::pair{std::string{name}, std::string{short_name}})
        {
        }

        [[nodiscard]] constexpr std::string_view name() const noexcept
        {
            if constexpr (static_named_)
                return Name.view();
            else
                return names_.first;
        }
        [[nodiscard]] constexpr std::string_view shortName() const noexcept
        {
            if constexpr (Short.size() != 0)
                return Short.view();
            else if constexpr (!static_named_)
                return names_.second;
            else
                return {};
        }
        [[nodiscard]] constexpr std::string_view description() const noexcept
        {
            return description_;
        }

        [[nodiscard]] constexpr Option required(bool yes = true) const&
        {
            return Option(*this).required(yes);
        }
        [[nodiscard]] constexpr Option required(bool yes = true) &&
        {
            settings_.required = yes;
            return std::move(*this);
        }
        [[nodiscard]] constexpr auto multi() const&
            requires detail::appendable_sequence<T>
        {
            return Option(*this).multi();
        }
        [[nodiscard]] constexpr auto multi() &&
            requires detail::appendable_sequence<T>
        {
            using O = Option<T, Name, Short, Converter, Validator, true, Default>;
            return O{
                std::move(names_),
                settings_,
                std::move(default_),
                description_,
                std::move(converter_),
                std::move(validator_)
            };
        }
        [[nodiscard]] constexpr auto arity(std::size_t min, std::size_t max) const&
            requires detail::appendable_sequence<T>
        {
            return Option(*this).arity(min, max);
        }
        [[nodiscard]] constexpr auto arity(std::size_t min, std::size_t max) &&
            requires detail::appendable_sequence<T>
        {
            auto r = std::move(*this).multi();
            r.settings_.min_values = min;
            r.settings_.max_values = max;
            return r;
        }
        [[nodiscard]] constexpr Option delimiter(char c) const&
            requires Multiple
        {
            return Option(*this).delimiter(c);
        }
        [[nodiscard]] constexpr Option delimiter(char c) &&
            requires Multiple
        {
            settings_.delimiter = c;
            return std::move(*this);
        }
        [[nodiscard]] constexpr Option positional() const&
            requires(!is_flag)
        {
            return Option(*this).positional();
        }
        [[nodiscard]] constexpr Option positional() && requires(!is_flag) {
            settings_.positional = true;
            return std::move(*this);
        } [[nodiscard]] constexpr Option onRepeat(ERepeat how) const&
        {
            return Option(*this).onRepeat(how);
        }
        [[nodiscard]] constexpr Option onRepeat(ERepeat how) &&
        {
            settings_.repeats = how;
            return std::move(*this);
        }
        [[nodiscard]] constexpr auto def(T value) const&
            requires std::copy_constructible<T>
        {
            return Option(*this).def(std::move(value));
        }
        [[nodiscard]] constexpr auto def(T value) &&
            requires std::copy_constructible<T>
        {
            using O = Option<T, Name, Short, Converter, Validator, Multiple, detail::StoredDefault<T>>;
            return O{
                std::move(names_),
                settings_,
                {std::move(value)},
                description_,
                std::move(converter_),
                std::move(validator_)
            };
        }
        [[nodiscard]] constexpr Option desc(std::string_view text) const&
        {
            return Option(*this).desc(text);
        }
        [[nodiscard]] constexpr Option desc(std::string_view text) &&
        {
            description_ = text;
            return std::move(*this);
        }
        template<class S>
            requires(std::same_as<std::remove_cvref_t<S>, std::string> && !std::is_lvalue_reference_v<S>)
        Option desc(S&&) const& = delete;
        template<class S>
            requires(std::same_as<std::remove_cvref_t<S>, std::string> && !std::is_lvalue_reference_v<S>)
        Option desc(S&&) && = delete;

        template<class C>
        [[nodiscard]] constexpr auto convert(C converter) const&
        {
            return Option(*this).convert(std::move(converter));
        }
        template<class C>
        [[nodiscard]] constexpr auto convert(C converter) &&
        {
            using O = Option<T, Name, Short, C, Validator, Multiple, Default>;
            return O{
                std::move(names_),
                settings_,
                std::move(default_),
                description_,
                std::move(converter),
                std::move(validator_)
            };
        }
        template<class V>
        [[nodiscard]] constexpr auto check(V validator) const&
        {
            return Option(*this).check(std::move(validator));
        }
        template<class V>
        [[nodiscard]] constexpr auto check(V validator) &&
        {
            using Both = detail::AllOf<Validator, V>;
            using O = Option<T, Name, Short, Converter, Both, Multiple, Default>;
            return O{
                std::move(names_),
                settings_,
                std::move(default_),
                description_,
                std::move(converter_),
                Both{std::move(validator_), std::move(validator)}
            };
        }

        [[nodiscard]] constexpr bool isRequired() const noexcept
        {
            return settings_.required;
        }
        [[nodiscard]] constexpr bool isMulti() const noexcept
        {
            return Multiple;
        }
        [[nodiscard]] constexpr bool isPositional() const noexcept
        {
            return settings_.positional;
        }
        [[nodiscard]] constexpr std::size_t minValues() const noexcept
        {
            return settings_.min_values;
        }
        [[nodiscard]] constexpr std::size_t maxValues() const noexcept
        {
            return settings_.max_values;
        }
        [[nodiscard]] constexpr char separator() const noexcept
        {
            return settings_.delimiter;
        }
        [[nodiscard]] constexpr ERepeat repeats() const noexcept
        {
            return settings_.repeats;
        }
        [[nodiscard]] constexpr const T* defaultValue() const noexcept
        {
            if constexpr (has_default)
                return &default_.value;
            else
                return nullptr;
        }
        [[nodiscard]] constexpr const Converter& converter() const noexcept
        {
            return converter_;
        }
        [[nodiscard]] constexpr const Validator& validator() const noexcept
        {
            return validator_;
        }

    private:
        template<class, ::lux::cxx::fixed_string, ::lux::cxx::fixed_string, class, class, bool, class>
        friend class Option;
        constexpr Option(
            names_storage names, OptionSettings settings, Default default_value,
            std::string_view description, Converter converter, Validator validator
        )
            : names_(std::move(names)),
              settings_(settings),
              default_(std::move(default_value)),
              description_(description),
              converter_(std::move(converter)),
              validator_(std::move(validator))
        {
        }

        LUX_CXX_ARGS_NO_UNIQUE_ADDRESS names_storage names_{};
        OptionSettings settings_{};
        LUX_CXX_ARGS_NO_UNIQUE_ADDRESS Default default_{};
        std::string_view description_{}; // borrowed documentation; never used to store parsed data
        LUX_CXX_ARGS_NO_UNIQUE_ADDRESS Converter converter_{};
        LUX_CXX_ARGS_NO_UNIQUE_ADDRESS Validator validator_{};
    };
} // namespace lux::cxx::args
