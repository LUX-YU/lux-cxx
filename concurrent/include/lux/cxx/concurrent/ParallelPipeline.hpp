#pragma once
#include "ThreadPool.hpp"
#include <lux/cxx/compile_time/computer_graph.hpp>

namespace lux::cxx
{
    template <typename TopoResult, typename Resource>
    class ParallelPipeline
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
        ParallelPipeline(size_t pool_size, Args&&... args)
        : pool_(pool_size){
            resource_ = std::make_unique<Resource>(std::forward<Args>(args)...);
        }

        ParallelPipeline(size_t pool_size, std::unique_ptr<Resource> resource)
            : pool_(pool_size), resource_(std::move(resource)) {
        }

        // Access the resource
        Resource& getResource() { return *resource_; }

        // Emplace a node into the pipeline
        template <typename NodeT, typename... Args>
        NodeT* emplaceNode(Args&&... args)
        {
            auto up                 = std::make_unique<NodeT>(std::forward<Args>(args)...);
            constexpr size_t idx    = findNodeIndex<NodeT>();
            std::get<idx>(nodes_)   = std::move(up);
            return static_cast<NodeT*>(std::get<idx>(nodes_).get());
        }

        // Run the pipeline
        void run()
        {
			static_assert(isLayered, "ParallelPipeline only support layered topologies");
            runLayered<TopoResult>();
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
            constexpr size_t i = findIndex<DepNode, depLoc>();
            auto& opt = std::get<i>(data_);
            assert(opt.has_value()); // Ensure the dependency has been computed
            return opt.value();
        }

        // Nodes can write output using this method
        // Example: getOutputRef<NodeType, OutputLocation, Type>()
        template <typename NodeT, std::size_t OutLoc, typename T>
        T& getOutputRef()
        {
            constexpr size_t i = findIndex<NodeT, OutLoc>();
            auto& opt = std::get<i>(data_);
            if (!opt.has_value()) {
                opt.emplace();
            }
            return opt.value();
        }

    private:
        std::unique_ptr<Resource> resource_; // Resource managed by the pipeline
        DataStorage               data_;      // Stores all (Node, outLoc) => std::optional<T>
        NodePtrStorage            nodes_;     // Stores pointers to the nodes
		ThreadPool				  pool_;      // Thread pool for parallel execution

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

        template <typename NodeTuple>
        void runParallelLayer()
        {
            // We'll collect futures so that we can wait for all nodes to finish
            // before proceeding to the next layer
            std::vector<std::future<void>> futures;

            // We do a compile-time iteration over NodeTuple = (NodeA, NodeB, NodeC, ...)
            tuple_traits::for_each_type<NodeTuple>(
                [this, &futures] <typename NodeT, size_t I>()
                {
                    auto& uptr = std::get<std::unique_ptr<NodeT>>(nodes_);
                    if (uptr) {
                        // Submit the node->execute() job to the thread pool
                        auto fut = pool_.submit(
                            [this, nodePtr = uptr.get()] {
                                nodePtr->execute(*this);
                            }
                        );
                        futures.push_back(std::move(fut));
                    }
                }
            );

            // Wait for all
            for (auto& f : futures) {
                f.get();
            }
        }

        //-------------------------------------------------------------
        // runLayered => each element of LayeredTuple is a sub-tuple of node types
        // => we run that sub-tuple in parallel, then wait, then proceed to next
        //-------------------------------------------------------------
        template <typename LayeredTuple>
        void runLayered()
        {
            // For each “layer” in LayeredTuple
            tuple_traits::for_each_type<LayeredTuple>(
                [this]<typename Layer, size_t I>()
                {
                    // Layer is e.g. std::tuple<NodeB, NodeC>
                    runParallelLayer<Layer>();
                }
            );
        }
    };
}