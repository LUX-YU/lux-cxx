#include <iostream>
#include <string>
#include "lux/cxx/concurrent/ParallelPipeline.hpp" // The parallel pipeline you adapted
// If you have a "tuple_traits.hpp" that provides for_each_type, include it, or remove that part.

namespace demo
{
    using namespace lux::cxx;

    //==============================================================
    // NodeA: No inputs => out<0,int>
    //==============================================================
    struct NodeA
        : NodeBase<NodeA,
        node_descriptor<>,                      // no inputs
        node_descriptor< out_binding<0, int> >    // out<0,int>
        >
    {
        bool execute(const in_param_t& /*in*/, out_param_t& out)
        {
            std::cout << "[NodeA] Writing out<0> = 100\n";
            std::get<0>(out) = 100;
            return true;
        }
    };

    //==============================================================
    // NodeB: input in<0,int> => out<0,std::string>
    //==============================================================
    struct NodeB
        : NodeBase<NodeB,
        node_descriptor< in_binding<0, int> >,
        node_descriptor< out_binding<0, std::string> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& inputVal = std::get<0>(in);
            std::cout << "[NodeB] Read inputVal = " << inputVal << "\n";

            auto& outStr = std::get<0>(out);
            outStr = "Processed by NodeB: " + std::to_string(inputVal);
            std::cout << "[NodeB] Writing out<0> = " << outStr << "\n";
            return true;
        }
    };

    //==============================================================
    // NodeC: input in<0,std::string> => out<0,double>
    //==============================================================
    struct NodeC
        : NodeBase<NodeC,
        node_descriptor< in_binding<0, std::string> >,
        node_descriptor< out_binding<0, double> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& inputStr = std::get<0>(in);
            std::cout << "[NodeC] Read inputStr = " << inputStr << "\n";

            auto& outDouble = std::get<0>(out);
            outDouble = 3.14;
            std::cout << "[NodeC] Writing out<0> = " << outDouble << "\n";
            return true;
        }
    };

    //==============================================================
    // NodeD: inputs in<0,int>, in<1,double> => out<0,std::string>
    //==============================================================
    struct NodeD
        : NodeBase<NodeD,
        node_descriptor< in_binding<0, int>, in_binding<1, double> >,
        node_descriptor< out_binding<0, std::string> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& inputInt = std::get<0>(in);
            auto& inputDouble = std::get<1>(in);

            std::cout << "[NodeD] Read in<0>=" << inputInt
                << ", in<1>=" << inputDouble << "\n";

            auto& outStr = std::get<0>(out);
            outStr = "Aggregated by NodeD: " + std::to_string(inputInt)
                + " and " + std::to_string(inputDouble);
            std::cout << "[NodeD] Writing out<0> = " << outStr << "\n";
            return true;
        }
    };

    //==============================================================
    // NodeE: No inputs => out<0,std::string>
    //==============================================================
    struct NodeE
        : NodeBase<NodeE,
        node_descriptor<>,
        node_descriptor< out_binding<0, std::string> >
        >
    {
        bool execute(const in_param_t& /*in*/, out_param_t& out)
        {
            std::cout << "[NodeE] Writing out<0> = \"NodeE Data\"\n";
            std::get<0>(out) = "NodeE Data";
            return true;
        }
    };

    //==============================================================
    // NodeF: inputs in<0,std::string>, in<1,std::string> => out<0,bool>
    //==============================================================
    struct NodeF
        : NodeBase<NodeF,
            node_descriptor< in_binding<0, std::string>, in_binding<1, std::string> >,
            node_descriptor< out_binding<0, bool> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& s1 = std::get<0>(in);
            auto& s2 = std::get<1>(in);

            std::cout << "[NodeF] Read in<0>=" << s1
                << ", in<1>=" << s2 << "\n";

            auto& outBool = std::get<0>(out);
            outBool = (s1.find("Aggregated") != std::string::npos)
                && (s2 == "NodeE Data");
            std::cout << "[NodeF] Writing out<0> = " << std::boolalpha << outBool << "\n";
            return true;
        }
    };

    //==============================================================
    // NodeG: inputs in<0,std::string>, in<1,double>, in<2,int>
    //        => out<0,float>, out<1,std::string>
    //==============================================================
    struct NodeG
        : NodeBase<NodeG,
        node_descriptor< in_binding<0, std::string>,
        in_binding<1, double>,
        in_binding<2, int> >,
        node_descriptor< out_binding<0, float>,
        out_binding<1, std::string> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& strVal = std::get<0>(in);
            auto& dblVal = std::get<1>(in);
            auto& intVal = std::get<2>(in);

            std::cout << "[NodeG] Read in<0>=" << strVal
                << ", in<1>=" << dblVal
                << ", in<2>=" << intVal << "\n";

            auto& out0 = std::get<0>(out); // float&
            out0 = static_cast<float>(dblVal + intVal);

            auto& out1 = std::get<1>(out); // std::string&
            out1 = strVal + " + extra";

            std::cout << "[NodeG] Writing out<0>=" << out0
                << ", out<1>=" << out1 << "\n";
            return true;
        }
    };

    //==============================================================
    // NodeH: inputs in<0,std::string>, in<1,float> => out<0,int>
    //==============================================================
    struct NodeH
        : NodeBase<NodeH,
        node_descriptor< in_binding<0, std::string>,
        in_binding<1, float> >,
        node_descriptor< out_binding<0, int> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& dStr = std::get<0>(in);
            auto& gFloat = std::get<1>(in);

            std::cout << "[NodeH] Read in<0>=" << dStr
                << ", in<1>=" << gFloat << "\n";

            auto& out0 = std::get<0>(out);
            out0 = static_cast<int>(gFloat) + static_cast<int>(dStr.size());

            std::cout << "[NodeH] Writing out<0> = " << out0 << "\n";
            return true;
        }
    };

    //==============================================================
    // NodeI: inputs in<0,std::string>, in<1,int> => out<0,bool>
    //==============================================================
    struct NodeI
        : NodeBase<NodeI,
        node_descriptor< in_binding<0, std::string>,
        in_binding<1, int> >,
        node_descriptor< out_binding<0, bool> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& eStr = std::get<0>(in);
            auto& hInt = std::get<1>(in);

            std::cout << "[NodeI] Read in<0>=" << eStr
                << ", in<1>=" << hInt << "\n";

            auto& out0 = std::get<0>(out);
            out0 = (eStr == "NodeE Data") && (hInt > 20);

            std::cout << "[NodeI] Writing out<0> = " << std::boolalpha << out0 << "\n";
            return true;
        }
    };

    //==============================================================
    // NodeJ: inputs in<0,int>, in<1,bool> => out<0,std::string>
    //==============================================================
    struct NodeJ
        : NodeBase<NodeJ,
        node_descriptor< in_binding<0, int>,
        in_binding<1, bool> >,
        node_descriptor< out_binding<0, std::string> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& hInt = std::get<0>(in);
            auto& fBool = std::get<1>(in);

            std::cout << "[NodeJ] Read in<0>=" << hInt
                << ", in<1>=" << fBool << "\n";

            auto& out0 = std::get<0>(out);
            out0 = "NodeJ says: H=" + std::to_string(hInt)
                + (fBool ? ", F=TRUE" : ", F=FALSE");

            std::cout << "[NodeJ] Writing out<0> = " << out0 << "\n";
            return true;
        }
    };

} // namespace demo


