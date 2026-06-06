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

#include <utility>
#include <type_traits>

namespace lux::cxx
{
    // Helper to prepend a value to an integer sequence
    template<typename Sequence, typename T, T val>
    struct sort_prepend;

    template<typename T, T... Elems, T val>
    struct sort_prepend<std::integer_sequence<T, Elems...>, T, val> {
        using type = std::integer_sequence<T, val, Elems...>;
    };

    // Helper to filter elements less than the pivot
    template<typename Sequence, typename T, T pivot>
    struct sort_filter_less_helper;

    template<typename T, T pivot, T first, T... Rest>
    struct sort_filter_less_helper<std::integer_sequence<T, first, Rest...>, T, pivot> {
        using filtered_tail = typename sort_filter_less_helper<std::integer_sequence<T, Rest...>, T, pivot>::type;
        using type = std::conditional_t<
            (first < pivot),
            typename sort_prepend<filtered_tail, T, first>::type,
            filtered_tail
        >; 
    };

    template<typename T, T pivot>
    struct sort_filter_less_helper<std::integer_sequence<T>, T, pivot> {
        using type = std::integer_sequence<T>;
    };

    // Helper to filter elements EQUAL to the pivot
    template<typename Sequence, typename T, T pivot>
    struct sort_filter_equal_helper;

    template<typename T, T pivot, T first, T... Rest>
    struct sort_filter_equal_helper<std::integer_sequence<T, first, Rest...>, T, pivot> {
        using filtered_tail = typename sort_filter_equal_helper<std::integer_sequence<T, Rest...>, T, pivot>::type;
        using type = std::conditional_t<
            (first == pivot),
            typename sort_prepend<filtered_tail, T, first>::type,
            filtered_tail
        >;
    };

    template<typename T, T pivot>
    struct sort_filter_equal_helper<std::integer_sequence<T>, T, pivot> {
        using type = std::integer_sequence<T>;
    };

    // Helper to filter elements STRICTLY greater than the pivot
    template<typename Sequence, typename T, T pivot>
    struct sort_filter_greater_helper;

    template<typename T, T pivot, T first, T... Rest>
    struct sort_filter_greater_helper<std::integer_sequence<T, first, Rest...>, T, pivot> {
        using filtered_tail = typename sort_filter_greater_helper<std::integer_sequence<T, Rest...>, T, pivot>::type;
        using type = std::conditional_t<
            (first > pivot),
            typename sort_prepend<filtered_tail, T, first>::type,
            filtered_tail
        >;
    };

    template<typename T, T pivot>
    struct sort_filter_greater_helper<std::integer_sequence<T>, T, pivot> {
        using type = std::integer_sequence<T>;
    };

    // Compile-time element access: value at index I of an integer_sequence.
    template<typename Sequence, std::size_t I>
    struct seq_value_at;

    template<typename T, T First, T... Rest, std::size_t I>
    struct seq_value_at<std::integer_sequence<T, First, Rest...>, I> {
        static constexpr T value = seq_value_at<std::integer_sequence<T, Rest...>, I - 1>::value;
    };

    template<typename T, T First, T... Rest>
    struct seq_value_at<std::integer_sequence<T, First, Rest...>, 0> {
        static constexpr T value = First;
    };

    // Helper to concatenate two integer sequences
    template<typename Seq1, typename Seq2>
    struct concat_sequences;

    template<typename T, T... S1, T... S2>
    struct concat_sequences<std::integer_sequence<T, S1...>, std::integer_sequence<T, S2...>> {
        using type = std::integer_sequence<T, S1..., S2...>;
    };

    // Primary template for quicksort
    template<typename Seq>
    struct quick_sort_sequence;  // Unspecified primary template declaration

    // Specialization: Empty sequence, returns an empty sequence
    template<typename T>
    struct quick_sort_sequence<std::integer_sequence<T>> {
        using type = std::integer_sequence<T>;
    };

    // Specialization: Single-element sequence, returns itself
    template<typename T, T x>
    struct quick_sort_sequence<std::integer_sequence<T, x>> {
        using type = std::integer_sequence<T, x>;
    };

    // Specialization: Multi-element sequence, applies quicksort.
    //
    // Pivot = the MIDDLE element's value (not the head): a head pivot degrades to
    // O(n^2) instantiation depth + linear recursion on already-sorted input, which
    // blows the compiler's template-depth limit. Three-way partition (less / equal /
    // greater) keeps all duplicates of the pivot in the `equal` bucket, so the `less`
    // and `greater` sub-sequences are STRICTLY smaller than the input — this both
    // guarantees termination (even for all-equal inputs) and balances the recursion.
    template<typename T, T A, T B, T... Rest>
    struct quick_sort_sequence<std::integer_sequence<T, A, B, Rest...>> {
    private:
        using whole = std::integer_sequence<T, A, B, Rest...>;
        static constexpr std::size_t n     = 2 + sizeof...(Rest);
        static constexpr T           pivot = seq_value_at<whole, n / 2>::value;

        using less_seq        = typename sort_filter_less_helper<whole, T, pivot>::type;
        using equal_seq       = typename sort_filter_equal_helper<whole, T, pivot>::type;
        using greater_seq     = typename sort_filter_greater_helper<whole, T, pivot>::type;

        using sorted_less     = typename quick_sort_sequence<less_seq>::type;
        using sorted_greater  = typename quick_sort_sequence<greater_seq>::type;
        using less_plus_equal = typename concat_sequences<sorted_less, equal_seq>::type;
    public:
        using type            = typename concat_sequences<less_plus_equal, sorted_greater>::type;
    };
}
