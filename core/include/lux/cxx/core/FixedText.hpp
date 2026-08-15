#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <string_view>

namespace lux::cxx
{
    template <std::size_t Capacity, typename Char = char>
    class FixedText final
    {
      public:
        using value_type = Char;
        using size_type = std::size_t;

        constexpr FixedText() noexcept = default;

        template <std::size_t Size>
        consteval FixedText(const Char (&text)[Size]) noexcept
        {
            static_assert(Size > 0);
            static_assert(Size - 1 <= Capacity, "literal exceeds FixedText capacity");
            for (std::size_t index = 0; index + 1 < Size; ++index)
            {
                storage_[index] = text[index];
            }
            size_ = Size - 1;
            storage_[size_] = Char{};
        }

        constexpr explicit FixedText(std::basic_string_view<Char> text) noexcept
        {
            assign(text);
        }

        [[nodiscard]] constexpr bool assign(
            std::basic_string_view<Char> text
        ) noexcept
        {
            size_ = (std::min)(Capacity, text.size());
            for (std::size_t index = 0; index < size_; ++index)
            {
                storage_[index] = text[index];
            }
            storage_[size_] = Char{};
            return size_ == text.size();
        }

        [[nodiscard]] constexpr bool append(
            std::basic_string_view<Char> text
        ) noexcept
        {
            const auto available = Capacity - size_;
            const auto count = (std::min)(available, text.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                storage_[size_ + index] = text[index];
            }
            size_ += count;
            storage_[size_] = Char{};
            return count == text.size();
        }

        constexpr void clear() noexcept
        {
            size_ = 0;
            storage_[0] = Char{};
        }

        [[nodiscard]] constexpr const Char* data() const noexcept
        {
            return storage_.data();
        }

        [[nodiscard]] constexpr const Char* c_str() const noexcept
        {
            return storage_.data();
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] static constexpr std::size_t capacity() noexcept
        {
            return Capacity;
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] constexpr std::basic_string_view<Char> view() const noexcept
        {
            return {storage_.data(), size_};
        }

        [[nodiscard]] constexpr explicit operator std::basic_string_view<Char>() const noexcept
        {
            return view();
        }

        [[nodiscard]] constexpr Char operator[](std::size_t index) const noexcept
        {
            return storage_[index];
        }

        [[nodiscard]] constexpr bool operator==(const FixedText& other) const noexcept
        {
            return view() == other.view();
        }

        [[nodiscard]] constexpr auto operator<=>(const FixedText& other) const noexcept
        {
            return view() <=> other.view();
        }

      private:
        std::array<Char, Capacity + 1> storage_{};
        std::size_t size_ = 0;
    };

    template <typename Char, std::size_t Size>
    FixedText(const Char (&)[Size]) -> FixedText<Size - 1, Char>;
} // namespace lux::cxx