int main()
{
    using namespace lux::cxx;
    using namespace demo;

    std::cout << "==== [Parallel Pipeline Test - Current Approach] ====\n";

    //------------------------------------------------------------------------------
    // 1) List all nodes in an unsorted tuple
    //------------------------------------------------------------------------------
    using Unsorted = std::tuple<
        NodeA, NodeB, NodeC, NodeD, NodeE,
        NodeF, NodeG, NodeH, NodeI, NodeJ
    >;

    //------------------------------------------------------------------------------
    // 2) Declare the dependency maps externally
    //    (Which node’s input location depends on which node’s output location)
    //------------------------------------------------------------------------------

    // NodeB in<0> depends on NodeA out<0>
    // NodeC in<0> depends on NodeB out<0>
    // NodeD in<0> depends on NodeA out<0>, in<1> on NodeC out<0>
    // NodeF in<0> depends on NodeD out<0>, in<1> on NodeE out<0>
    // NodeG in<0> depends on NodeB out<0>, in<1> on NodeC out<0>, in<2> on NodeA out<0>
    // NodeH in<0> depends on NodeD out<0>, in<1> on NodeG out<0> (float)
    // NodeI in<0> depends on NodeE out<0>, in<1> on NodeH out<0>
    // NodeJ in<0> depends on NodeH out<0>, in<1> on NodeF out<0>

    using DepList = std::tuple<
        dependency_map<NodeB, 0, NodeA, 0>,
        dependency_map<NodeC, 0, NodeB, 0>,
        dependency_map<NodeD, 0, NodeA, 0>,
        dependency_map<NodeD, 1, NodeC, 0>,
        dependency_map<NodeF, 0, NodeD, 0>,
        dependency_map<NodeF, 1, NodeE, 0>,
        dependency_map<NodeG, 0, NodeB, 0>,
        dependency_map<NodeG, 1, NodeC, 0>,
        dependency_map<NodeG, 2, NodeA, 0>,
        dependency_map<NodeH, 0, NodeD, 0>,
        dependency_map<NodeH, 1, NodeG, 0>,
        dependency_map<NodeI, 0, NodeE, 0>,
        dependency_map<NodeI, 1, NodeH, 0>,
        dependency_map<NodeJ, 0, NodeH, 0>,
        dependency_map<NodeJ, 1, NodeF, 0>
    >;

    //------------------------------------------------------------------------------
    // 3) Perform layered topological sort
    //------------------------------------------------------------------------------
    using Layered = layered_topological_sort<Unsorted, DepList>::type;

    // Optionally, print out the layers (if you have tuple_traits::for_each_type)
    // If you don't have it, just skip this.
#ifdef HAS_TUPLE_TRAITS
    tuple_traits::for_each_type<Layered>(
        []<typename Layer, size_t I>() {
        std::cout << "Layer " << I << ":\n";
        tuple_traits::for_each_type<Layer>(
            []<typename Node, size_t J>() {
            std::cout << "  Node " << J << ": " << typeid(Node).name() << "\n";
        }
        );
    }
    );
#endif

    //------------------------------------------------------------------------------
    // 4) Build the parallel pipeline with the layered nodes + the dependency list
    //------------------------------------------------------------------------------
    ParallelComputerPipeline<Layered, DepList> pipeline(4 /* thread pool size */);

    // 5) Emplace each node
    pipeline.emplaceNode<NodeA>();
    pipeline.emplaceNode<NodeB>();
    pipeline.emplaceNode<NodeC>();
    pipeline.emplaceNode<NodeD>();
    pipeline.emplaceNode<NodeE>();
    pipeline.emplaceNode<NodeF>();
    pipeline.emplaceNode<NodeG>();
    pipeline.emplaceNode<NodeH>();
    pipeline.emplaceNode<NodeI>();
    pipeline.emplaceNode<NodeJ>();

    // 6) Run the pipeline (nodes in each layer execute in parallel)
    pipeline.run();

    return 0;
}
