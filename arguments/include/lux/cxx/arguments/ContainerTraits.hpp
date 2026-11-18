#pragma once
// Opt-in container adaptation. Include this for .multi(); StructuredValue.hpp
// also includes it. Scalar-only clients do not pull in STL container adapters.
// See docs/TRAITS.md for the complete, intentionally small customization contracts.

#include <lux/cxx/arguments/Value.hpp>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lux::cxx::args
{

    inline constexpr std::size_t dynamic_extent = unlimited;
    enum class EDuplicateKeys : std::uint8_t
    {
        REJECT,
        FIRST,
        LAST
    };

    namespace detail
    {
        // Shared implementation, not structural detection: a user type opts in by
        // specializing sequence_traits, rather than accidentally satisfying push_back.
        template<class C>
        struct PushSequence
        {
            using value_type = typename C::value_type;
            static constexpr std::size_t extent = dynamic_extent;
            static value_type makeElement(const C& out)
            {
                return std::make_obj_using_allocator<value_type>(out.get_allocator());
            }
            static void clear(C& out) noexcept(noexcept(out.clear()))
            {
                out.clear();
            }
            static EErrorCode append(C& out, value_type&& value)
            {
                out.push_back(std::move(value));
                return EErrorCode::OK;
            }
        };
        template<class C>
        struct IndexedSequence : PushSequence<C>
        {
            using value_type = typename C::value_type;
            static std::size_t size(const C& out) noexcept
            {
                return out.size();
            }
            static EErrorCode resize(C& out, std::size_t n)
            {
                out.resize(n);
                return EErrorCode::OK;
            }
            static decltype(auto) at(C& out, std::size_t i) noexcept
            {
                return out[i];
            }
        };
        template<class C>
        struct InsertSequence
        {
            using value_type = typename C::value_type;
            static constexpr std::size_t extent = dynamic_extent;
            static value_type makeElement(const C& out)
            {
                return std::make_obj_using_allocator<value_type>(out.get_allocator());
            }
            static void clear(C& out) noexcept(noexcept(out.clear()))
            {
                out.clear();
            }
            static EErrorCode append(C& out, value_type&& value)
            {
                out.insert(std::move(value));
                return EErrorCode::OK;
            }
        };
        template<class M>
        struct UniqueMap
        {
            using key_type = typename M::key_type;
            using mapped_type = typename M::mapped_type;
            static key_type makeKey(const M& out)
            {
                return std::make_obj_using_allocator<key_type>(out.get_allocator());
            }
            static mapped_type makeMapped(const M& out)
            {
                return std::make_obj_using_allocator<mapped_type>(out.get_allocator());
            }
            static void clear(M& out) noexcept(noexcept(out.clear()))
            {
                out.clear();
            }
            static EErrorCode insert(M& out, key_type&& key, mapped_type&& value, EDuplicateKeys policy)
            {
                auto [it, inserted] = out.try_emplace(std::move(key), std::move(value));
                if (inserted)
                    return EErrorCode::OK;
                if (policy == EDuplicateKeys::REJECT)
                    return EErrorCode::DUPLICATE_KEY;
                if (policy == EDuplicateKeys::LAST)
                    it->second = std::move(value);
                return EErrorCode::OK;
            }
        };
        template<class T>
        concept mapping = requires {
            typename MapTraits<T>::key_type;
            typename MapTraits<T>::mapped_type;
        };
        template<class T>
        concept tuple_like = requires { TupleTraits<T>::size; };
        template<class T>
        concept indexed_dynamic_sequence = sequence<T> && requires(T& v, std::size_t i) {
            { SequenceTraits<T>::size(v) } -> std::convertible_to<std::size_t>;
            { SequenceTraits<T>::resize(v, i) } -> std::same_as<EErrorCode>;
            { SequenceTraits<T>::at(v, i) } -> std::same_as<typename SequenceTraits<T>::value_type&>;
        };
    } // namespace detail

    template<class T, class A>
    struct SequenceTraits<std::vector<T, A>> : detail::IndexedSequence<std::vector<T, A>>
    {
    };
    template<class T, class A>
    struct SequenceTraits<std::deque<T, A>> : detail::IndexedSequence<std::deque<T, A>>
    {
    };
    template<class T, class A>
    struct SequenceTraits<std::list<T, A>> : detail::PushSequence<std::list<T, A>>
    {
    };
    template<class T, std::size_t N>
    struct SequenceTraits<std::array<T, N>>
    {
        using value_type = T;
        static constexpr std::size_t extent = N;
        static T& at(std::array<T, N>& out, std::size_t i) noexcept
        {
            return out[i];
        }
    };
    template<class T, class Compare, class A>
    struct SequenceTraits<std::set<T, Compare, A>> : detail::InsertSequence<std::set<T, Compare, A>>
    {
    };
    template<class T, class Compare, class A>
    struct SequenceTraits<std::multiset<T, Compare, A>>
        : detail::InsertSequence<std::multiset<T, Compare, A>>
    {
    };
    template<class T, class Hash, class Equal, class A>
    struct SequenceTraits<std::unordered_set<T, Hash, Equal, A>>
        : detail::InsertSequence<std::unordered_set<T, Hash, Equal, A>>
    {
    };
    template<class T, class Hash, class Equal, class A>
    struct SequenceTraits<std::unordered_multiset<T, Hash, Equal, A>>
        : detail::InsertSequence<std::unordered_multiset<T, Hash, Equal, A>>
    {
    };

    template<class K, class V, class Compare, class A>
    struct MapTraits<std::map<K, V, Compare, A>> : detail::UniqueMap<std::map<K, V, Compare, A>>
    {
    };
    template<class K, class V, class Hash, class Equal, class A>
    struct MapTraits<std::unordered_map<K, V, Hash, Equal, A>>
        : detail::UniqueMap<std::unordered_map<K, V, Hash, Equal, A>>
    {
    };

    template<class... T>
    struct TupleTraits<std::tuple<T...>>
    {
        static constexpr std::size_t size = sizeof...(T);
        template<std::size_t I>
        static decltype(auto) get(std::tuple<T...>& out) noexcept
        {
            return std::get<I>(out);
        }
    };
    template<class A, class B>
    struct TupleTraits<std::pair<A, B>>
    {
        static constexpr std::size_t size = 2;
        template<std::size_t I>
        static decltype(auto) get(std::pair<A, B>& out) noexcept
        {
            return std::get<I>(out);
        }
    };

    // Recursive ownership checking, including user-adapted containers.
    template<class T>
        requires detail::sequence<T>
    struct BorrowsInput<T> : BorrowsInput<typename SequenceTraits<T>::value_type>
    {
    };
    template<class T>
        requires detail::mapping<T>
    struct BorrowsInput<T> : std::bool_constant<
                                 BorrowsInput<typename MapTraits<T>::key_type>::value ||
                                 BorrowsInput<typename MapTraits<T>::mapped_type>::value>
    {
    };
    template<class T>
        requires detail::tuple_like<T>
    struct BorrowsInput<T> : std::bool_constant<[]<std::size_t... I>(std::index_sequence<I...>) {
        return (
            BorrowsInput<std::remove_cvref_t<decltype(TupleTraits<T>::template get<I>(std::declval<T&>())
            )>>::value ||
            ...
        );
    }(std::make_index_sequence<TupleTraits<T>::size>{})>
    {
    };

} // namespace lux::cxx::args
