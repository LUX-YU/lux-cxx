#include <tuple>
#include <optional>
#include <memory>
#include <iostream>
#include <cassert>
#include <type_traits>

//--------------------------------------------------------------
// 1) Definitions from your snippet (simplified) 
//--------------------------------------------------------------
namespace lux::cxx
{
    // forward declarations
    template<typename T> struct is_tuple : std::false_type {};
    template<typename... Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};
    template<typename T> inline constexpr bool is_tuple_v = is_tuple<T>::value;

    template <std::size_t L, typename T> struct in_binding { static constexpr size_t location=L; using value_t=T; };
    template <std::size_t L, typename T> struct out_binding{ static constexpr size_t location=L; using value_t=T; };

    template <typename... B> struct node_descriptor { using bindings_tuple_t = std::tuple<B...>; };

    // node_dependency_map
    template <std::size_t InLoc, typename DepNode, std::size_t OutLoc>
    struct node_dependency_map
    {
        static constexpr std::size_t input_location  = InLoc;
        using dependency_node_t = DepNode;
        static constexpr std::size_t dep_output_location = OutLoc;
    };

    // NodeBase with CRTP 
    // (The version that has pipeline pointer is below, so let's keep minimal here.)
    template<typename Derived, typename InDesc, typename OutDesc, typename... Mapping>
    struct NodeBase
    {
        using input_descriptor  = InDesc;
        using output_descriptor = OutDesc;
        using mapping_tuple     = std::tuple<Mapping...>;

        void execute()
        {
            static_cast<Derived*>(this)->execute();
        }
    };

    //-----------------------------------------------------------------------
    // A small topological or layered sort is omitted here for brevity
    // We just assume we have a final "NodeList" or "LayerList."
    //-----------------------------------------------------------------------
}

//--------------------------------------------------------------
// 2) Make a NodeBase that can actually do getInput<loc> / getOutput<loc>
//--------------------------------------------------------------
namespace lux::cxx
{
    // We'll forward-declare a PipelineBase 
    struct PipelineBase
    {
        virtual ~PipelineBase() = default;
    };

    // A new NodeBase that references a PipelineBase
    // so each node can do pipeline->getDataSlot<Node,OutLoc>()
    template<typename Derived, typename InDesc, typename OutDesc, typename... Mapping>
    struct NodeBaseWithAccess
      : public NodeBase<Derived, InDesc, OutDesc, Mapping...>
    {
        // pointer to pipeline
        PipelineBase* pipeline_ = nullptr;

        // The node calls these in its execute()
        template <std::size_t Loc>
        auto& getInput()
        {
            using T = typename find_in_binding<Loc, InDesc>::value_t;
            // We also must discover which node produces this input 
            // => from the mapping_tuple
            // But let's do a simpler approach: 
            // Our pipeline will do the correct "wired" approach:
            // we'll add a method pipeline_->template getInputRef<Derived, Loc, T>() 
            // that does the compile-time logic.
            auto* self = static_cast<Derived*>(this);
            return self->template doGetInputRef<T, Derived, Loc>(pipeline_);
        }

        template <std::size_t Loc>
        auto& getOutput()
        {
            using T = typename find_out_binding<Loc, OutDesc>::value_t;
            auto* self = static_cast<Derived*>(this);
            return self->template doGetOutputRef<T, Derived, Loc>(pipeline_);
        }

    private:
        // Helpers that find an in_binding / out_binding in a node_descriptor
        template <std::size_t L, typename Desc>
        struct find_in_binding_impl { using type = void; };
        template <std::size_t L, typename... Bs>
        struct find_in_binding_impl<L, node_descriptor<Bs...>>
        {
        private:
            // scan each B
            template <typename B> struct match : std::false_type {};
            template <std::size_t LL, typename TT>
            struct match< in_binding<LL,TT> > : std::bool_constant<(LL==L)> {};

            // we want the B that matches
            template <typename Found, typename...Rest>
            struct pick;
            template <typename Found>
            struct pick<Found> { using type = Found; };
            template <typename Found, typename Head, typename...Tail>
            struct pick<Found, Head, Tail...>
            {
                using type = std::conditional_t<
                    match<Head>::value,
                    Head,
                    typename pick<Found, Tail...>::type
                >;
            };

        public:
            using type = typename pick<void, Bs...>::type;
        };
        template <std::size_t L, typename Desc>
        using find_in_binding = typename find_in_binding_impl<L,Desc>::type;

        // same for out_binding
        template <std::size_t L, typename Desc>
        struct find_out_binding_impl { using type = void; };
        template <std::size_t L, typename... Bs>
        struct find_out_binding_impl<L, node_descriptor<Bs...>>
        {
            private:
            template <typename B> struct match : std::false_type {};
            template <std::size_t LL, typename TT>
            struct match< out_binding<LL,TT> > : std::bool_constant<(LL==L)> {};

            template <typename Found, typename...Rest>
            struct pick;
            template <typename Found>
            struct pick<Found> { using type = Found; };
            template <typename Found, typename Head, typename...Tail>
            struct pick<Found, Head, Tail...>
            {
                using type = std::conditional_t<
                    match<Head>::value,
                    Head,
                    typename pick<Found, Tail...>::type
                >;
            };
            public:
            using type = typename pick<void, Bs...>::type;
        };
        template <std::size_t L, typename Desc>
        using find_out_binding = typename find_out_binding_impl<L,Desc>::type;
    };
}

