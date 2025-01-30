#include <lux/cxx/compile_time/computer_graph.hpp>
#include <iostream>
#include <string>

namespace demo
{
    using namespace lux::cxx;

    // ==============================================================
    // Node Definitions
    // ==============================================================

    // NodeA: No dependencies => mapping_tuple is empty
    // Outputs: out<0, int>
    struct NodeA
        : NodeBase<NodeA,
        node_descriptor<>,                                 // No inputs
        node_descriptor< out_binding<0, int> >            // Outputs: out<0, int>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            std::cout << "[NodeA] Writing out<0> = 100\n";
            auto& ref = p.template getOutputRef<NodeA, 0, int>();
            ref = 100;
        }
    };

    // NodeB: Depends on NodeA's out<0, int>
    // Inputs: in<0, int>
    // Outputs: out<0, std::string>
    // Dependencies: node_dependency_map<0, NodeA, 0>
    struct NodeB
        : NodeBase<NodeB,
        node_descriptor< in_binding<0, int> >,            // Inputs: in<0, int>
        node_descriptor< out_binding<0, std::string> >,   // Outputs: out<0, std::string>
        node_dependency_map<0, NodeA, 0>                   // Dependency: NodeA's out<0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& inputVal = p.template getInputRef<NodeB, 0, int>();
            std::cout << "[NodeB] Read NodeA out<0> = " << inputVal << "\n";

            auto& outStr = p.template getOutputRef<NodeB, 0, std::string>();
            outStr = "Processed by NodeB: " + std::to_string(inputVal);
            std::cout << "[NodeB] Writing out<0> = " << outStr << "\n";
        }
    };

    // NodeC: Depends on NodeB's out<0, std::string>
    // Inputs: in<0, std::string>
    // Outputs: out<0, double>
    // Dependencies: node_dependency_map<0, NodeB, 0>
    struct NodeC
        : NodeBase<NodeC,
        node_descriptor< in_binding<0, std::string> >,     // Inputs: in<0, std::string>
        node_descriptor< out_binding<0, double> >,         // Outputs: out<0, double>
        node_dependency_map<0, NodeB, 0>                   // Dependency: NodeB's out<0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& inputStr = p.template getInputRef<NodeC, 0, std::string>();
            std::cout << "[NodeC] Read NodeB out<0> = " << inputStr << "\n";

            auto& outDouble = p.template getOutputRef<NodeC, 0, double>();
            outDouble = 3.14;
            std::cout << "[NodeC] Writing out<0> = " << outDouble << "\n";
        }
    };

    // NodeD: Depends on NodeA's out<0, int> and NodeC's out<0, double>
    // Inputs: in<0, int>, in<1, double>
    // Outputs: out<0, std::string>
    // Dependencies: node_dependency_map<0, NodeA, 0>, node_dependency_map<1, NodeC, 0>
    struct NodeD
        : NodeBase<NodeD,
        node_descriptor<
        in_binding<0, int>,                            // Inputs: in<0, int>
        in_binding<1, double>                          // Inputs: in<1, double>
        >,
        node_descriptor< out_binding<0, std::string> >,    // Outputs: out<0, std::string>
        node_dependency_map<0, NodeA, 0>,                  // Dependency: NodeA's out<0>
        node_dependency_map<1, NodeC, 0>                   // Dependency: NodeC's out<0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& inputInt = p.template getInputRef<NodeD, 0, int>();
            auto& inputDouble = p.template getInputRef<NodeD, 1, double>();
            std::cout << "[NodeD] Read NodeA out<0> = " << inputInt << ", NodeC out<0> = " << inputDouble << "\n";

            auto& outStr = p.template getOutputRef<NodeD, 0, std::string>();
            outStr = "Aggregated by NodeD: " + std::to_string(inputInt) + " and " + std::to_string(inputDouble);
            std::cout << "[NodeD] Writing out<0> = " << outStr << "\n";
        }
    };

    // NodeE: No dependencies
    // Outputs: out<0, std::string>
    struct NodeE
        : NodeBase<NodeE,
        node_descriptor<>,                                  // No inputs
        node_descriptor< out_binding<0, std::string> >      // Outputs: out<0, std::string>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            std::cout << "[NodeE] Writing out<0> = \"NodeE Data\"\n";
            auto& outStr = p.template getOutputRef<NodeE, 0, std::string>();
            outStr = "NodeE Data";
        }
    };

    // NodeF: Depends on NodeD's out<0, std::string> and NodeE's out<0, std::string>
    // Inputs: in<0, std::string>, in<1, std::string>
    // Outputs: out<0, bool>
    // Dependencies: node_dependency_map<0, NodeD, 0>, node_dependency_map<1, NodeE, 0>
    struct NodeF
        : NodeBase<NodeF,
        node_descriptor<
        in_binding<0, std::string>,                       // Inputs: in<0, std::string>
        in_binding<1, std::string>                        // Inputs: in<1, std::string>
        >,
        node_descriptor< out_binding<0, bool> >,             // Outputs: out<0, bool>
        node_dependency_map<0, NodeD, 0>,                     // Dependency: NodeD's out<0>
        node_dependency_map<1, NodeE, 0>                      // Dependency: NodeE's out<0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& inputStr1 = p.template getInputRef<NodeF, 0, std::string>();
            auto& inputStr2 = p.template getInputRef<NodeF, 1, std::string>();
            std::cout << "[NodeF] Read NodeD out<0> = " << inputStr1 << ", NodeE out<0> = " << inputStr2 << "\n";

            auto& outBool = p.template getOutputRef<NodeF, 0, bool>();
            outBool = (inputStr1.find("Aggregated") != std::string::npos) && (inputStr2 == "NodeE Data");
            std::cout << "[NodeF] Writing out<0> = " << std::boolalpha << outBool << "\n";
        }
    };
}

