#include <tuple>
#include <type_traits>
#include <memory>
#include <optional>
#include <iostream>
#include <cassert>
#include <string>
#include <array>

#include "tuple_traits.hpp"

// ==============================================================
 // 2) Node Basics: in_binding / out_binding / node_dependency_map / NodeBase
 // ==============================================================
namespace lux::cxx
{
    // Represents an input binding with a specific location and type
    template <std::size_t Loc, typename T>
    struct in_binding
    {
        static constexpr std::size_t location = Loc;
        using value_t = T;
    };

    // Represents an output binding with a specific location and type
    template <std::size_t Loc, typename T>
    struct out_binding
    {
        static constexpr std::size_t location = Loc;
        using value_t = T;
    };

    // Descriptor for a node, containing a tuple of bindings
    template <typename... Bindings>
    struct node_descriptor
    {
        using bindings_tuple_t = std::tuple<Bindings...>;
    };

    // Maps a node's input location to its dependency node and the output location of that dependency
    template <std::size_t InLoc, typename DepNode, std::size_t OutLoc>
    struct node_dependency_map
    {
        static constexpr std::size_t input_location = InLoc;
        using dependency_node_t = DepNode;
        static constexpr std::size_t dep_output_location = OutLoc;
    };

    template<typename Descriptor>
    class descriptor_to_params
    {
    private:
        static constexpr std::size_t N = std::tuple_size_v<typename Descriptor::bindings_tuple_t>;
        template<size_t... I>
        // to a tuple with references to the value types of the bindings
        static constexpr auto to_tuple(std::index_sequence<I...>)
            -> std::tuple<typename std::tuple_element_t<I, typename Descriptor::bindings_tuple_t>::value_t&...>;

    public:
        using type = decltype(to_tuple(std::make_index_sequence<N>{}));
    };

    template<typename Descriptor>
    class descriptor_to_locations
    {
    private:
        static constexpr std::size_t N = std::tuple_size_v<typename Descriptor::bindings_tuple_t>;

        template<size_t... I>
        static constexpr auto to_array(std::index_sequence<I...>) {
            return std::array<size_t, N>{std::tuple_element_t<I, typename Descriptor::bindings_tuple_t>::location...};
        }

    public:
        static constexpr std::array<size_t, N> loc_seq = to_array(std::make_index_sequence<N>{});
    };

    // CRTP (Curiously Recurring Template Pattern) base class for nodes
    // Derived classes should implement Derived::execute(Pipeline&) with the actual logic
    template<typename Derived,
        typename InDesc,
        typename OutDesc,
        typename... Mapping>
    struct NodeBase
    {
        using input_descriptor  = InDesc;
        using output_descriptor = OutDesc;
        using mapping_tuple     = std::tuple<Mapping...>;

        using in_param_t                    = typename descriptor_to_params<InDesc>::type;
        static constexpr auto in_loc_seq    = descriptor_to_locations<InDesc>::loc_seq;

        using out_param_t                   = typename descriptor_to_params<OutDesc>::type;
        static constexpr auto out_loc_seq   = descriptor_to_locations<OutDesc>::loc_seq;

        // Default execute method (can be overridden by derived classes)
        void execute(const in_param_t& in, out_param_t& out) {
			static_cast<Derived*>(this)->execute(in, out);
        }
    };
}

namespace lux::cxx
{
    // Extracts dependencies of a node
    template <typename Node>
    struct node_dependencies
    {
    private:
        using maps = typename Node::mapping_tuple;

        // Helper to extract the dependency node type from a node_dependency_map
        template <typename M> struct get_dep_node;

        template <std::size_t I, typename DN, std::size_t O>
        struct get_dep_node< node_dependency_map<I, DN, O> >
        {
            using type = DN;
        };

        // Generates a tuple of dependency nodes
        template <typename... Ms>
        static auto getAll(std::tuple<Ms...>)
        {
            return std::tuple<typename get_dep_node<Ms>::type...>{};
        }

    public:
        using type = decltype(getAll(std::declval<maps>()));
    };

    // Checks if all dependencies of NodeT are present in the Sorted tuple
    template <typename NodeT, typename Sorted>
    struct dependencies_in_sorted
    {
    private:
        using deps = typename node_dependencies<NodeT>::type;

        // Determines if type X is present in tuple Tup
        template <typename X, typename Tup>
        struct in_tuple;

