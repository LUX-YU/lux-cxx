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

#include "computer_node.hpp"       // for NodeBase, in_binding, out_binding, etc.
#include "computer_node_sort.hpp"  // for dependency_map, etc.
#include <tuple>
#include <memory>
#include <cassert>
#include <iostream>
#include <type_traits>

/**
 * Restored "failure-exit" logic:
 *   - if a node returns false on execute(...), the pipeline returns false (stops).
 *   - The linear or layered calls short-circuit once `ok` is false.
 */

namespace lux::cxx
{
    // A concept to detect if T is recognized by std::tuple_size as a tuple-like type.
    template<typename T>
    concept TupleLike = requires {
        typename std::tuple_size<T>::type;
    };

    //--------------------------------------------------------------------------
    // 1) NodeOutPair / node_outputs / gather_all_outputs
    //--------------------------------------------------------------------------
    template <typename Node, typename OB>
    struct NodeOutPair
    {
        using node_type = Node;
        using out_binding_type = OB;
    };

    template <typename Node>
    struct node_outputs
    {
    private:
        using outdesc = typename Node::output_descriptor;
        using outtuple = typename outdesc::bindings_tuple_t;

        template <typename OB>
        using pair_ = NodeOutPair<Node, OB>;

        template <typename... Obs>
        static auto doCat(std::tuple<Obs...>)
        {
            return std::tuple< pair_<Obs>... >{};
        }
    public:
        using type = decltype(doCat(std::declval<outtuple>()));
    };

    template <typename... Nodes>
    struct gather_all_outputs
    {
    private:
        template <typename... Ts>
        static auto catAll()
        {
            return std::tuple_cat(typename node_outputs<Ts>::type{}...);
        }
    public:
        using type = decltype(catAll<Nodes...>());
    };

    // NodeOutPair -> the actual data type
    template <typename Pair>
    struct pair_to_val
    {
        using OB = typename Pair::out_binding_type;
        using type = typename OB::value_t;
    };

    // Convert a tuple<NodeOutPair<...>, ...> -> tuple<value1, value2, ...>
    template <typename TuplePairs>
    struct pairs_to_data;
    template <typename... Ps>
    struct pairs_to_data<std::tuple<Ps...>>
    {
        using type = std::tuple<typename pair_to_val<Ps>::type ...>;
    };

    //--------------------------------------------------------------------------
    // 2) Finding (NodeWanted, outLoc) in data storage
    //--------------------------------------------------------------------------
    template <typename NOPair, typename NodeWanted, std::size_t OutLoc>
    struct match_pair : std::false_type {};

    template <typename N, typename OB, typename NW, std::size_t L>
    struct match_pair<NodeOutPair<N, OB>, NW, L>
    {
        static constexpr bool value =
            (std::is_same_v<N, NW> && (OB::location == L));
    };

    template <typename Tup, typename NW, std::size_t Loc>
    struct find_data_index;

    template <typename NW, std::size_t Loc>
    struct find_data_index<std::tuple<>, NW, Loc>
    {
        static constexpr size_t value = size_t(-1);
    };

    template <typename Head, typename...Tail, typename NW, std::size_t L>
    struct find_data_index<std::tuple<Head, Tail...>, NW, L>
    {
    private:
        static constexpr bool matched = match_pair<Head, NW, L>::value;
        static constexpr size_t nxt = find_data_index<std::tuple<Tail...>, NW, L>::value;
    public:
        static constexpr size_t value =
            matched ? 0 : (nxt == size_t(-1) ? size_t(-1) : nxt + 1);
    };

    template <typename Tup, typename NW, std::size_t L>
    inline constexpr size_t find_data_index_v = find_data_index<Tup, NW, L>::value;


    //--------------------------------------------------------------------------
    // 3) find_dep_map: external "NodeT, inLoc -> (SourceNode, SourceOutLoc)"
    //--------------------------------------------------------------------------
    template <typename NodeT, std::size_t inLoc, typename AllDeps>
    struct find_dep_map;

    template <typename NodeT, std::size_t inLoc, typename... Ms>
    struct find_dep_map<NodeT, inLoc, std::tuple<Ms...>>
    {
    private:
        template <typename M>
        static constexpr bool is_match =
            (std::is_same_v<typename M::target_node_t, NodeT> &&
                M::target_in_loc == inLoc);

        template <bool C, typename T> struct pick_if { using type = void; };
        template <typename T> struct pick_if<true, T> { using type = T; };

        template <typename Found, typename... Ts>
        struct scan { using type = Found; };

        template <typename Found, typename Head, typename...Tail>
        struct scan<Found, Head, Tail...>
        {
            static constexpr bool ok = is_match<Head>;
            using maybe = typename pick_if<ok, Head>::type;
            using type = typename scan<
                std::conditional_t<std::is_void_v<Found>, maybe, Found>,
                Tail...
            >::type;
        };

        using result = typename scan<void, Ms...>::type;
    public:
        using type = result; // 'void' if not found
    };


    //--------------------------------------------------------------------------
    // 4) ComputerPipeline: run in linear or layered mode, with failure-exit logic
    //--------------------------------------------------------------------------
    template <typename NodeList, typename AllDeps>
    class ComputerPipeline
    {
    public:
        using SortedOrLayeredNodes = typename topological_sort<NodeList, AllDeps>::type;

