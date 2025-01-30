#pragma once
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <memory>
#include <type_traits>
#include <utility>
#include <optional>
#include <concepts>  // C++20 concepts
#include <array>
#include <chrono>
#include <algorithm> // std::max

#include "tuple_traits.hpp"


namespace lux::cxx
{
    // -- Binding
    template <std::size_t Loc, typename T>
    struct in_binding
    {
        static constexpr std::size_t location = Loc;
        using value_t = T;
    };

    template <std::size_t Loc, typename T>
    struct out_binding
    {
        static constexpr std::size_t location = Loc;
        using value_t = T;
    };

    // -- descriptor
    template <typename... Bindings>
    struct node_descriptor
    {
        using bindings_tuple_t = std::tuple<Bindings...>;
    };

    // -- concepts
    namespace __concept
    {
        template <typename T>
        concept descriptor = requires {
            typename T::bindings_tuple_t;
            requires (std::tuple_size_v<typename T::bindings_tuple_t> >= 0);
        };

        // 这里原本写了 concept node = requires { typename T::input_descriptor; ...}
        // 但是某些编译器对 CRTP + concept 的支持可能有 Bug，或者在当前上下文类型不完整。
        // 建议改用 static_assert(__concept::node<T>)。
        template<typename T>
        concept node = requires {
            typename T::input_descriptor;
            typename T::output_descriptor;
            typename T::mapping_tuple;
        };
    }

    // -- dependency map
    template <std::size_t InLoc, __concept::node DepNode, std::size_t OutLoc>
    struct node_dependency_map
    {
        static constexpr std::size_t input_location = InLoc;
        using dependency_node_t = DepNode;
        static constexpr std::size_t dep_output_location = OutLoc;
    };

    // -- CRTP NodeBase
    template<typename Derived, __concept::descriptor InDesc,
        __concept::descriptor OutDesc, typename... Mapping>
    struct NodeBase
    {
        using input_descriptor = InDesc;
        using output_descriptor = OutDesc;
        using mapping_tuple = std::tuple<Mapping...>;

        void execute()
        {
            static_cast<Derived*>(this)->execute();
        }
    };

    // -- 提取节点依赖
    template <typename Node>
    struct node_dependencies
    {
        static_assert(__concept::node<Node>, "node_dependencies: Node must satisfy node concept");

    private:
        using mappings = typename Node::mapping_tuple;

        template<typename U> struct dep_node_of_map;

        template <std::size_t I, typename DN, std::size_t O>
        struct dep_node_of_map<node_dependency_map<I, DN, O>>
        {
            using type = DN;
        };

        template <typename... Ms>
        static auto find_deps(std::tuple<Ms...>)
        {
            return std::tuple<typename dep_node_of_map<Ms>::type...>{};
        }

    public:
        using type = decltype(find_deps(std::declval<mappings>()));
    };

    // -- 检查某节点 NodeT 的依赖是否都在 SortedTuple 中
    template <typename NodeT, typename SortedTuple>
    struct dependencies_in_sorted
    {
        static_assert(__concept::node<NodeT>,
            "dependencies_in_sorted: NodeT must satisfy node concept");

    private:
        using dependencies_tuple_t = typename node_dependencies<NodeT>::type;

        template <typename Dep, typename Tup>
        struct in_tuple;

        template <typename Dep>
        struct in_tuple<Dep, std::tuple<>> : std::false_type {};

        template <typename Dep, typename Head, typename... Tail>
        struct in_tuple<Dep, std::tuple<Head, Tail...>>
            : std::conditional_t<
            std::is_same_v<Dep, Head>,
            std::true_type,
            in_tuple<Dep, std::tuple<Tail...>>
            >
        {
        };

        template <typename... Ds>
        static constexpr bool checkAll(std::tuple<Ds...>)
        {
            return (... && in_tuple<Ds, SortedTuple>::value);
        }

    public:
        static constexpr bool value = checkAll(dependencies_tuple_t{});
    };

