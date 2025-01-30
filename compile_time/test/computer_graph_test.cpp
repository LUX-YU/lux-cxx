#include <lux/cxx/compile_time/computer_graph.hpp>
#include <string>

using namespace lux::cxx;

struct NodeA
    : lux::cxx::NodeBase<NodeA,
    lux::cxx::node_descriptor<>,
    lux::cxx::node_descriptor<>
    >
{
    void execute() {
        std::cout << "Running NodeA\n";
    }
};

struct NodeB
    : lux::cxx::NodeBase<NodeB,
    lux::cxx::node_descriptor<>,
    lux::cxx::node_descriptor<>,
    lux::cxx::node_dependency_map<0, NodeA, 0>>
{
    void execute() {
        std::cout << "Running NodeB (depends on A)\n";
    }
};

struct NodeC
    : lux::cxx::NodeBase<NodeC,
    lux::cxx::node_descriptor<>,
    lux::cxx::node_descriptor<>,
    lux::cxx::node_dependency_map<0, NodeA, 0>>
{
    void execute() {
        std::cout << "Running NodeC (depends on A)\n";
    }
};

struct NodeD
    : lux::cxx::NodeBase<NodeD,
    lux::cxx::node_descriptor<>,
    lux::cxx::node_descriptor<>,
    lux::cxx::node_dependency_map<0, NodeB, 0>,
    lux::cxx::node_dependency_map<0, NodeC, 0>>
{
    void execute() {
        std::cout << "Running NodeD (depends on B and C)\n";
    }
};

struct PipelineResource
{
	std::string name;
};

int main()
{
    // Suppose we define an unsorted set <B,C,A,D>
    using unsorted = std::tuple<NodeB, NodeC, NodeA, NodeD>;

    // 1) Linear topological sort
    using linear_sorted = topological_sort<unsorted>::type;
    // might produce: std::tuple<NodeA, NodeB, NodeC, NodeD>

    // 2) Layered topological sort
    using layered_sorted = layered_topological_sort<unsorted>::type;
    // might produce: std::tuple< std::tuple<NodeA>, std::tuple<NodeB, NodeC>, std::tuple<NodeD> >

    // Build pipeline with linear sort
    Pipeline<linear_sorted, PipelineResource> pipeline_linear;
    pipeline_linear.emplaceNode<NodeA>();
    pipeline_linear.emplaceNode<NodeB>();
    pipeline_linear.emplaceNode<NodeC>();
    pipeline_linear.emplaceNode<NodeD>();

    std::cout << "Running linear pipeline...\n";
    pipeline_linear.run(); // executes A => B => C => D in order

    std::cout << "\n====================\n";

    // Build pipeline with layered sort
    Pipeline<layered_sorted, PipelineResource> pipeline_layered;
    pipeline_layered.emplaceNode<NodeA>();
    pipeline_layered.emplaceNode<NodeB>();
    pipeline_layered.emplaceNode<NodeC>();
    pipeline_layered.emplaceNode<NodeD>();

    std::cout << "Running layered pipeline...\n";
    pipeline_layered.run();
    // executes layer1: A
    //         layer2: B, C  (in any order, or "parallel" conceptually)
    //         layer3: D

    return 0;
}