        // Build data storage from all outputs
        template <typename Tup> struct gather_nodes_outs;
        template <typename... Ns>
        struct gather_nodes_outs<std::tuple<Ns...>>
        {
            using big_pair_t = typename gather_all_outputs<Ns...>::type;
            using data_t     = typename pairs_to_data<big_pair_t>::type;
        };
        using DataStorage = typename gather_nodes_outs<SortedOrLayeredNodes>::data_t;

        // Node pointer storage
        template <typename Tup> struct node_ptrs_impl;
        template <typename... Ns>
        struct node_ptrs_impl<std::tuple<Ns...>>
        {
            using type = std::tuple< std::unique_ptr<Ns>... >;
        };
        using NodePtrStorage = typename node_ptrs_impl<SortedOrLayeredNodes>::type;

        ComputerPipeline() = default;

        // Emplace a node
        template <typename NodeT, typename... Args>
        NodeT* emplaceNode(Args&&... args)
        {
            auto up = std::make_unique<NodeT>(std::forward<Args>(args)...);
            constexpr size_t idx = findNodeIndex<NodeT>();
            std::get<idx>(nodes_) = std::move(up);
            return static_cast<NodeT*>(std::get<idx>(nodes_).get());
        }

        // The main run function => returns false if a node fails
        bool run()
        {
            return runImpl<SortedOrLayeredNodes>(
                std::make_index_sequence<std::tuple_size_v<SortedOrLayeredNodes>>{}
            );
        }

        // Access input references
        template <typename NodeT, std::size_t InLoc, typename T>
        T& getInputRef()
        {
            using dep_map_t = typename find_dep_map<NodeT, InLoc, AllDeps>::type;
            static_assert(!std::is_void_v<dep_map_t>,
                "No dependency found for (NodeT, InLoc). Check your DepList.");

            using SourceNode = typename dep_map_t::source_node_t;
            static constexpr size_t sourceLoc = dep_map_t::source_out_loc;

            static constexpr size_t i = findIndex<SourceNode, sourceLoc>();
            return std::get<i>(data_);
        }

        // Access output references
        template <typename NodeT, std::size_t OutLoc, typename T>
        T& getOutputRef()
        {
            static constexpr size_t i = findIndex<NodeT, OutLoc>();
            return std::get<i>(data_);
        }

    private:
        DataStorage    data_;
        NodePtrStorage nodes_;

    private:
        //--------------------------------------------------------------------------
        // Helpers to find node index and output index
        //--------------------------------------------------------------------------
        template <typename NodeT>
        static constexpr size_t findNodeIndex()
        {
            return findIndexIn<NodeT, SortedOrLayeredNodes>();
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
            static_assert(r != size_t(-1), "Node type T not found in node list.");
            return r;
        }

        template <typename NW, std::size_t outLoc>
        static constexpr size_t findIndex()
        {
            using bigPairTuple = typename gather_all_outputs_from<SortedOrLayeredNodes>::type;
            constexpr size_t r = find_data_index_v<bigPairTuple, NW, outLoc>;
            static_assert(r != size_t(-1), "No matching (Node, outLoc) in data storage.");
            return r;
        }

        template <typename> struct gather_all_outputs_from;
        template <typename... Ns>
        struct gather_all_outputs_from<std::tuple<Ns...>>
        {
            using type = typename gather_all_outputs<Ns...>::type;
        };

        //--------------------------------------------------------------------------
        // 5) runImpl: differentiate single node vs. sub-tuple layer
        //    returns bool => false if any node fails
        //--------------------------------------------------------------------------
        // base-case
        template <typename NodeTuple>
        bool runImpl(std::index_sequence<>)
        {
            return true;
        }

        // single-node element
        template <typename NodeTuple, size_t I0, size_t... IRest>
            requires (!TupleLike<std::tuple_element_t<I0, NodeTuple>>)
        bool runImpl(std::index_sequence<I0, IRest...>)
        {
            using N = std::tuple_element_t<I0, NodeTuple>;
            bool ok = runOne<N>();
            if (!ok) return false;
            return runImpl<NodeTuple>(std::index_sequence<IRest...>{});
        }

        // sub-tuple layer
        template <typename NodeTuple, size_t I0, size_t... IRest>
            requires TupleLike<std::tuple_element_t<I0, NodeTuple>>
        bool runImpl(std::index_sequence<I0, IRest...>)
        {
            using LayerT = std::tuple_element_t<I0, NodeTuple>;
            if (!runLayer<LayerT>()) return false;
            return runImpl<NodeTuple>(std::index_sequence<IRest...>{});
        }

        // runLayer => if any node in that layer fails => false
        template <typename LayerT>
        bool runLayer()
        {
            return runLinear<LayerT>();
        }

        // runOne => build in/out, call execute
        template <typename N>
        bool runOne()
        {
            constexpr size_t idx = findNodeIndex<N>();
            auto& uptr = std::get<idx>(nodes_);

            auto in = build_in_param<N>();
            auto out = build_out_param<N>();

            return uptr->execute(in, out);
        }

        //--------------------------------------------------------------------------
        // 6) runLinear => runs each node in a tuple in sequence
        //--------------------------------------------------------------------------
        template <typename NodeTuple>
        bool runLinear()
        {
            return runLinearImpl<NodeTuple>(std::make_index_sequence<std::tuple_size_v<NodeTuple>>{});
        }

        template <typename NodeTuple, size_t... I>
        bool runLinearImpl(std::index_sequence<I...>)
        {
            bool ok = true;
            bool dummy[] = { (ok = ok && runOne< std::tuple_element_t<I, NodeTuple> >())... };
            (void)dummy;
            return ok;
        }

        //--------------------------------------------------------------------------
        // 7) build_in_param / build_out_param
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
