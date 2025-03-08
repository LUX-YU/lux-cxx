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

#include <tuple>
#include <memory>
#include <future>
#include <array>
#include <type_traits>
#include <cassert>
#include <iostream>

#include "ThreadPool.hpp" // For your thread pool
#include "lux/cxx/compile_time/computer_pipeline.hpp" // gather_all_outputs, find_dep_map, etc. (the new version above)

namespace lux::cxx
{
    /**
     * ParallelComputerPipeline<LayeredNodes, AllDeps>
     *   - Intended for *layered* topologies: a std::tuple of sub-tuples,
     *   - Each layer is executed in parallel among the nodes in that layer,
     *   - Returns false if any node fails in any layer.
     *
     * NOTE: This pipeline does not currently attempt to skip subsequent layers
     *       if an earlier layer fails, but we can detect that by storing the result.
     *       (We do a short-circuit with a boolean.)
     */
    template <typename LayeredNodes, typename AllDeps>
    class ParallelComputerPipeline
    {
    public:
        // Basic static check: a layered topology is typically a tuple-of-tuples
        static_assert(std::tuple_size_v<LayeredNodes> > 0,
            "LayeredNodes must be a non-empty tuple of sub-tuples (layers).");

    private:
        //--------------------------------------------------------------------------
        // 1) Flatten all sub-tuples -> a single list of node types -> data storage
        //--------------------------------------------------------------------------
        template <typename TupleOfTuples>
        struct flatten_tuple_of_tuples;

        template <typename... Layers>
        struct flatten_tuple_of_tuples<std::tuple<Layers...>>
        {
        private:
            // For each Layers (which is a std::tuple<NodeX, NodeY,...>), we cat them
            template <typename U> struct identity { using type = U; };
            static auto catAll(Layers...)
                -> decltype(std::tuple_cat(typename identity<Layers>::type{}...));
        public:
            using type = decltype(catAll(std::declval<Layers>()...));
        };

        using FlattenedNodes = typename flatten_tuple_of_tuples<LayeredNodes>::type;

        // Gather node outputs from FlattenedNodes
        template <typename... Ns>
        struct gather_impl
        {
            using big = typename gather_all_outputs<Ns...>::type;
            using data_t = typename pairs_to_data<big>::type;
        };

        template <typename T> struct gather_nodes_outs;
        template <typename... Ns>
        struct gather_nodes_outs<std::tuple<Ns...>>
        {
            using data_t = typename gather_impl<Ns...>::data_t;
        };

        using DataStorage = typename gather_nodes_outs<FlattenedNodes>::data_t;

        // Node pointer storage
        template <typename T> struct node_ptrs_impl;
        template <typename... Ns>
        struct node_ptrs_impl<std::tuple<Ns...>>
        {
            using type = std::tuple<std::unique_ptr<Ns>...>;
        };
        using NodePtrStorage = typename node_ptrs_impl<FlattenedNodes>::type;

    public:
        //--------------------------------------------------------------------------
        // 2) Constructor: specify how many threads in the pool
        //--------------------------------------------------------------------------
        explicit ParallelComputerPipeline(std::size_t pool_size)
            : pool_(pool_size)
        {
        }

        //--------------------------------------------------------------------------
        // 3) Emplace a node
        //--------------------------------------------------------------------------
        template <typename NodeT, typename... Args>
        NodeT* emplaceNode(Args&&... args)
        {
            auto up = std::make_unique<NodeT>(std::forward<Args>(args)...);
            constexpr size_t idx = findNodeIndex<NodeT>();
            std::get<idx>(nodes_) = std::move(up);
            return static_cast<NodeT*>(std::get<idx>(nodes_).get());
        }

        //--------------------------------------------------------------------------
        // 4) Run: each layer in parallel, returns false if any node fails
        //--------------------------------------------------------------------------
        bool run()
        {
            // We'll short-circuit on the first failing layer
            return runAllLayers(std::make_index_sequence<std::tuple_size_v<LayeredNodes>>{});
        }

        //--------------------------------------------------------------------------
        // 5) getInputRef / getOutputRef
        //--------------------------------------------------------------------------
        template <typename NodeT, std::size_t InLoc, typename T>
        T& getInputRef()
        {
            using dep_map_t = typename find_dep_map<NodeT, InLoc, AllDeps>::type;
            static_assert(!std::is_void_v<dep_map_t>,
                "No dependency found. Check your DepList for (NodeT, InLoc).");

            using SourceNode = typename dep_map_t::source_node_t;
            static constexpr size_t sourceLoc = dep_map_t::source_out_loc;

            static constexpr size_t idx = findIndex<SourceNode, sourceLoc>();
            return std::get<idx>(data_);
        }

