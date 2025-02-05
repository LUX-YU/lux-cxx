#include <utility>
#include <type_traits>

namespace lux::cxx
{
    // Helper to prepend a value to an integer sequence
    template<typename Sequence, typename T, T val>
    struct Prepend;

    template<typename T, T... Elems, T val>
    struct Prepend<std::integer_sequence<T, Elems...>, T, val> {
        using type = std::integer_sequence<T, val, Elems...>;
    };

    // Helper to filter elements less than the pivot
    template<typename Sequence, typename T, T pivot>
    struct FilterLessHelper;

    template<typename T, T pivot, T first, T... Rest>
    struct FilterLessHelper<std::integer_sequence<T, first, Rest...>, T, pivot> {
        using filtered_tail = typename FilterLessHelper<std::integer_sequence<T, Rest...>, T, pivot>::type;
        using type = std::conditional_t<
            (first < pivot),
            typename Prepend<filtered_tail, T, first>::type,
            filtered_tail
        >;
    };

    template<typename T, T pivot>
    struct FilterLessHelper<std::integer_sequence<T>, T, pivot> {
        using type = std::integer_sequence<T>;
    };

    // Helper to filter elements greater than or equal to the pivot
    template<typename Sequence, typename T, T pivot>
    struct FilterGreaterEqualHelper;

    template<typename T, T pivot, T first, T... Rest>
    struct FilterGreaterEqualHelper<std::integer_sequence<T, first, Rest...>, T, pivot> {
        using filtered_tail = typename FilterGreaterEqualHelper<std::integer_sequence<T, Rest...>, T, pivot>::type;
        using type = std::conditional_t<
            (first >= pivot),
            typename Prepend<filtered_tail, T, first>::type,
            filtered_tail
        >;
    };

    template<typename T, T pivot>
    struct FilterGreaterEqualHelper<std::integer_sequence<T>, T, pivot> {
        using type = std::integer_sequence<T>;
    };

    // Helper to concatenate two integer sequences
    template<typename Seq1, typename Seq2>
    struct ConcatSequences;

    template<typename T, T... S1, T... S2>
    struct ConcatSequences<std::integer_sequence<T, S1...>, std::integer_sequence<T, S2...>> {
        using type = std::integer_sequence<T, S1..., S2...>;
    };

    // Primary template for quicksort
    template<typename Seq>
    struct QuickSortSequence;  // Unspecified primary template declaration

    // Specialization: Empty sequence, returns an empty sequence
    template<typename T>
    struct QuickSortSequence<std::integer_sequence<T>> {
        using type = std::integer_sequence<T>;
    };

    // Specialization: Single-element sequence, returns itself
    template<typename T, T x>
    struct QuickSortSequence<std::integer_sequence<T, x>> {
        using type = std::integer_sequence<T, x>;
    };

    // Specialization: Multi-element sequence, applies quicksort
    template<typename T, T pivot, T next, T... Rest>
    struct QuickSortSequence<std::integer_sequence<T, pivot, next, Rest...>> {
    private:
        // Partition sequence (excluding pivot) into left and right parts
        using left_seq          = typename FilterLessHelper<std::integer_sequence<T, next, Rest...>, T, pivot>::type;
        using right_seq         = typename FilterGreaterEqualHelper<std::integer_sequence<T, next, Rest...>, T, pivot>::type;
        // Recursively sort left and right parts
        using sorted_left       = typename QuickSortSequence<left_seq>::type;
        using sorted_right      = typename QuickSortSequence<right_seq>::type;
        // Merge sorted left part, pivot (as a single-element sequence), and sorted right part
        using pivot_seq         = std::integer_sequence<T, pivot>;
        using left_plus_pivot   = typename ConcatSequences<sorted_left, pivot_seq>::type;
    public:
        using type              = typename ConcatSequences<left_plus_pivot, sorted_right>::type;
    };
}
