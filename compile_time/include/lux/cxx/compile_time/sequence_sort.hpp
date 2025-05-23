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

    // Helper to filter elements greater than or equal to the pivot
    template<typename Sequence, typename T, T pivot>
    struct sort_filter_greater_equal_helper;

    template<typename T, T pivot, T first, T... Rest>
    struct sort_filter_greater_equal_helper<std::integer_sequence<T, first, Rest...>, T, pivot> {
        using filtered_tail = typename sort_filter_greater_equal_helper<std::integer_sequence<T, Rest...>, T, pivot>::type;
        using type = std::conditional_t<
            (first >= pivot),
            typename sort_prepend<filtered_tail, T, first>::type,
            filtered_tail
        >;
    };

    template<typename T, T pivot>
    struct sort_filter_greater_equal_helper<std::integer_sequence<T>, T, pivot> {
        using type = std::integer_sequence<T>;
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

    // Specialization: Multi-element sequence, applies quicksort
    template<typename T, T pivot, T next, T... Rest>
    struct quick_sort_sequence<std::integer_sequence<T, pivot, next, Rest...>> {
    private:
        // Partition sequence (excluding pivot) into left and right parts
        using left_seq          = typename sort_filter_less_helper<std::integer_sequence<T, next, Rest...>, T, pivot>::type;
        using right_seq         = typename sort_filter_greater_equal_helper<std::integer_sequence<T, next, Rest...>, T, pivot>::type;
        // Recursively sort left and right parts
        using sorted_left       = typename quick_sort_sequence<left_seq>::type;
        using sorted_right      = typename quick_sort_sequence<right_seq>::type;
        // Merge sorted left part, pivot (as a single-element sequence), and sorted right part
        using pivot_seq         = std::integer_sequence<T, pivot>;
        using left_plus_pivot   = typename concat_sequences<sorted_left, pivot_seq>::type;
    public:
        using type              = typename concat_sequences<left_plus_pivot, sorted_right>::type;
    };
}