        // Base case: X is not found in an empty tuple
        template <typename X>
        struct in_tuple<X, std::tuple<>> : std::false_type {};

        // Recursive case: Check head and then tail
        template <typename X, typename Head, typename...Tail>
        struct in_tuple<X, std::tuple<Head, Tail...>>
            : std::conditional_t<
            std::is_same_v<X, Head>,
            std::true_type,
            in_tuple<X, std::tuple<Tail...>>
            >
        {
        };

        // Checks if all dependencies are in the Sorted tuple
        template <typename... Ds>
        static constexpr bool checkAll(std::tuple<Ds...>)
        {
            return (... && in_tuple<Ds, Sorted>::value);
        }

    public:
        static constexpr bool value = checkAll(deps{});
    };

    // Implementation of topological sorting
    template <typename Unsorted, typename Sorted>
    struct topo_sort_impl
    {
    private:
        static constexpr bool empty = (std::tuple_size_v<Unsorted> == 0);

        // Finds the first node in TTuple whose dependencies are satisfied
        template <typename TTuple>
        struct find_satisfied;

        template <typename... Ns>
        struct find_satisfied<std::tuple<Ns...>>
        {
        private:
            // Checks if a node's dependencies are satisfied
            template <typename N>
            static constexpr bool is_ok = dependencies_in_sorted<N, Sorted>::value;

            // Helper to pick a type if condition is true
            template <bool C, typename T> struct pick_if { using type = void; };
            template <typename T> struct pick_if<true, T> { using type = T; };

            // Scans the list of nodes to find the first satisfied node
            template <typename Found, typename...Ts>
            struct scan_list { using type = Found; };

            template <typename Found, typename Head, typename...Tail>
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

        // Remove the found node from Unsorted and append it to Sorted
        using next_unsorted = tuple_remove_t<Unsorted, found>;
        using next_sorted = tuple_append_t<Sorted, found>;

        // Recursive case: Continue sorting with the updated tuples
        using step_case = typename topo_sort_impl<next_unsorted, next_sorted>::type;
    public:
        using type = std::conditional_t<
            empty,
            Sorted,
            std::conditional_t<found_any, step_case, std::false_type>
        >;

        // Static assertion to ensure sorting is possible (no cycles)
        static_assert(!std::is_same_v<type, std::false_type>,
            "Topological sorting failed: There is a cycle or unresolved dependencies.");
    };

    // Base case: When Unsorted is empty, return Sorted
    template <typename Sorted>
    struct topo_sort_impl<std::tuple<>, Sorted>
    {
        using type = Sorted;
    };

    // Initiates topological sorting on a tuple of nodes
    template <typename NodeTuple>
    struct topological_sort
    {
        using type = typename topo_sort_impl<NodeTuple, std::tuple<>>::type;
    };
}

// ==============================================================
 // 4) Layered Topological Sorting
 // ==============================================================
namespace lux::cxx
{
    // Flattens a tuple of tuples into a single tuple
    template <typename T>
    struct flatten_tuple_of_tuples
    {
        using type = std::tuple<T>;
    };

    // Specialization for tuples of tuples
    template <typename... Ts>
    struct flatten_tuple_of_tuples<std::tuple<Ts...>>
    {
    private:
        // Concatenates all flattened inner tuples
        template <typename... Xs>
        static auto catAll(std::tuple<Xs...>)
        {
            return std::tuple_cat(typename flatten_tuple_of_tuples<Xs>::type{}...);
        }
    public:
        using type = decltype(catAll(std::declval<std::tuple<Ts...>>()));
    };

    // Helper alias to simplify usage
    template <typename T>
    using flatten_tuple_of_tuples_t = typename flatten_tuple_of_tuples<T>::type;

    // Filters a tuple based on a predicate
    template <typename Tuple, template <typename> class Pred>
    struct filter_tuple;

    // Base case: Filtering an empty tuple results in an empty tuple
    template <template <typename> class Pred>
    struct filter_tuple<std::tuple<>, Pred>
    {
        using type = std::tuple<>;
    };

    // Recursive case: Include or exclude the head based on the predicate
    template <typename Head, typename...Tail, template <typename> class Pred>
    struct filter_tuple<std::tuple<Head, Tail...>, Pred>
    {
    private:
        using tail_filtered = typename filter_tuple<std::tuple<Tail...>, Pred>::type;
        static constexpr bool keep = Pred<Head>::value;
    public:
        using type = std::conditional_t<
            keep,
            decltype(std::tuple_cat(std::declval<std::tuple<Head>>(), std::declval<tail_filtered>())),
            tail_filtered
        >;
    };