//--------------------------------------------------------------
// 3) Implementation: Pipeline that stores data for each node's output 
//    and lets other nodes read it
//--------------------------------------------------------------
namespace lux::cxx
{
    // A "tag" to store (NodeType, out_binding<Loc,T>)
    template <typename Node, typename OB>
    struct NodeOutPair
    {
        using node_type = Node;
        using out_binding_type = OB;
    };

    // Collect all out_bindings from one Node
    template <typename Node>
    struct node_outputs_list
    {
    private:
        using out_desc_tuple = typename Node::output_descriptor::bindings_tuple_t;
        template <typename OB>
        using pair_ = NodeOutPair<Node, OB>;

        template <typename... Obs>
        static auto doCollect(std::tuple<Obs...>)
        {
            return std::tuple< pair_<Obs>... >{};
        }
    public:
        using type = decltype(doCollect(std::declval<out_desc_tuple>()));
    };

    // Flatten out_bindings of multiple nodes => big tuple of NodeOutPair<...>
    template <typename... Nodes>
    struct all_nodes_outputs
    {
        template <typename N>
        using single = typename node_outputs_list<N>::type;

        static auto cat()
        {
            return std::tuple_cat(std::declval<single<Nodes>>()...);
        }
        using type = decltype(cat());
    };

    // Transform NodeOutPair<Node, out_binding<Loc,T>> => std::optional<T>
    template <typename Pair>
    struct output_pair_to_storage
    {
        using outB = typename Pair::out_binding_type;
        using type = std::optional<typename outB::value_t>;
    };

    // Convert a tuple< NodeOutPair<...>, ...> into tuple< optional<...>, ...>
    template <typename PairsTuple> struct to_storage_tuple;
    template <typename... Pairs>
    struct to_storage_tuple<std::tuple<Pairs...>>
    {
        using type = std::tuple< typename output_pair_to_storage<Pairs>::type ...>;
    };

    template <typename T>
    using to_storage_tuple_t = typename to_storage_tuple<T>::type;

    // Now for indexing:
    // We want find_data_index<(NodeWanted, LocWanted)> => index in the big data tuple
    template <typename Pair, typename NodeWanted, std::size_t LocWanted>
    struct matches_pair : std::false_type {};

    template <typename Node, typename OB, typename NodeWanted, std::size_t LocWanted>
    struct matches_pair< NodeOutPair<Node,OB>, NodeWanted, LocWanted >
    {
    private:
        static constexpr bool same_node = std::is_same_v<Node, NodeWanted>;
        static constexpr bool same_loc  = (OB::location == LocWanted);
    public:
        static constexpr bool value = (same_node && same_loc);
    };

    // Search in a pack
    template <typename PairsTuple, typename NodeWanted, std::size_t LocWanted>
    struct find_data_index;

    template <typename NodeWanted, std::size_t LocWanted>
    struct find_data_index<std::tuple<>, NodeWanted, LocWanted>
    {
        static constexpr std::size_t value = size_t(-1);
    };

    template <typename Head, typename... Tail, typename NodeWanted, std::size_t LocWanted>
    struct find_data_index<std::tuple<Head, Tail...>, NodeWanted, LocWanted>
    {
        static constexpr bool match = matches_pair<Head, NodeWanted, LocWanted>::value;
        static constexpr std::size_t next = find_data_index<std::tuple<Tail...>, NodeWanted, LocWanted>::value;
        static constexpr std::size_t value = match ? 0 : (next == size_t(-1) ? size_t(-1) : next+1);
    };

    template <typename PairsTuple, typename NodeWanted, std::size_t LocWanted>
    inline constexpr std::size_t find_data_index_v = find_data_index<PairsTuple, NodeWanted, LocWanted>::value;

