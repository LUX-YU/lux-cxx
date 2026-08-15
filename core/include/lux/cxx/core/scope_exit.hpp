#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    template <typename Callable>
    class scope_exit final
    {
      public:
        template <typename Source>
        requires std::constructible_from<Callable, Source>
        constexpr explicit scope_exit(Source&& callable) noexcept(
            std::is_nothrow_constructible_v<Callable, Source>
        )
            : callable_(std::forward<Source>(callable))
        {
        }

        scope_exit(const scope_exit&) = delete;
        scope_exit& operator=(const scope_exit&) = delete;
        scope_exit& operator=(scope_exit&&) = delete;

        constexpr scope_exit(scope_exit&& other) noexcept(
            std::is_nothrow_move_constructible_v<Callable>
        )
            : callable_(std::move(other.callable_)),
              active_(std::exchange(other.active_, false))
        {
        }

        constexpr ~scope_exit() noexcept(noexcept(std::declval<Callable&>()()))
        {
            if (active_) callable_();
        }

        constexpr void release() noexcept
        {
            active_ = false;
        }

      private:
        [[no_unique_address]] Callable callable_;
        bool active_ = true;
    };

    template <typename Callable>
    scope_exit(Callable) -> scope_exit<Callable>;
} // namespace lux::cxx