    // Helper alias to simplify usage
    template <typename T, template <typename> class P>
    using filter_tuple_t = typename filter_tuple<T, P>::type;

    // Removes elements from a tuple based on a predicate
    template <typename Tuple, template <typename> class Pred>
    struct remove_if_tuple;

    // Base case: Removing from an empty tuple results in an empty tuple
    template <template <typename> class Pred>
    struct remove_if_tuple<std::tuple<>, Pred>
    {
        using type = std::tuple<>;
    };

    // Recursive case: Remove or keep the head based on the predicate
    template <typename Head, typename...Tail, template <typename> class Pred>
    struct remove_if_tuple<std::tuple<Head, Tail...>, Pred>
    {
    private:
        using tail_removed = typename remove_if_tuple<std::tuple<Tail...>, Pred>::type;
        static constexpr bool rm = Pred<Head>::value;
    public:
        using type = std::conditional_t<
            rm,
            tail_removed,
            decltype(std::tuple_cat(std::declval<std::tuple<Head>>(), std::declval<tail_removed>()))
        >;
    };

    // Helper alias to simplify usage
    template <typename T, template <typename> class P>
    using remove_if_tuple_t = typename remove_if_tuple<T, P>::type;

    // Implementation of layered topological sorting
    template <typename Unsorted, typename SoFar>
    struct layered_topo_sort_impl
    {
    private:
        static constexpr bool empty = (std::tuple_size_v<Unsorted> == 0);
        using finish_case = SoFar;

        // Flatten the current layers to check dependencies
        using FlattenedSoFar = flatten_tuple_of_tuples_t<SoFar>;

        // Predicate to determine if a node can run based on current dependencies
        template <typename N>
        struct can_run_now {
            static constexpr bool value = dependencies_in_sorted<N, FlattenedSoFar>::value;
        };

        // Select the next group of nodes that can run
        using next_group = filter_tuple_t<Unsorted, can_run_now>;
        static constexpr bool group_empty = (std::tuple_size_v<next_group> == 0);

        // Predicate to remove nodes that can run now
        template <typename N>
        struct remove_if_can_run {
            static constexpr bool value = dependencies_in_sorted<N, FlattenedSoFar>::value;
        };

        // Remove nodes that can run from the unsorted tuple
        using next_unsorted = remove_if_tuple_t<Unsorted, remove_if_can_run>;
        static constexpr bool have_unsorted = (std::tuple_size_v<Unsorted> != 0);

        // Append the next group to the sorted layers
        using appended =
            decltype(std::tuple_cat(std::declval<SoFar>(), std::declval<std::tuple<next_group>>()));

        // Recursive case: Continue sorting with the updated tuples
        using step_case = typename layered_topo_sort_impl<next_unsorted, appended>::type;
    public:
        using type = std::conditional_t<
            empty,
            SoFar,
            std::conditional_t<(group_empty&& have_unsorted), std::false_type, step_case>
        >;

        // Static assertion to ensure sorting is possible (no cycles)
        static_assert(!std::is_same_v<type, std::false_type>,
            "Layered topological sorting failed: There is a cycle or unresolved dependencies.");
    };

    // Base case: When Unsorted is empty, return the sorted layers
    template <typename SoFar>
    struct layered_topo_sort_impl<std::tuple<>, SoFar>
    {
        using type = SoFar;
    };

    // Initiates layered topological sorting on a tuple of nodes
    template <typename NodeTuple>
    struct layered_topological_sort
    {
        using type = typename layered_topo_sort_impl<NodeTuple, std::tuple<>>::type;
    };
}

// ==============================================================
 // 5) Helpers: Detecting Whether It's a Tuple of Tuples
 // ==============================================================
namespace lux::cxx
{
    // Determines if a type is a tuple of tuples
    template <typename T>
    struct is_tuple_of_tuples : std::false_type {};

    template <typename... Layers>
    struct is_tuple_of_tuples<std::tuple<Layers...>>
    {
    private:
        static constexpr bool all_tuples = (... && is_tuple_v<Layers>);
    public:
        static constexpr bool value = (sizeof...(Layers) > 0) && all_tuples;
    };

