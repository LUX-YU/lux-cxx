#pragma once
// Owning, type-preserving result. Compile-time names select tuple slots.
// Include only this file when parse() + Result is desired.

#include <lux/cxx/arguments/detail/Engine.hpp>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lux::cxx::args
{

    enum class EReadStatus : std::uint8_t
    {
        OK,
        ABSENT,
        UNKNOWN_NAME,
        WRONG_TYPE
    };

    namespace typed
    {
        template<class... Options>
        class Result;
    }
    namespace detail
    {
        template<class... Options>
        class ResultSink;
        template<bool Enabled, std::size_t N>
        struct ResultNames;
        template<std::size_t N>
        struct ResultNames<true, N>
        {
            std::array<std::string, N> names{};
            void set(std::size_t i, std::string_view value)
            {
                names[i].assign(value);
            }
            [[nodiscard]] std::string_view get(std::size_t i) const noexcept
            {
                return names[i];
            }
        };
        template<std::size_t N>
        struct ResultNames<false, N>
        {
            void set(std::size_t, std::string_view) noexcept {}
            [[nodiscard]] std::string_view get(std::size_t) const noexcept
            {
                return {};
            }
        };
    } // namespace detail

    namespace typed
    {

        template<class... Options>
        class [[nodiscard]] Result
        {
            static constexpr std::size_t option_count_ = sizeof...(Options);
            using values_type = std::tuple<std::optional<typename Options::value_type>...>;
            static constexpr std::array<std::string_view, option_count_> static_names_{
                Options::static_name.view()...
            };
            static constexpr bool all_static_ = (Options::named_at_compile_time && ...);
            using names_type = detail::ResultNames<!all_static_, option_count_>;

            template<::lux::cxx::fixed_string Key>
            static consteval std::size_t indexOf()
            {
                for (std::size_t i = 0; i < option_count_; ++i)
                    if (!static_names_[i].empty() && static_names_[i] == Key.view())
                        return i;
                return option_count_;
            }

        public:
            static constexpr std::size_t size = option_count_;
            [[nodiscard]] explicit operator bool() const noexcept
            {
                return !error_;
            }
            [[nodiscard]] Error error() const noexcept
            {
                return error_;
            }

            template<::lux::cxx::fixed_string Key>
            [[nodiscard]] decltype(auto) get() & noexcept
            {
                constexpr auto I = indexOf<Key>();
                static_assert(I < option_count_, "option name not declared as a compile-time key");
                return (std::get<I>(values_));
            }
            template<::lux::cxx::fixed_string Key>
            [[nodiscard]] decltype(auto) get() const& noexcept
            {
                constexpr auto I = indexOf<Key>();
                static_assert(I < option_count_, "option name not declared as a compile-time key");
                return (std::get<I>(values_));
            }
            // Prevent dangling references when get() is called on a temporary Result.
            template<::lux::cxx::fixed_string Key>
            void get() && = delete;
            template<::lux::cxx::fixed_string Key>
            void get() const&& = delete;

            template<::lux::cxx::fixed_string Key>
            [[nodiscard]] bool supplied() const noexcept
            {
                constexpr auto I = indexOf<Key>();
                static_assert(I < option_count_, "unknown compile-time key");
                return supplied_[I];
            }

            template<::lux::cxx::fixed_string Key, class V>
            [[nodiscard]] bool get(V* destination) const
            {
                constexpr auto I = indexOf<Key>();
                static_assert(I < option_count_, "unknown compile-time key");
                using T = typename std::tuple_element_t<I, std::tuple<Options...>>::value_type;
                static_assert(
                    std::same_as<std::remove_cv_t<V>, T> && std::is_assignable_v<V&, const T&>,
                    "destination must be the registered type and copy-assignable"
                );
                if (!destination || !std::get<I>(values_))
                    return false;
                *destination = *std::get<I>(values_);
                return true;
            }
            template<::lux::cxx::fixed_string Key, class V>
            [[nodiscard]] bool take(V* destination) &
            {
                constexpr auto I = indexOf<Key>();
                static_assert(I < option_count_, "unknown compile-time key");
                using T = typename std::tuple_element_t<I, std::tuple<Options...>>::value_type;
                static_assert(
                    std::same_as<std::remove_cv_t<V>, T> && std::is_assignable_v<V&, T&&>,
                    "destination must be the registered type and move-assignable"
                );
                if (!destination || !std::get<I>(values_))
                    return false;
                *destination = std::move(*std::get<I>(values_));
                std::get<I>(values_).reset();
                return true;
            }
            template<class O, class V>
            [[nodiscard]] bool get(const O& handle, V* destination) const
            {
                if constexpr (O::named_at_compile_time)
                {
                    constexpr auto I = indexOf<O::static_name>();
                    static_assert(I < option_count_, "handle name is not present in this Result");
                    using Registered = typename std::tuple_element_t<I, std::tuple<Options...>>::value_type;
                    static_assert(
                        std::same_as<typename O::value_type, Registered>, "handle value type does not match"
                    );
                    return get<O::static_name>(destination);
                }
                else
                    return get<typename O::value_type>(handle.name(), destination) == EReadStatus::OK;
            }

            template<class T>
            [[nodiscard]] EReadStatus get(std::string_view name, T* destination) const
            {
                if (!destination)
                    return EReadStatus::WRONG_TYPE;
                EReadStatus status = EReadStatus::UNKNOWN_NAME;
                bool found = false;
                const auto visit = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                    if (found)
                        return;
                    using Opt = std::tuple_element_t<I, std::tuple<Options...>>;
                    auto candidate = static_names_[I];
                    if constexpr (!Opt::named_at_compile_time)
                        candidate = names_.get(I);
                    if (candidate != name)
                        return;
                    found = true;
                    using V = typename Opt::value_type;
                    if constexpr (std::same_as<V, T> && std::is_assignable_v<T&, const T&>)
                    {
                        const auto& v = std::get<I>(values_);
                        if (!v)
                            status = EReadStatus::ABSENT;
                        else
                        {
                            *destination = *v;
                            status = EReadStatus::OK;
                        }
                    }
                    else
                        status = EReadStatus::WRONG_TYPE;
                };
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (visit(std::integral_constant<std::size_t, I>{}), ...);
                }(std::index_sequence_for<Options...>{});
                return status;
            }

            template<std::size_t I>
            [[nodiscard]] decltype(auto) at() & noexcept
            {
                static_assert(I < option_count_);
                return (std::get<I>(values_));
            }
            template<std::size_t I>
            [[nodiscard]] decltype(auto) at() const& noexcept
            {
                static_assert(I < option_count_);
                return (std::get<I>(values_));
            }
            template<std::size_t I>
            void at() && = delete;
            template<std::size_t I>
            void at() const&& = delete;

        private:
            friend class ::lux::cxx::args::detail::ResultSink<Options...>;
            values_type values_{};
            std::array<bool, option_count_> supplied_{};
            LUX_CXX_ARGS_NO_UNIQUE_ADDRESS names_type names_{};
            Error error_{};
        };

    } // namespace typed

    namespace detail
    {

        template<class... Options>
        class ResultSink
        {
        public:
            explicit ResultSink(typed::Result<Options...>& result) noexcept
                : result_(result)
            {
            }
            template<class Descriptors>
            [[nodiscard]] Error seed(const Descriptors& ds)
            {
                Error error{};
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (([&] {
                         using Opt = std::tuple_element_t<I, std::tuple<Options...>>;
                         using T = typename Opt::value_type;
                         if constexpr (!Opt::named_at_compile_time)
                             result_.names_.set(I, std::get<I>(ds).name());
                         if constexpr (Opt::has_default)
                         {
                             const auto* default_value = std::get<I>(ds).defaultValue();
                             if constexpr (std::copy_constructible<T>)
                                 std::get<I>(result_.values_).emplace(*default_value);
                             else
                                 error = {EErrorCode::INVALID_SCHEMA, static_cast<option_id>(I)};
                         }
                     }()),
                     ...);
                }(std::index_sequence_for<Options...>{});
                return error;
            }
            template<std::size_t I>
            [[nodiscard]] auto& value()
            {
                auto& slot = std::get<I>(result_.values_);
                if (!slot)
                    slot.emplace();
                return *slot;
            }
            void supplied(option_id id) noexcept
            {
                result_.supplied_[id] = true;
            }
            void setError(Error e) noexcept
            {
                result_.error_ = e;
            }

        private:
            typed::Result<Options...>& result_;
        };

    } // namespace detail

    // Public entry points. `parse(text)` contains option tokens only; `parse(argc,argv)`
    // skips argv[0]. Result owns its strings and dynamic option names.
    template<TokenSource Source, class... Options>
    [[nodiscard]] typed::Result<Options...> parseFrom(
        Source& source, Workspace& work, const Options&... options
    )
    {
        typed::Result<Options...> output{};
        detail::ResultSink<Options...> sink{output};
        sink.setError(detail::Engine<Options...>::run(source, work, sink, options...));
        return output;
    }

    template<class... Options>
    [[nodiscard]] typed::Result<Options...> parse(std::string_view text, const Options&... options)
    {
        Workspace work;
        BasicTextSource<(Options::structured_syntax || ...)> source{text};
        return parseFrom(source, work, options...);
    }
    template<class... Options>
    [[nodiscard]] typed::Result<Options...> parse(
        int argc, const char* const* argv, const Options&... options
    )
    {
        Workspace work;
        ArgvSource source{argc, argv};
        return parseFrom(source, work, options...);
    }
    template<class... Options>
    [[nodiscard]] typed::Result<Options...> parse(
        std::span<const std::string_view> tokens, const Options&... options
    )
    {
        Workspace work;
        ViewSource source{tokens};
        return parseFrom(source, work, options...);
    }

} // namespace lux::cxx::args
