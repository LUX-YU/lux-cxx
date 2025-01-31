#pragma once
#include "ThreadPool.hpp"
#include <lux/cxx/compile_time/computer_graph.hpp>
#include <future>
#include <vector>

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
        //    DataStorage is a tuple of value for each (Node, out_binding)
        template <typename... Ns>
        struct gather_impl
        {
            using big    = typename gather_all_outputs<Ns...>::type;
            using data_t = typename pairs_to_data<big>::type;
        };

        // Extract nodes from Flattened and apply gather_impl
        template <typename Tup> 
        struct gather_nodes_outs;

        template <typename... Ns>
        struct gather_nodes_outs<std::tuple<Ns...>>
        {
            using data_t = typename gather_impl<Ns...>::data_t;
        };

        // 这里就是存储全部输出的  的 tuple
        using DataStorage = typename gather_nodes_outs<Flattened>::data_t;

        // 存储节点指针: std::tuple< std::unique_ptr<NodeA>, std::unique_ptr<NodeB>, ...>
        template <typename... Ns>
        struct node_ptrs
        {
            using type = std::tuple<std::unique_ptr<Ns>...>;
        };
        template <typename Tup> struct node_ptrs_impl;
        template <typename... Ns>
        struct node_ptrs_impl<std::tuple<Ns...>>
        {
            using type = std::tuple<std::unique_ptr<Ns>...>;
        };
        using NodePtrStorage = typename node_ptrs_impl<Flattened>::type;

        //==============================================================
        // Constructors
        //==============================================================
        template<typename... Args>
        ParallelPipeline(size_t pool_size, Args&&... args)
            : pool_(pool_size)
        {
            resource_ = std::make_unique<Resource>(std::forward<Args>(args)...);
        }

        ParallelPipeline(size_t pool_size, std::unique_ptr<Resource> resource)
            : pool_(pool_size), resource_(std::move(resource))
        {
        }

        // Access the resource
        Resource& getResource() { return *resource_; }

        //==============================================================
        // Emplace a node into the pipeline
        //==============================================================
        template <typename NodeT, typename... Args>
        NodeT* emplaceNode(Args&&... args)
        {
            auto up = std::make_unique<NodeT>(std::forward<Args>(args)...);
            constexpr size_t idx = findNodeIndex<NodeT>();
            std::get<idx>(nodes_) = std::move(up);
            return static_cast<NodeT*>(std::get<idx>(nodes_).get());
        }

        //==============================================================
        // Run the pipeline (parallel, layered only)
        //==============================================================
        void run()
        {
            static_assert(isLayered, "ParallelPipeline only supports layered topologies.");
            runLayered<TopoResult>();
        }

        //==============================================================
        // getInputRef / getOutputRef
        //  - 同串行 Pipeline 中一样，对  做访问
        //==============================================================
        template <typename NodeT, std::size_t InLoc, typename T>
        T& getInputRef()
        {
            using map = find_dep_map_t<InLoc, typename NodeT::mapping_tuple>;
            using DepNode = typename map::dependency_node_t;
            static constexpr size_t depLoc = map::dep_output_location;

            // Find the index of the dependency node's output in DataStorage
            constexpr size_t I = findIndex<DepNode, depLoc>();
            return std::get<I>(data_); // T&
        }

        template <typename NodeT, std::size_t OutLoc, typename T>
        T& getOutputRef()
        {
            constexpr size_t I = findIndex<NodeT, OutLoc>();
            return std::get<I>(data_); // T&
        }

    private:
        //==============================================================
        // Internal Storage
        //==============================================================
        std::unique_ptr<Resource> resource_;
        DataStorage               data_;
        NodePtrStorage            nodes_;
        ThreadPool                pool_;  // for parallel execution

    private:
        //==============================================================
        // 下面是与串行 Pipeline 类似的辅助元编程: 
        //  findNodeIndex, findIndex<>, gather_all_outputs_from<>, etc.
        //==============================================================

        // 查找 NodeT 在 Flattened 里的下标
        template <typename NodeT>
        static constexpr size_t findNodeIndex()
        {
            return findIndexIn<NodeT, Flattened>();
        }

        // index_in< T, tuple<...> >
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

        template <typename T, typename Tup>
        static constexpr size_t findIndexIn()
        {
            constexpr size_t r = index_in<T, Tup>::value;
            static_assert(r != size_t(-1), "Node T not found in Flattened tuple");
            return r;
        }

        // 查找 (NodeWanted, outLoc) 在 data_ 里的下标
        template <typename NW, std::size_t outLoc>
        static constexpr size_t findIndex()
        {
            using bigPairTuple = typename gather_all_outputs_from<Flattened>::type;
            constexpr size_t r = find_data_index_v<bigPairTuple, NW, outLoc>;
            static_assert(r != size_t(-1), "Corresponding storage for (NodeWanted, outLoc) not found");
            return r;
        }

        // 收集 Flattened 里所有节点的 outputs 合并成一个 NodeOutPair tuple
        template <typename> struct gather_all_outputs_from;
        template <typename... Ns>
        struct gather_all_outputs_from<std::tuple<Ns...>>
        {
            using type = typename gather_all_outputs<Ns...>::type;
        };

        //==============================================================
        // 构造 in/out param：与串行 Pipeline 同理
        //==============================================================
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
                // N::in_loc_seq[I] 是第I个 input binding 的 location
                this->template getInputRef<
                    N,
                    N::in_loc_seq[I],
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
            return out_tuple_t {
                this->template getOutputRef<
                    N,
                    N::out_loc_seq[I],
                    std::remove_reference_t<std::tuple_element_t<I, out_tuple_t>>
                >()...
            };
        }

        //==============================================================
        // 1) runLayered(): 按层次并行执行
        //==============================================================
        template <typename LayeredTuple>
        void runLayered()
        {
            // LayeredTuple 形如 std::tuple< std::tuple<NodeA,NodeB>, std::tuple<NodeC>, ... >
            // 对每一层并行执行
            runLayeredImpl(std::make_index_sequence<std::tuple_size_v<LayeredTuple>>{});
        }

        template<size_t... I>
        void runLayeredImpl(std::index_sequence<I...>)
        {
            bool ok = true;
            bool dummy[] = { (ok = ok && runParallelLayer<std::tuple_element_t<I, TopoResult>>())... };
            (void)dummy;
        }

        //==============================================================
        // 2) runParallelLayer<Layer>：对 Layer 内的节点 并行 调度
        //==============================================================
        template <typename NodeTuple>
        bool runParallelLayer()
        {
            static constexpr size_t numNodes = std::tuple_size_v<NodeTuple>;
            std::array<std::future<bool>, numNodes> futures;

            // 这里对 NodeTuple = (NodeA, NodeB, ...) 做编译期遍历
            tuple_traits::for_each_type<NodeTuple>(
                [this, &futures]<typename NodeT, size_t I>()
                {
                    // 并行提交
                    auto fut = pool_.submit([this]() {
                        return runOneNode<NodeT>();
                    });
                    futures[I] = std::move(fut);
                }
            );

            bool layer_ok = true;
            // 等所有节点完成
            for (auto &f : futures) {
                layer_ok &= f.get();
            }

            return layer_ok;
        }

        //==============================================================
        // 3) runOneNode<NodeT>：对 单个节点 做 in/out 构造 + execute
        //==============================================================
        template <typename N>
        bool runOneNode()
        {
            constexpr size_t idx = findNodeIndex<N>();
            auto& uptr = std::get<idx>(nodes_);

            auto in  = build_in_param<N>();
            auto out = build_out_param<N>();

            return uptr->execute(in, out);
        }
    };
} // namespace lux::cxx