    // Helper variable template
    template <typename T>
    inline constexpr bool is_tuple_of_tuples_v = is_tuple_of_tuples<T>::value;
}

// ==============================================================
 // 6) Node Output Storage: (Node, out_binding<Loc,T>) => std::optional<T>
 // ==============================================================
namespace lux::cxx
{
    // Pair structure to associate a node with its output binding
    template <typename Node, typename OB>
    struct NodeOutPair
    {
        using node_type = Node;
        using out_binding_type = OB;
    };

    // Extracts all output bindings from a node
    template <typename Node>
    struct node_outputs
    {
    private:
        using outdesc = typename Node::output_descriptor;
        using outtuple = typename outdesc::bindings_tuple_t;

        // Helper to create NodeOutPair for each output binding
        template <typename OB>
        using pair_ = NodeOutPair<Node, OB>;

        // Concatenates all NodeOutPairs into a single tuple
        template <typename... Obs>
        static auto doCat(std::tuple<Obs...>)
        {
            return std::tuple< pair_<Obs>... >{};
        }
    public:
        using type = decltype(doCat(std::declval<outtuple>()));
    };

    // Gathers outputs from multiple nodes into a single tuple of NodeOutPairs
    template <typename... Nodes>
    struct gather_all_outputs
    {
    private:
        // Concatenates all outputs from the nodes
        template <typename... Ts>
        static auto catAll()
        {
            return std::tuple_cat(typename node_outputs<Ts>::type{}...);
        }
    public:
        using type = decltype(catAll<Nodes...>());
    };

    // Transforms a NodeOutPair into a value of its value type
    template <typename Pair>
    struct pair_to_val
    {
        using OB = typename Pair::out_binding_type;
        using type = typename OB::value_t;
    };

    // Transforms a tuple of NodeOutPairs into a tuple of value types
    template <typename T> struct pairs_to_data;
    template <typename... Ps>
    struct pairs_to_data<std::tuple<Ps...>>
    {
        using type = std::tuple<typename pair_to_val<Ps>::type ...>;
    };

    // Matches a specific Node and output location within a NodeOutPair
    template <typename Pair, typename NodeWanted, std::size_t OutLoc>
    struct match_pair : std::false_type {};

    template <typename N, typename OB, typename NodeWanted, std::size_t L>
    struct match_pair<NodeOutPair<N, OB>, NodeWanted, L>
    {
        static constexpr bool value =
            (std::is_same_v<N, NodeWanted> && (OB::location == L));
    };

    // Finds the index of a specific Node and output location within a tuple of NodeOutPairs
    template <typename TuplePairs, typename NodeWanted, std::size_t Loc>
    struct find_data_index;

    // Base case: Not found
    template <typename NodeWanted, std::size_t Loc>
    struct find_data_index<std::tuple<>, NodeWanted, Loc>
    {
        static constexpr size_t value = size_t(-1);
    };

    // Recursive case: Check head and then tail
    template <typename Head, typename... Tail, typename NW, std::size_t L>
    struct find_data_index<std::tuple<Head, Tail...>, NW, L>
    {
    private:
        static constexpr bool matched = match_pair<Head, NW, L>::value;
        static constexpr size_t next = find_data_index<std::tuple<Tail...>, NW, L>::value;
    public:
        static constexpr size_t value =
            matched ? 0 : (next == size_t(-1) ? size_t(-1) : next + 1);
    };

    // Helper variable template
    template <typename Tup, typename NW, std::size_t L>
    inline constexpr size_t find_data_index_v = find_data_index<Tup, NW, L>::value;

    // Finds the dependency map for a given input location within mappings
    template <std::size_t InLoc, typename Maps>
    struct find_dep_map;

    // Base case: Not found
    template <std::size_t InLoc>
    struct find_dep_map<InLoc, std::tuple<>>
    {
        using type = void;
    };

    // Recursive case: Check head and then tail
    template <std::size_t InLoc, typename Head, typename...Tail>
    struct find_dep_map<InLoc, std::tuple<Head, Tail...>>
    {
        static constexpr bool match = (Head::input_location == InLoc);
        using next = typename find_dep_map<InLoc, std::tuple<Tail...>>::type;
        using type = std::conditional_t<match, Head, next>;
    };

    // Helper alias to simplify usage
    template <std::size_t L, typename M>
    using find_dep_map_t = typename find_dep_map<L, M>::type;
}

// ==============================================================
 // 7) Pipeline: Handles Linear or Layered Topological Results; Provides Node Data Storage
 // ==============================================================
namespace lux::cxx
{
    // Forward declaration of flatten_tuple_of_tuples
    template <typename T> struct flatten_tuple_of_tuples;