    //====================================================
    // 线性（单节点）拓扑排序
    //====================================================
    template <typename Unsorted, typename Sorted>
    struct topo_sort_impl
    {
    private:
        static constexpr bool unsorted_empty = (std::tuple_size_v<Unsorted> == 0);
        using finish_case = Sorted;

        // 找第一个依赖都满足的节点
        template <typename TTuple>
        struct find_satisfied;

        template <typename... Ns>
        struct find_satisfied<std::tuple<Ns...>>
        {
        private:
            template <typename N>
            static constexpr bool is_ok = dependencies_in_sorted<N, Sorted>::value;

            template <bool C, typename T>
            struct pick_if { using type = void; };

            template <typename T>
            struct pick_if<true, T> { using type = T; };

            template <typename Found, typename... Ts>
            struct scan_list
            {
                using type = Found; // 终止
            };

            // 逐个扫描
            template <typename Found, typename Head, typename... Tail>
            struct scan_list<Found, Head, Tail...>
            {
                static constexpr bool ok = is_ok<Head>;
                using maybe = typename pick_if<ok, Head>::type;
                using type = typename scan_list<
                    std::conditional_t<std::is_void_v<Found>, maybe, Found>,
                    Tail...
                >::type;
            };

        public:
            using type = typename scan_list<void, Ns...>::type;
        };

        using found = typename find_satisfied<Unsorted>::type;
        static constexpr bool found_any = !std::is_void_v<found>;

        using next_unsorted = tuple_remove_t<Unsorted, found>;
        using next_sorted = tuple_append_t<Sorted, found>;

        // 递归
        using step_case = typename topo_sort_impl<next_unsorted, next_sorted>::type;
    public:
        using type = std::conditional_t<
            unsorted_empty,
            finish_case,
            std::conditional_t<found_any, step_case, std::false_type>
        >;

        static_assert(
            !std::is_same_v<type, std::false_type>,
            "Topological sort: cycle or unresolvable dependency!"
            );
    };

    template <typename Sorted>
    struct topo_sort_impl<std::tuple<>, Sorted>
    {
        using type = Sorted;
    };

    template <typename NodeTuple>
    struct topological_sort;

    template <typename... Ns>
    struct topological_sort<std::tuple<Ns...>>
    {
        using type = typename topo_sort_impl<std::tuple<Ns...>, std::tuple<>>::type;
    };
} // end namespace lux::cxx

namespace lux::cxx
{
    //------------------------------------------------------------------
    // 1) We declare a master template flatten_tuple_of_tuples<T>.
    //    We also give a fallback for "non-tuple" T to avoid errors
    //    when T is not a tuple at all.
    //------------------------------------------------------------------
    template <typename T>
    struct flatten_tuple_of_tuples
    {
        // If T is NOT a tuple, just wrap T in a 1-element tuple
        // so that flattening "does nothing" interesting:
        using type = std::tuple<T>;
    };

    // 2) Specialization for the case T is exactly std::tuple<...>
    template <typename... Ts>
    struct flatten_tuple_of_tuples<std::tuple<Ts...>>
    {
    private:
        // We flatten each element Ts one by one and "concatenate"
        template <typename... Args>
        static auto flatten_all(std::tuple<Args...>)
        {
            // For each Arg in Args..., flatten it:
            // flatten_tuple_of_tuples<Arg>::type
            // then std::tuple_cat them together
            return (std::tuple_cat(typename flatten_tuple_of_tuples<Args>::type{}...));
        }
    public:
        using type = decltype(flatten_all(std::declval<std::tuple<Ts...>>()));
    };

    template <typename T>
    using flatten_tuple_of_tuples_t = typename flatten_tuple_of_tuples<T>::type;

    //-------------------------------------------------------
    // 2) Filter a tuple by a boolean predicate
    //-------------------------------------------------------
    // Filter by a boolean predicate
    template <typename Tuple, template <typename> class Pred>
    struct filter_tuple;

    template <template <typename> class Pred>
    struct filter_tuple<std::tuple<>, Pred>
    {
        using type = std::tuple<>;
    };

    template <typename Head, typename... Tail, template <typename> class Pred>
    struct filter_tuple<std::tuple<Head, Tail...>, Pred>
    {
    private:
        using tail_filtered = typename filter_tuple<std::tuple<Tail...>, Pred>::type;

