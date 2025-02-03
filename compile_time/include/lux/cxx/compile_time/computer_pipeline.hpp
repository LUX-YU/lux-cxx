#pragma once
#include "computer_node.hpp"

// ==============================================================
 // ComputerPipeline: Handles Linear or Layered Topological Results; Provides Node Data Storage
 // ==============================================================
namespace lux::cxx
{
    // Forward declaration of flatten_tuple_of_tuples
    template <typename T> struct flatten_tuple_of_tuples;

    // ComputerPipeline class template
    template <typename TopoResult>
    class ComputerPipeline
    {
    public:
        // 1) Determine if TopoResult is layered
        //    If layered: std::tuple< std::tuple<NodeA>, std::tuple<NodeB,NodeC>, ...>
        //    If linear: std::tuple<NodeA, NodeB, NodeC, ...>
        static constexpr bool isLayered = is_tuple_of_tuples_v<TopoResult>;

        // 2) Flatten the topology result to get a single list of nodes
        using Flattened = flatten_tuple_of_tuples_t<TopoResult>;

        // 3) Gather all output bindings from the flattened nodes into DataStorage
        //    DataStorage is a tuple of value for each (Node, out_binding)
        template <typename... Ns>
        struct gather_impl
        {
            using big    = typename gather_all_outputs<Ns...>::type;
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
        ComputerPipeline() = default;

        // Emplace a node into the Computerpipeline
        template <typename NodeT, typename... Args>
        NodeT* emplaceNode(Args&&... args)
        {
            auto up = std::make_unique<NodeT>(std::forward<Args>(args)...);
            constexpr size_t idx = findNodeIndex<NodeT>();
            std::get<idx>(nodes_) = std::move(up);
            return static_cast<NodeT*>(std::get<idx>(nodes_).get());
        }

        // Run the Computerpipeline
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
        DataStorage               data_;      // Stores all (Node, outLoc) => value
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
                getInputRef<N, N::in_loc_seq[I],
                    std::remove_reference_t<std::tuple_element_t<I, in_tuple_t>> 
                >()...
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
                getOutputRef<N, N::out_loc_seq[I],
                    std::remove_reference_t<std::tuple_element_t<I, out_tuple_t>>
                >()...
            };
        }

        // Executes nodes in a linear order
        template <typename NodeTuple>
        bool runLinear()
        {
            return runLinearImpl<NodeTuple>(std::make_index_sequence<std::tuple_size_v<NodeTuple>>{});
        }

        // Helper function to execute each node in the tuple by index
        template <typename NodeTuple, size_t... I>
        bool runLinearImpl(std::index_sequence<I...>)
        {
            bool ok = true;

            bool dummy[] = { (ok = ok && runOne< std::tuple_element_t<I, NodeTuple> >())... };
            (void)dummy;

            return ok;
        }

        // Executes a single node
        template <typename N>
        bool runOne()
        {
            constexpr size_t idx = findNodeIndex<N>();
            auto& uptr = std::get<idx>(nodes_);

            auto in  = build_in_param<N>();
            auto out = build_out_param<N>();

            return uptr->execute(in, out);
        }

        // Executes nodes in layered order
        template <typename LayeredTup>
        bool runLayered()
        {
            // LayeredTup = std::tuple< std::tuple<NodeA>, std::tuple<NodeB,NodeC>, ...>
            return runLayersImpl<LayeredTup>(std::make_index_sequence<std::tuple_size_v<LayeredTup>>{});
        }

        // Helper function to execute each layer
        template <typename LayeredTup, size_t... I>
        bool runLayersImpl(std::index_sequence<I...>)
        {
            bool ok = true;
            // Execute each layer sequentially
            bool dummy[] = { (ok = ok && runLinear<std::tuple_element_t<I, LayeredTup>>())... };
            (void)dummy;

            return ok;
        }
    };
}