    // Pipeline class template
    template <typename TopoResult, typename Resource>
    class Pipeline
    {
    public:
        // 1) Determine if TopoResult is layered
        //    If layered: std::tuple< std::tuple<NodeA>, std::tuple<NodeB,NodeC>, ...>
        //    If linear: std::tuple<NodeA, NodeB, NodeC, ...>
        static constexpr bool isLayered = is_tuple_of_tuples_v<TopoResult>;

        // 2) Flatten the topology result to get a single list of nodes
        using Flattened = flatten_tuple_of_tuples_t<TopoResult>;

        // 3) Gather all output bindings from the flattened nodes into DataStorage
        //    DataStorage is a tuple of std::optional<T> for each (Node, out_binding)
        template <typename... Ns>
        struct gather_impl
        {
            using big = typename gather_all_outputs<Ns...>::type;
            using data_t = typename pairs_to_data<big>::type;
        };

        // Extract nodes from Flattened and apply gather_impl
        template <typename Tup> struct gather_nodes_outs;
        template <typename... Ns>
        struct gather_nodes_outs<std::tuple<Ns...>>
        {
            using data_t = typename gather_impl<Ns...>::data_t;
        };

        using DataStorage = typename gather_nodes_outs<Flattened>::data_t;

        // 4) Store node pointers as a tuple of std::unique_ptr<Node>
        template <typename... Ns>
        struct node_ptrs
        {
            using type = std::tuple< std::unique_ptr<Ns>... >;
        };
        template <typename Tup> struct node_ptrs_impl;
        template <typename... Ns>
        struct node_ptrs_impl<std::tuple<Ns...>>
        {
            using type = std::tuple< std::unique_ptr<Ns>... >;
        };
        using NodePtrStorage = typename node_ptrs_impl<Flattened>::type;

        // Constructors
        template<typename... Args>
        Pipeline(Args&&... args)
        {
            resource_ = std::make_unique<Resource>(std::forward<Args>(args)...);
        }

        Pipeline(std::unique_ptr<Resource> resource)
            : resource_(std::move(resource)) {
        }

        // Access the resource
        Resource& getResource() { return *resource_; }

        // Emplace a node into the pipeline
        template <typename NodeT, typename... Args>
        NodeT* emplaceNode(Args&&... args)
        {
            auto up = std::make_unique<NodeT>(std::forward<Args>(args)...);
            constexpr size_t idx = findNodeIndex<NodeT>();
            std::get<idx>(nodes_) = std::move(up);
            return static_cast<NodeT*>(std::get<idx>(nodes_).get());
        }

        // Run the pipeline
        void run()
        {
            if constexpr (isLayered) {
                runLayered<TopoResult>();
            }
            else {
                runLinear<TopoResult>();
            }
        }

        // Nodes can read input using this method
        // Example: getInputRef<NodeType, InputLocation, Type>()
        template <typename NodeT, std::size_t InLoc, typename T>
        T& getInputRef()
        {
            // Find the dependency map for the given input location
            using map = find_dep_map_t<InLoc, typename NodeT::mapping_tuple>;
            using DepNode = typename map::dependency_node_t;
            static constexpr size_t depLoc = map::dep_output_location;

            // Find the index of the dependency node's output in DataStorage
            constexpr size_t I = findIndex<DepNode, depLoc>();
            // auto& opt = std::get<I>(data_);
            // assert(opt.has_value()); // Ensure the dependency has been computed
            return std::get<I>(data_);
        }

        // Nodes can write output using this method
        // Example: getOutputRef<NodeType, OutputLocation, Type>()
        template <typename NodeT, std::size_t OutLoc, typename T>
        T& getOutputRef()
        {
            constexpr size_t I = findIndex<NodeT, OutLoc>();
            // auto& opt = std::get<i>(data_);
            // if (!opt.has_value()) {
            //     opt.emplace();
            // }
            return std::get<I>(data_);
        }

    private:
        std::unique_ptr<Resource> resource_; // Resource managed by the pipeline
        DataStorage               data_;      // Stores all (Node, outLoc) => std::optional<T>
        NodePtrStorage            nodes_;     // Stores pointers to the nodes

    private:
        // Finds the index of NodeT in the Flattened tuple
        template <typename NodeT>
        static constexpr size_t findNodeIndex()
        {
            return findIndexIn< NodeT, Flattened >();
        }