    //--------------------------------------------------------------
    // A pipeline base class so that Node can store pipeline_ pointer
    //--------------------------------------------------------------
    struct PipelineBase
    {
        virtual ~PipelineBase() = default;
    };

    //--------------------------------------------------------------
    // The actual Pipeline, templated by NodeList (layered or linear)
    // + Resource
    //--------------------------------------------------------------
    template <typename NodeList, typename ResourceT>
    class Pipeline : public PipelineBase
    {
    public:
        // Step1: Flatten NodeList into a single list of node types. 
        //        E.g. if layered => flatten.
        //        We'll just assume user has that done. 
        //        For demonstration, let's define a "extract_nodes" for layered or linear.
        using all_nodes = typename extract_all_nodes<NodeList>::type;

        // Step2: all_nodes_outputs => tuple of NodeOutPair<...>
        using all_outputs_tuple = typename all_nodes_outputs< all_nodes_as_types<all_nodes>... >::type;
            // ^ we'll define a helper all_nodes_as_types<all_nodes>
        // Step3: transform => data_ is a tuple< optional<T>... >
        using data_storage_t = to_storage_tuple_t<all_outputs_tuple>;

        // We'll hold an instance of ResourceT, plus a node container, plus data_.
        Pipeline()
        {
            resource_ = std::make_unique<ResourceT>();
        }

        ResourceT& getResource() { return *resource_; }

        // We'll hold each node in a std::unique_ptr, same as your existing approach:
        using node_storage_t = create_node_storage<all_nodes>; // define below

        // setNode / emplaceNode => same idea
        template <typename NodeT, typename... Args>
        NodeT* emplaceNode(Args&&... args)
        {
            auto p = std::make_unique<NodeT>(std::forward<Args>(args)...);
            // Store pipeline_ pointer
            p->pipeline_ = this;
            auto raw = p.get();
            std::get<std::unique_ptr<NodeT>>(nodes_) = std::move(p);
            return raw;
        }

        // We'll define run() etc. just do linear for brevity
        void run()
        {
            // For each node in topological order, call node->execute()
            // skipping layered specifics for brevity
            // (You can adapt from your existing "layered run" or "linear run".)

            doRun<all_nodes>();
        }

        //----------------------------------------------------------
        // The function that Node's getInputRef / getOutputRef call
        //   doGetOutputRef<T, NodeType, OutLoc>(this)
        //   => returns a reference to data_[the correct index]
        //----------------------------------------------------------
        template <typename T, typename NodeType, std::size_t OutLoc>
        T& doGetOutputRef(void* pipelineSelf)
        {
            auto* self = static_cast<Pipeline*>(pipelineSelf);
            static constexpr std::size_t idx = find_data_index_v<all_outputs_tuple, NodeType, OutLoc>;
            static_assert(idx != size_t(-1), "Cannot find (NodeType, OutLoc) in data storage!");
            // data_ is e.g. std::tuple<optional<T1>, optional<T2>, ...>
            // We want get<idx>(data_).value()
            auto& opt = std::get<idx>(self->data_);
            if(!opt.has_value()) {
                opt.emplace(); // default-construct
            }
            return opt.value();
        }

        template <typename T, typename NodeType, std::size_t InLoc>
        T& doGetInputRef(void* pipelineSelf)
        {
            auto* self = static_cast<Pipeline*>(pipelineSelf);
            // We must see which node+outloc produces NodeType's input InLoc
            // => from NodeType::mapping_tuple
            // e.g. node_dependency_map<InLoc, DepNode, DepOutLoc>
            // We find the map with InLoc that matches. 
            // For simplicity, we assume there's exactly one such map. 
            using dep_map = find_dependency_map<InLoc, typename NodeType::mapping_tuple>;
            using DepNode = typename dep_map::dependency_node_t;
            static constexpr size_t DepLoc = dep_map::dep_output_location;
            // So we want doGetOutputRef< T, DepNode, DepLoc >
            static constexpr std::size_t idx = find_data_index_v<all_outputs_tuple, DepNode, DepLoc>;
            auto& opt = std::get<idx>(self->data_);
            assert(opt.has_value() && "Input depends on a node that hasn't produced data yet!");
            // Return reference
            return opt.value();
        }

    private:
        std::unique_ptr<ResourceT> resource_;
        node_storage_t             nodes_;
        data_storage_t             data_;

        // "doRun" can do a simple linear expansion
        template <typename TList> void doRun();

    private:
        //----------------------------------------------------------
        //  A) We need "extract_all_nodes" from NodeList
        //----------------------------------------------------------
        // If NodeList is a single tuple of NodeA, NodeB => done
        // If NodeList is layered => a tuple< std::tuple<NodeA>, std::tuple<NodeB,NodeC>, ... >
        // we flatten it
        template <typename T> struct extract_all_nodes;
        