        static constexpr bool keep = Pred<Head>::value;  // <--- ‘value’ must exist
    public:
        using type = std::conditional_t<
            keep,
            decltype(std::tuple_cat(std::declval<std::tuple<Head>>(),
                std::declval<tail_filtered>())),
            tail_filtered
        >;
    };

    template <typename Tuple, template <typename> class Pred>
    using filter_tuple_t = typename filter_tuple<Tuple, Pred>::type;

    // remove_if by a boolean predicate
    template <typename Tuple, template <typename> class Pred>
    struct remove_if_tuple;

    template <template <typename> class Pred>
    struct remove_if_tuple<std::tuple<>, Pred>
    {
        using type = std::tuple<>;
    };

    template <typename Head, typename... Tail, template <typename> class Pred>
    struct remove_if_tuple<std::tuple<Head, Tail...>, Pred>
    {
    private:
        using tail_removed = typename remove_if_tuple<std::tuple<Tail...>, Pred>::type;

        static constexpr bool remove = Pred<Head>::value;  // <--- ‘value’ must exist
    public:
        using type = std::conditional_t<
            remove,
            // skip Head
            tail_removed,
            // keep Head
            decltype(std::tuple_cat(std::declval<std::tuple<Head>>(), std::declval<tail_removed>()))
        >;
    };

    template <typename Tuple, template <typename> class Pred>
    using remove_if_tuple_t = typename remove_if_tuple<Tuple, Pred>::type;

    //-------------------------------------------------------
    // 4) The layered topological sort:
    //    layered_topo_sort_impl<Unsorted, SoFarGroups> => next SoFarGroups
    //-------------------------------------------------------
    // The recursive layer-building
    template <typename Unsorted, typename SoFarGroups>
    struct layered_topo_sort_impl
    {
    private:
        static constexpr bool is_empty = (std::tuple_size_v<Unsorted> == 0);
        using finished_case = SoFarGroups;

        // Flatten all previous layers to find what is "already satisfied"
        using FlattenedSoFar = flatten_tuple_of_tuples_t<SoFarGroups>;

        // We want to pick ALL nodes in Unsorted whose dependencies are satisfied by FlattenedSoFar
        // We'll do that by a "predicate" that returns true if a node can run now.
        template <typename N>
        struct can_run_now
        {
            static constexpr bool value =
                dependencies_in_sorted<N, FlattenedSoFar>::value;
        };

        using next_group = filter_tuple_t<Unsorted, can_run_now>; // All that can run now
        static constexpr bool next_group_empty = (std::tuple_size_v<next_group> == 0);

        // We'll remove them from Unsorted
        template <typename N>
        struct remove_if_can_run
        {
            static constexpr bool value =
                dependencies_in_sorted<N, FlattenedSoFar>::value;
        };
        using next_unsorted = remove_if_tuple_t<Unsorted, remove_if_can_run>;

        // If next_group is empty but we still have unsorted => cycle
        static constexpr bool have_unsorted = (std::tuple_size_v<Unsorted> != 0);

        // Append next_group as the next layer
        using appended_groups =
            decltype(std::tuple_cat(std::declval<SoFarGroups>(),
                std::declval<std::tuple<next_group>>()));

        // Recurse
        using step_case = typename layered_topo_sort_impl<next_unsorted, appended_groups>::type;

    public:
        using type = std::conditional_t<
            is_empty,
            // no more nodes => finished
            finished_case,
            // otherwise either produce next layer or error
            std::conditional_t<
            (next_group_empty&& have_unsorted),
            // cycle => error
            std::false_type,
            // otherwise continue
            step_case
            >
        >;

        static_assert(
            !std::is_same_v<type, std::false_type>,
            "Layered topological sort: cycle or unresolvable dependency!"
            );
    };

    // Base case: if Unsorted is empty, we’re done
    template <typename SoFarGroups>
    struct layered_topo_sort_impl<std::tuple<>, SoFarGroups>
    {
        using type = SoFarGroups;
    };