        // Helper struct to find the index of a type in a tuple
        template <typename T, typename Tup> struct index_in;
        template <typename T>
        struct index_in<T, std::tuple<>> { static constexpr size_t value = -1; };

        template <typename T, typename Head, typename...Tail>
        struct index_in<T, std::tuple<Head, Tail...>>
        {
        private:
            static constexpr bool eq = std::is_same_v<T, Head>;
            static constexpr size_t nxt = index_in<T, std::tuple<Tail...>>::value;
        public:
            static constexpr size_t value = eq ? 0 : (nxt == size_t(-1) ? size_t(-1) : nxt + 1);
        };

        // Helper function to retrieve the index at compile time with static assertion
        template <typename T, typename Tup>
        static constexpr size_t findIndexIn()
        {
            constexpr size_t r = index_in<T, Tup>::value;
            static_assert(r != size_t(-1), "Node T not found in Flattened tuple");
            return r;
        }

        // Finds the index of (NodeWanted, outLoc) in DataStorage
        template <typename NW, std::size_t outLoc>
        static constexpr size_t findIndex()
        {
            using bigPairTuple = typename gather_all_outputs_from<Flattened>::type;
            constexpr size_t r = find_data_index_v<bigPairTuple, NW, outLoc>;
            static_assert(r != size_t(-1), "Corresponding storage for (NodeWanted, outLoc) not found");
            return r;
        }

        // Gathers all outputs from Flattened into a single tuple of NodeOutPairs
        template <typename> struct gather_all_outputs_from;
        template <typename... Ns>
        struct gather_all_outputs_from<std::tuple<Ns...>>
        {
            using type = typename gather_all_outputs<Ns...>::type;
        };

    private:
        template <typename N>
        auto build_in_param()
        {
            constexpr size_t numInputs = std::tuple_size_v<typename N::in_param_t>;
            return build_in_param_impl<N>(std::make_index_sequence<numInputs>{});
        }

        template <typename N, size_t... I>
        auto build_in_param_impl(std::index_sequence<I...>)
        {
            using in_tuple_t = typename N::in_param_t;
            return in_tuple_t {
                getInputRef<N,
                            N::in_loc_seq[I],
                            std::remove_reference_t<std::tuple_element_t<I, in_tuple_t>> >()
                ...
            };
        }

        template <typename N>
        auto build_out_param()
        {
            constexpr size_t numOutputs = std::tuple_size_v<typename N::out_param_t>;
            return build_out_param_impl<N>(std::make_index_sequence<numOutputs>{});
        }

        template <typename N, size_t... I>
        auto build_out_param_impl(std::index_sequence<I...>)
        {
            using out_tuple_t = typename N::out_param_t;
            return out_tuple_t{
                getOutputRef<N,
                             N::out_loc_seq[I],
                             std::remove_reference_t<std::tuple_element_t<I, out_tuple_t>> >()
                ...
            };
        }

        // Executes nodes in a linear order
        template <typename NodeTuple>
        void runLinear()
        {
            runLinearImpl<NodeTuple>(std::make_index_sequence<std::tuple_size_v<NodeTuple>>{});
        }

        // Helper function to execute each node in the tuple by index
        template <typename NodeTuple, size_t... I>
        void runLinearImpl(std::index_sequence<I...>)
        {
            // Execute each node in sequence
            (runOne< std::tuple_element_t<I, NodeTuple> >(), ...);
        }

        // Executes a single node
        template <typename N>
        void runOne()
        {
            constexpr size_t idx = findNodeIndex<N>();
            auto& uptr = std::get<idx>(nodes_);

            auto in  = build_in_param<N>();
            auto out = build_out_param<N>();

            uptr->execute(in, out); // Calls Derived::execute(Pipeline&)
        }

        // Executes nodes in layered order
        template <typename LayeredTup>
        void runLayered()
        {
            // LayeredTup = std::tuple< std::tuple<NodeA>, std::tuple<NodeB,NodeC>, ...>
            runLayersImpl<LayeredTup>(std::make_index_sequence<std::tuple_size_v<LayeredTup>>{});
        }

        // Helper function to execute each layer
        template <typename LayeredTup, size_t... I>
        void runLayersImpl(std::index_sequence<I...>)
        {
            // Execute each layer sequentially
            (runLinear<std::tuple_element_t<I, LayeredTup> >(), ...);
        }
    };
}