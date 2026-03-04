#pragma once
/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file fixed_string.hpp
 * @brief A compile-time fixed-capacity string that can be used as a non-type
 *        template parameter (NTTP). Extends literal_ct_string with runtime-friendly
 *        operations (comparison, concatenation, search, slicing, I/O).
 */

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <string_view>
#include <type_traits>

namespace lux::cxx
{
    /**
     * @class fixed_string
     * @brief A null-terminated, fixed-capacity string suitable for use as an NTTP.
     *
     * The capacity @p N is the maximum number of characters (excluding the null
     * terminator). The string stores its current length so that embedded NULs
     * are supported, but the data array is always null-terminated for C
     * compatibility.
     *
     * @tparam N   Maximum number of characters (not counting the null terminator).
     * @tparam CharT Character type (default: char).
     */
    template <std::size_t N, typename CharT = char>
    struct fixed_string
    {
        using char_type    = CharT;
        using size_type    = std::size_t;
        using view_type    = std::basic_string_view<CharT>;

        static constexpr size_type capacity = N;

        CharT    data_[N + 1]{};
        size_type size_ = 0;

        // ---- constructors ---------------------------------------------------

        /** @brief Default-constructs an empty string. */
        constexpr fixed_string() noexcept = default;

        /** @brief Constructs from a character array literal (deduces size). */
        template <std::size_t M>
        constexpr fixed_string(const CharT (&str)[M]) noexcept
            : size_(M - 1)      // M includes the null terminator
        {
            static_assert(M - 1 <= N, "String literal exceeds fixed_string capacity");
            for (size_type i = 0; i < size_; ++i)
                data_[i] = str[i];
            data_[size_] = CharT{};
        }

        /** @brief Constructs from a string_view (length must fit). */
        constexpr explicit fixed_string(view_type sv) noexcept
            : size_(sv.size())
        {
            for (size_type i = 0; i < size_; ++i)
                data_[i] = sv[i];
            data_[size_] = CharT{};
        }

        /** @brief Constructs from a pointer and a length. */
        constexpr fixed_string(const CharT* ptr, size_type len) noexcept
            : size_(len)
        {
            for (size_type i = 0; i < size_; ++i)
                data_[i] = ptr[i];
            data_[size_] = CharT{};
        }

        /** @brief Constructs a string filled with @p count copies of @p ch. */
        constexpr fixed_string(size_type count, CharT ch) noexcept
            : size_(count)
        {
            for (size_type i = 0; i < size_; ++i)
                data_[i] = ch;
            data_[size_] = CharT{};
        }

        // ---- element access -------------------------------------------------

        [[nodiscard]] constexpr const CharT* data()  const noexcept { return data_; }
        [[nodiscard]] constexpr const CharT* c_str() const noexcept { return data_; }
        [[nodiscard]] constexpr size_type    size()  const noexcept { return size_; }
        [[nodiscard]] constexpr size_type    length()const noexcept { return size_; }
        [[nodiscard]] constexpr bool         empty() const noexcept { return size_ == 0; }

        [[nodiscard]] constexpr CharT  operator[](size_type i) const noexcept { return data_[i]; }
        [[nodiscard]] constexpr CharT& operator[](size_type i)       noexcept { return data_[i]; }

        [[nodiscard]] constexpr CharT  front() const noexcept { return data_[0]; }
        [[nodiscard]] constexpr CharT  back()  const noexcept { return data_[size_ - 1]; }

        // ---- conversion -----------------------------------------------------

        [[nodiscard]] constexpr view_type view() const noexcept
        {
            return view_type(data_, size_);
        }

        [[nodiscard]] constexpr operator view_type() const noexcept { return view(); }

        // ---- iterators ------------------------------------------------------

        [[nodiscard]] constexpr const CharT* begin()  const noexcept { return data_; }
        [[nodiscard]] constexpr const CharT* end()    const noexcept { return data_ + size_; }
        [[nodiscard]] constexpr const CharT* cbegin() const noexcept { return data_; }
        [[nodiscard]] constexpr const CharT* cend()   const noexcept { return data_ + size_; }

        // ---- comparison (works across different capacities) -----------------

        template <std::size_t M>
        [[nodiscard]] constexpr bool operator==(const fixed_string<M, CharT>& rhs) const noexcept
        {
            return view() == rhs.view();
        }

        template <std::size_t M>
        [[nodiscard]] constexpr auto operator<=>(const fixed_string<M, CharT>& rhs) const noexcept
        {
            return view() <=> rhs.view();
        }

        [[nodiscard]] constexpr bool operator==(view_type rhs) const noexcept
        {
            return view() == rhs;
        }

        [[nodiscard]] constexpr auto operator<=>(view_type rhs) const noexcept
        {
            return view() <=> rhs;
        }

        // ---- search ---------------------------------------------------------

        [[nodiscard]] constexpr bool starts_with(view_type prefix) const noexcept
        {
            return view().starts_with(prefix);
        }

        [[nodiscard]] constexpr bool ends_with(view_type suffix) const noexcept
        {
            return view().ends_with(suffix);
        }

        [[nodiscard]] constexpr bool contains(view_type needle) const noexcept
        {
            return find(needle) != view_type::npos;
        }

        [[nodiscard]] constexpr size_type find(view_type needle, size_type pos = 0) const noexcept
        {
            return view().find(needle, pos);
        }

        [[nodiscard]] constexpr size_type rfind(view_type needle, size_type pos = view_type::npos) const noexcept
        {
            return view().rfind(needle, pos);
        }

        // ---- sub-string (returns a potentially smaller fixed_string) --------

        /**
         * @brief Returns a sub-string as a fixed_string<N> (same capacity).
         * @param pos   Start position.
         * @param count Number of characters (clamped to available length).
         */
        [[nodiscard]] constexpr fixed_string substr(size_type pos, size_type count = view_type::npos) const noexcept
        {
            auto sv = view().substr(pos, count);
            return fixed_string(sv);
        }

        // ---- concatenation --------------------------------------------------

        /**
         * @brief Concatenates two fixed_strings. The result capacity is N + M.
         */
        template <std::size_t M>
        [[nodiscard]] constexpr auto operator+(const fixed_string<M, CharT>& rhs) const noexcept
            -> fixed_string<N + M, CharT>
        {
            fixed_string<N + M, CharT> result;
            for (size_type i = 0; i < size_; ++i)
                result.data_[i] = data_[i];
            for (size_type i = 0; i < rhs.size_; ++i)
                result.data_[size_ + i] = rhs.data_[i];
            result.size_ = size_ + rhs.size_;
            result.data_[result.size_] = CharT{};
            return result;
        }

        // ---- stream output --------------------------------------------------

        friend std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os, const fixed_string& s)
        {
            return os << s.view();
        }
    };

    // ---- deduction guides ---------------------------------------------------

    template <std::size_t M, typename CharT>
    fixed_string(const CharT (&)[M]) -> fixed_string<M - 1, CharT>;

    // ---- free function: compile-time concatenation --------------------------

    /**
     * @brief Concatenates two fixed_strings into a new one whose capacity is
     *        the sum of both source capacities.
     */
    template <std::size_t N1, std::size_t N2, typename CharT>
    [[nodiscard]] constexpr auto concat(const fixed_string<N1, CharT>& a,
                                        const fixed_string<N2, CharT>& b) noexcept
        -> fixed_string<N1 + N2, CharT>
    {
        return a + b;
    }

} // namespace lux::cxx
