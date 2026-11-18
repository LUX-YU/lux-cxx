#pragma once
// Direct binding into unrelated user objects, without an intermediate Result.
// Include only this file when parseInto() is desired.

#include <lux/cxx/arguments/detail/Engine.hpp>
#include <tuple>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace lux::cxx::args
{

    // Lvalue descriptors are borrowed (zero copy); rvalue descriptors are owned.
    // Thus bind(Option<int, "x">{}.required(), field) is lifetime-safe, while a
    // registered long-lived option is never copied on the hot path.
    template<class O, bool Reference>
    class Binding
    {
    public:
        using option_type = O;
        using value_type = typename O::value_type;
        explicit Binding(const O& descriptor, value_type& destination)
            requires Reference
            : option_(&descriptor),
              destination_(&destination)
        {
        }
        explicit Binding(O&& descriptor, value_type& destination)
            requires(!Reference)
            : option_(std::move(descriptor)),
              destination_(&destination)
        {
        }
        [[nodiscard]] const O& descriptor() const noexcept
        {
            if constexpr (Reference)
                return *option_;
            else
                return option_;
        }
        [[nodiscard]] value_type& destination() const noexcept
        {
            return *destination_;
        }

    private:
        std::conditional_t<Reference, const O*, O> option_;
        value_type* destination_;
    };

    template<class O>
    [[nodiscard]] auto bind(O&& descriptor, typename std::remove_cvref_t<O>::value_type& out)
    {
        using Type = std::remove_cvref_t<O>;
        return Binding<Type, std::is_lvalue_reference_v<O>>{std::forward<O>(descriptor), out};
    }

    namespace detail
    {

        template<class... Bindings>
        class DirectSink
        {
        public:
            explicit DirectSink(const Bindings&... bindings) noexcept
                : bindings_(bindings...)
            {
            }
            template<class Descriptors>
            [[nodiscard]] Error seed(const Descriptors& descriptions)
            {
                Error error{};
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (([&] {
                         using Bound = std::tuple_element_t<I, std::tuple<Bindings...>>;
                         using T = typename Bound::value_type;
                         if constexpr (Bound::option_type::has_default)
                         {
                             const auto* def = std::get<I>(descriptions).defaultValue();
                             if constexpr (std::is_assignable_v<T&, const T&>)
                                 value<I>() = *def;
                             else
                                 error = {EErrorCode::INVALID_SCHEMA, static_cast<option_id>(I)};
                         }
                     }()),
                     ...);
                }(std::index_sequence_for<Bindings...>{});
                return error;
            }
            template<std::size_t I>
            [[nodiscard]] auto& value() const noexcept
            {
                return std::get<I>(bindings_).destination();
            }
            void supplied(option_id) const noexcept {}

        private:
            std::tuple<const Bindings&...> bindings_;
        };

    } // namespace detail

    template<TokenSource Source, class... B>
    [[nodiscard]] Error parseIntoFrom(Source& source, Workspace& work, const B&... bound)
    {
        detail::DirectSink<B...> sink{bound...};
        return detail::Engine<typename B::option_type...>::run(source, work, sink, bound.descriptor()...);
    }

    template<class... B>
    [[nodiscard]] Error parseInto(std::string_view text, const B&... bound)
    {
        Workspace work;
        BasicTextSource<(B::option_type::structured_syntax || ...)> source{text};
        return parseIntoFrom(source, work, bound...);
    }
    template<class... B>
    [[nodiscard]] Error parseInto(int argc, const char* const* argv, const B&... bound)
    {
        Workspace work;
        ArgvSource source{argc, argv};
        return parseIntoFrom(source, work, bound...);
    }
    template<class... B>
    [[nodiscard]] Error parseInto(std::span<const std::string_view> tokens, const B&... bound)
    {
        Workspace work;
        ViewSource source{tokens};
        return parseIntoFrom(source, work, bound...);
    }

} // namespace lux::cxx::args