    // Convenience front-end
    template <typename NodeTuple>
    struct layered_topological_sort
    {
        using type = typename layered_topo_sort_impl<NodeTuple, std::tuple<>>::type;
    };
} // end namespace lux::cxx

//===============================================================
//  A "Pipeline" that can run either a linear or layered sort
//
//  Key differences from the previous snippet:
//  - We store only unique_ptr<NodeA>, unique_ptr<NodeB>, ...
//  - For layered sorts, we do a *two-level* compile-time iteration:
//    outer loop => each sub-tuple of nodes in the layer
//    inner loop => each node type in that sub-tuple
//  - Removed the "apply_one" references and the runtime-lambda
//    capturing "layerTag". Instead we do pure type-based iteration.
//
//===============================================================
namespace lux::cxx
{
    template <typename T>
    struct is_tuple_of_tuples : std::false_type {};

    template <typename... Layers>
    struct is_tuple_of_tuples<std::tuple<Layers...>>
    {
    private:
        static constexpr bool all_are_tuples = (... && is_tuple_v<Layers>);
    public:
        static constexpr bool value = (sizeof...(Layers) > 0) && all_are_tuples;
    };

    template <typename T>
    inline constexpr bool is_tuple_of_tuples_v = is_tuple_of_tuples<T>::value;

    //---------------------------------------------
    // "Flatten" T so that we have one unique_ptr
    // per Node type. E.g. if T = std::tuple<NodeA, NodeB>,
    // or T = std::tuple<std::tuple<NodeA>, std::tuple<NodeB,NodeC>> => flatten => NodeA,B,C
    //---------------------------------------------
    template <typename T>
    struct flatten_tuple_types; // produce std::tuple<NodeA, NodeB, NodeC> from layered

    // If T is just a node, treat it like a single-element tuple
    template <typename Node>
    struct flatten_tuple_types<std::tuple<Node>>
    {
        using type = std::tuple<Node>;
    };

    // If T is already a tuple-of-nodes, we can just use T directly
    //   but be careful if T might itself be a layered structure
    template <typename... Ts>
    struct flatten_tuple_types<std::tuple<Ts...>>
    {
    private:
        // For each Ts, flatten if needed
        template <typename X>
        struct flatten_one : flatten_tuple_types<std::tuple<X>> {};

        // If X is itself a tuple, flatten that
        template <typename... Sub>
        struct flatten_one<std::tuple<Sub...>> : flatten_tuple_types<std::tuple<Sub...>> {};

        // cat them
        template <typename... Xs>
        static auto cat_all(std::tuple<Xs...>)
        {
            return (std::tuple_cat(typename flatten_one<Xs>::type{}...));
        }

    public:
        using type = decltype(cat_all(std::declval<std::tuple<Ts...>>()));
    };

    template <typename T>
    using flatten_tuple_types_t = typename flatten_tuple_types<T>::type;


    // Now produce a single storage tuple: e.g. std::tuple<std::unique_ptr<NodeA>, ...>
    template <typename T>
    struct unique_ptr_storage
    {
    private:
        using flattened_nodes = flatten_tuple_types_t<T>;
        // e.g. if T = layered, we end up with NodeA, NodeB, NodeC in a single std::tuple<...>
    public:
        using type = std::conditional_t<
            std::tuple_size_v<flattened_nodes> == 0,
            std::tuple<>, // no nodes
            std::tuple<std::unique_ptr<std::tuple_element_t<0, flattened_nodes>>,
            std::unique_ptr<std::tuple_element_t<1, flattened_nodes>>,
            // ...
            std::unique_ptr<std::tuple_element_t<std::tuple_size_v<flattened_nodes>-1, flattened_nodes>>
            >
        >;
        // In practice, you want a pack expansion. Let's do that more properly with a helper:
    };

    // Actually let's define a short helper using a pack expansion:
    template <typename... Ns>
    struct make_unique_ptr_tuple
    {
        using type = std::tuple<std::unique_ptr<Ns>...>;
    };