        template <typename NodeT, std::size_t OutLoc, typename T>
        T& getOutputRef()
        {
            static constexpr size_t idx = findIndex<NodeT, OutLoc>();
            return std::get<idx>(data_);
        }

    private:
        DataStorage    data_;
        NodePtrStorage nodes_;
        ThreadPool     pool_;

    private:
        //--------------------------------------------------------------------------
        // 6) findNodeIndex, findIndex<...>, flattening logic
        //--------------------------------------------------------------------------
        template <typename NodeT>
        static constexpr size_t findNodeIndex()
        {
            return findIndexIn<NodeT, FlattenedNodes>();
        }

        template <typename T, typename Tup> struct index_in;
        template <typename T>
        struct index_in<T, std::tuple<>>
        {
            static constexpr size_t value = size_t(-1);
        };
        template <typename T, typename Head, typename...Tail>
        struct index_in<T, std::tuple<Head, Tail...>>
        {
            static constexpr bool eq = std::is_same_v<T, Head>;
            static constexpr size_t nxt = index_in<T, std::tuple<Tail...>>::value;
            static constexpr size_t value = eq ? 0 : (nxt == size_t(-1) ? size_t(-1) : nxt + 1);
        };

        template <typename T, typename Tup>
        static constexpr size_t findIndexIn()
        {
            constexpr size_t r = index_in<T, Tup>::value;
            static_assert(r != size_t(-1), "Node T not found in FlattenedNodes.");
            return r;
        }

        template <typename NW, std::size_t outLoc>
        static constexpr size_t findIndex()
        {
            using bigPairTuple = typename gather_all_outputs_from<FlattenedNodes>::type;
            constexpr size_t r = find_data_index_v<bigPairTuple, NW, outLoc>;
            static_assert(r != size_t(-1),
                "No matching (NW, outLoc) in data storage.");
            return r;
        }

        template <typename> struct gather_all_outputs_from;
        template <typename... Ns>
        struct gather_all_outputs_from<std::tuple<Ns...>>
        {
            using type = typename gather_all_outputs<Ns...>::type;
        };

        //--------------------------------------------------------------------------
        // 7) runAllLayers: go layer by layer
        //--------------------------------------------------------------------------
        template <size_t... Is>
        bool runAllLayers(std::index_sequence<Is...>)
        {
            bool all_ok = true;
            // For each layer, run them in parallel => short-circuit the first fail
            bool dummy[] = {
                (all_ok = all_ok && runOneLayer<std::tuple_element_t<Is, LayeredNodes>>())...
            };
            (void)dummy;
            return all_ok;
        }

        // runOneLayer => parallel execution of each node in that layer
        template <typename LayerT>
        bool runOneLayer()
        {
            static constexpr size_t N = std::tuple_size_v<LayerT>;
            std::array<std::future<bool>, N> futures{};

            // Launch each node in the layer
            launchNodes<LayerT>(futures, std::make_index_sequence<N>{});

            // Wait for them. If any fails, we mark fail.
            bool layer_ok = true;
            for (auto& f : futures) {
                bool node_ok = f.get();
                layer_ok &= node_ok;
            }
            return layer_ok;
        }

        template <typename LayerT, size_t... I>
        void launchNodes(std::array<std::future<bool>, std::tuple_size_v<LayerT>>& futures,
            std::index_sequence<I...>)
        {
            // expand pack => schedule each node
            (..., launchOne< std::tuple_element_t<I, LayerT> >(futures[I]));
        }

        void launchOne(std::future<bool>& slot, auto runFunc)
        {
            slot = pool_.submit(runFunc);
        }

        template <typename N>
        void launchOne(std::future<bool>& slot)
        {
            slot = pool_.submit(
                [this]() {
                    return runNode<N>();
                }
            );
        }

        // runNode => build in/out, call execute
        template <typename N>
        bool runNode()
        {
            constexpr size_t idx = findNodeIndex<N>();
            auto& uptr = std::get<idx>(nodes_);

            auto in = build_in_param<N>();
            auto out = build_out_param<N>();

            return uptr->execute(in, out);
        }

        //--------------------------------------------------------------------------
        // 8) build_in_param / build_out_param
        //--------------------------------------------------------------------------
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
            return in_tuple_t{
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
    };

} // namespace lux::cxx