// ==============================================================
 // Main: Demonstrates Linear and Layered Topological Sorting
 // ==============================================================
int main()
{
    using namespace lux::cxx;

    std::cout << "==== [Linear Topology] ====\n";
    {
        // Unsorted: <NodeF, NodeD, NodeB, NodeC, NodeA, NodeE>
        using Unsorted = std::tuple<demo::NodeF, demo::NodeD, demo::NodeB, demo::NodeC, demo::NodeA, demo::NodeE>;
        // Perform linear topological sort
        using Sorted = topological_sort<Unsorted>::type;
        // Expected Order: (NodeA, NodeE, NodeB, NodeC, NodeD, NodeF)

        struct MyResource { int resource_id = 42; };
        Pipeline<Sorted, MyResource> pipeline;
        pipeline.emplaceNode<demo::NodeA>();
        pipeline.emplaceNode<demo::NodeE>();
        pipeline.emplaceNode<demo::NodeB>();
        pipeline.emplaceNode<demo::NodeC>();
        pipeline.emplaceNode<demo::NodeD>();
        pipeline.emplaceNode<demo::NodeF>();

        pipeline.run();
    }

    std::cout << "\n==== [Layered Topology] ====\n";
    {
        // Unsorted: <NodeF, NodeD, NodeB, NodeC, NodeA, NodeE>
        using Unsorted = std::tuple<demo::NodeF, demo::NodeD, demo::NodeB, demo::NodeC, demo::NodeA, demo::NodeE>;
        // Perform layered topological sort
        // Expected Layers:
        // Layer 0: <NodeA, NodeE>
        // Layer 1: <NodeB, NodeC>
        // Layer 2: <NodeD>
        // Layer 3: <NodeF>
        using Layered = layered_topological_sort<Unsorted>::type;

        struct MyResource2 { std::string name = "Resource2"; };
        Pipeline<Layered, MyResource2> pipeline2;
        pipeline2.emplaceNode<demo::NodeA>();
        pipeline2.emplaceNode<demo::NodeE>();
        pipeline2.emplaceNode<demo::NodeB>();
        pipeline2.emplaceNode<demo::NodeC>();
        pipeline2.emplaceNode<demo::NodeD>();
        pipeline2.emplaceNode<demo::NodeF>();

        pipeline2.run();
    }

    return 0;
}