        // If T is just a std::tuple<Node...> => done
        template <typename... Ns>
        struct extract_all_nodes<std::tuple<Ns...>>
        {
            using type = std::tuple<Ns...>;
        };

        // If T is a tuple of tuples => flatten 
        template <typename... Layers>
        struct extract_all_nodes<std::tuple<std::tuple<Layers...>>>
        {
            // Actually we might have multiple layers
            // We'll do a recursive approach, or just do a partial approach for brevity
            // Let's keep it simple for demonstration
            using type = std::tuple<Layers...>; // single layer
        };

        // If truly layered, you'd do more recursion

        //----------------------------------------------------------
        //  B) We define create_node_storage => std::tuple<std::unique_ptr<NodeA>, ...>
        //----------------------------------------------------------
        template <typename T> struct create_node_storage_impl;
        template <typename... Ns>
        struct create_node_storage_impl<std::tuple<Ns...>>
        {
            using type = std::tuple< std::unique_ptr<Ns>... >;
        };

        template <typename T>
        using create_node_storage = typename create_node_storage_impl<T>::type;

        //----------------------------------------------------------
        //  C) We define find_dependency_map<InLoc, MappingTuple> 
        //     => e.g. node_dependency_map<InLoc, DepNode, OutLoc>
        //----------------------------------------------------------
        template <std::size_t L, typename MapsTuple> struct find_dependency_map_impl
        {
            using type = void;
        };
        template <std::size_t L, typename... Ms>
        struct find_dependency_map_impl<L, std::tuple<Ms...>>
        {
        private:
            template <typename M> struct is_match : std::false_type {};
            template <std::size_t IL, typename DN, std::size_t OL>
            struct is_match< node_dependency_map<IL, DN, OL> >
              : std::bool_constant<(IL==L)> {}; 

            template <typename Found, typename...Ts>
            struct pick { using type = Found; };
            template <typename Found, typename Head, typename...Tail>
            struct pick<Found, Head, Tail...>
            {
                static constexpr bool match = is_match<Head>::value;
                using maybe = std::conditional_t<match, Head, Found>;
                using type = typename pick<maybe, Tail...>::type;
            };
        public:
            using type = typename pick<void, Ms...>::type;
        };
        template <std::size_t L, typename Maps>
        using find_dependency_map = typename find_dependency_map_impl<L, Maps>::type;
    };

    // The doRun for "all_nodes" linear
    template <typename NodeList, typename ResourceT>
    template <typename TList>
    void Pipeline<NodeList,ResourceT>::doRun()
    {
        // TList is e.g. std::tuple<NodeA, NodeB, NodeC>
        doRunImpl<TList>(std::make_index_sequence<std::tuple_size_v<TList>>{});
    }

    template <typename NodeList, typename ResourceT>
    template <typename TList>
    void Pipeline<NodeList,ResourceT>::doRunImpl(std::index_sequence<>) 
    {
        // end
    }

    template <typename NodeList, typename ResourceT>
    template <typename TList>
    void Pipeline<NodeList,ResourceT>::doRunImpl(std::index_sequence<0>)
    {
        // 1-element
        using N0 = std::tuple_element_t<0,TList>;
        auto& nptr = std::get<std::unique_ptr<N0>>(nodes_);
        if (nptr) {
            nptr->execute();
        }
    }

    template <typename NodeList, typename ResourceT>
    template <typename TList>
    void Pipeline<NodeList,ResourceT>::doRunImpl(std::index_sequence<0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15> /*some big pack*/)
    {
        // You can generalize for N elements. For brevity, let's do a fold expression or recursion.
        // Real code can do a simple recursion or "for_each_type" approach.
        // We'll do a naive approach:
        static constexpr size_t Count = std::tuple_size_v<TList>;
        runOne<0,Count-1, TList>();
    }

    // Because this snippet is already huge, let's just do a small approach:
    // In real code, you'd do a normal "for index in [0..Count-1]" approach.

    template <typename NodeList, typename ResourceT>
    template <typename TList, size_t I, size_t N>
    void Pipeline<NodeList,ResourceT>::runOne()
    {
        if constexpr (I < N+1)
        {
            using NodeT = std::tuple_element_t<I,TList>;
            auto& nptr = std::get<std::unique_ptr<NodeT>>(nodes_);
            if (nptr) {
                nptr->execute();
            }
            runOne<I+1,N,TList>();
        }
    }
} // end namespace lux::cxx