    // We can combine them:
    template <typename T>
    struct unique_ptr_storage2
    {
    private:
        using flat = flatten_tuple_types_t<T>; // NodeA, NodeB, NodeC...
        template <typename> struct to_uptrs;
        template <typename... Ns>
        struct to_uptrs<std::tuple<Ns...>>
        {
            using type = std::tuple<std::unique_ptr<Ns>...>;
        };
    public:
        using type = typename to_uptrs<flat>::type;
    };

    template <typename T>
    using unique_ptr_storage_t = typename unique_ptr_storage2<T>::type;

    //=========================================================
    // A compile-time "for_each_type" that calls a templated functor
    //   - We do not pass any runtime object, we pass the type in a template call
    //=========================================================
    namespace detail
    {
        template <typename Tuple, typename Func, std::size_t... I>
        void for_each_type_impl(Func&& f, std::index_sequence<I...>)
        {
            // We'll call f.template operator()<std::tuple_element_t<I, Tuple>>()... 
            // or a lambda that has a template operator()<Type>()...
            ((void)(f.template operator() < std::tuple_element_t<I, Tuple> > ()), ...);
        }
    }

    template <typename Tuple, typename Func>
    void for_each_type(Func&& f)
    {
        detail::for_each_type_impl<Tuple>(
            std::forward<Func>(f),
            std::make_index_sequence<std::tuple_size_v<Tuple>>{}
        );
    }

    //=========================================================
    // The Pipeline
    //=========================================================
    template <typename TopoResult, typename ResourceT>
    class Pipeline
    {
    public:
        using topo_type = TopoResult;            // might be linear or layered
        using storage_type = unique_ptr_storage_t<topo_type>;

        Pipeline() {
            resource_ = std::make_unique<ResourceT>();
        }

        ResourceT& getResource() { return *resource_; }
        const ResourceT& getResource() const { return *resource_; }

        // setNode / emplaceNode
        template <typename NodeType>
        void setNode(std::unique_ptr<NodeType> ptr)
        {
            // For NodeType, find that unique_ptr<NodeType> in storage_type
            std::get<std::unique_ptr<NodeType>>(nodes_) = std::move(ptr);
        }

        template <typename NodeType, typename... Args>
        void emplaceNode(Args&&... args)
        {
            auto p = std::make_unique<NodeType>(std::forward<Args>(args)...);
            setNode(std::move(p));
        }

        // The main "run"
        void run()
        {
            // Distinguish layered or linear at compile time
            if constexpr (is_tuple_of_tuples_v<topo_type>) {
                // layered => each element is a sub-tuple of node types
                runLayered<topo_type>();
            }
            else {
                // linear => direct tuple of node types
                runLinear<topo_type>();
            }
        }

    private:
        std::unique_ptr<ResourceT> resource_;
        storage_type               nodes_; // e.g. std::tuple<std::unique_ptr<NodeA>, NodeB, ...>

    private:
        //------------------------------------------------
        // runLinear<SomeTupleOfNodes> => expands each Node
        //------------------------------------------------
        template <typename NodeTuple>
        void runLinear()
        {
            // NodeTuple might be e.g. std::tuple<NodeA, NodeB, NodeC>.
            // We'll do a compile-time for-each of those node types.
            for_each_type<NodeTuple>([this]
                <typename NodeT>() // NodeT is e.g. NodeA
            {
                auto& uptr = std::get<std::unique_ptr<NodeT>>(nodes_);
                if (uptr) {
                    uptr->execute();
                }
            });
        }

        //------------------------------------------------
        // runLayered<LayeredTuple> => each element is itself a tuple of Node types
        // e.g. std::tuple<std::tuple<NodeA>, std::tuple<NodeB, NodeC>, std::tuple<NodeD>>
        // We'll do a compile-time for-each to run runLinear on each sub-tuple
        //------------------------------------------------
        template <typename LayeredTuple>
        void runLayered()
        {
            // for each layer type => runLinear on that layer
            for_each_type<LayeredTuple>([this]
                <typename Layer>() // Layer is e.g. std::tuple<NodeB, NodeC>
            {
                runLinear<Layer>();
            });
        }
    };
} // end namespace lux::cxx