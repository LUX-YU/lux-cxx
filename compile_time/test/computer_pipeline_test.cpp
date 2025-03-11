#include "lux/cxx/compile_time/computer_node.hpp"
#include "lux/cxx/compile_time/computer_node_sort.hpp"
#include "lux/cxx/compile_time/computer_pipeline.hpp"
#include <iostream>
#include <string>

namespace demo
{
    using namespace lux::cxx;

    // Example NodeA: no inputs => out<0,int>
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

    // Example NodeB: input in<0,int> => output out<0,std::string>
    struct NodeB
        : NodeBase<NodeB,
        node_descriptor< in_binding<0, int> >,
        node_descriptor< out_binding<0, std::string> >
        >
    {
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& val = std::get<0>(in);
            std::cout << "[NodeB] Read in<0> = " << val << "\n";

            auto& str = std::get<0>(out);
            str = "NodeB result: " + std::to_string(val);
            std::cout << "[NodeB] Writing out<0> = " << str << "\n";
            return true;
        }
    };
}

int main()
{
    using namespace lux::cxx;
    using namespace demo;

    // 1) Define the node set
    using NodeList = std::tuple<NodeA, NodeB>;

    // 2) Define the dependency edges: NodeB's in<0> depends on NodeA's out<0>
    using DepList = std::tuple<
        dependency_map<NodeB, 0, NodeA, 0>
    >;

    // 4) Build the pipeline
    ComputerPipeline<NodeList, DepList> pipeline;
    pipeline.emplaceNode<NodeA>();
    pipeline.emplaceNode<NodeB>();

    // 5) Run => NodeA executes, then NodeB
    pipeline.run();

    return 0;
}
