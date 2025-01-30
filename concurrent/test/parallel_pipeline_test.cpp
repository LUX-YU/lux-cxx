#include <iostream>
#include <string>
#include <lux/cxx/concurrent/ParallelPipeline.hpp>

namespace demo
{
    using namespace lux::cxx;

    // ============================================================== 
    // 与之前相同的节点定义（仅供演示，如果你已有这些节点定义，可复用） 
    // ==============================================================

    // NodeA: 无依赖 => mapping_tuple 为空
    // 输出：out<0, int>
    struct NodeA
        : NodeBase<NodeA,
        node_descriptor<>,                   // 无输入
        node_descriptor< out_binding<0, int> > // 输出： out<0, int>
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

    // NodeB: 依赖 NodeA 的 out<0, int>
    // Inputs: in<0, int>
    // Outputs: out<0, std::string>
    // Dependencies: node_dependency_map<0, NodeA, 0>
    struct NodeB
        : NodeBase<NodeB,
        node_descriptor< in_binding<0, int> >,
        node_descriptor< out_binding<0, std::string> >,
        node_dependency_map<0, NodeA, 0>
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

    // NodeC: 依赖 NodeB 的 out<0, std::string>
    // Inputs: in<0, std::string>
    // Outputs: out<0, double>
    // Dependencies: node_dependency_map<0, NodeB, 0>
    struct NodeC
        : NodeBase<NodeC,
        node_descriptor< in_binding<0, std::string> >,
        node_descriptor< out_binding<0, double> >,
        node_dependency_map<0, NodeB, 0>
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

    // NodeD: 依赖 NodeA 的 out<0, int> 和 NodeC 的 out<0, double>
    // Inputs: in<0, int>, in<1, double>
    // Outputs: out<0, std::string>
    // Dependencies: node_dependency_map<0, NodeA, 0>, node_dependency_map<1, NodeC, 0>
    struct NodeD
        : NodeBase<NodeD,
        node_descriptor<
        in_binding<0, int>,
        in_binding<1, double>
        >,
        node_descriptor< out_binding<0, std::string> >,
        node_dependency_map<0, NodeA, 0>,
        node_dependency_map<1, NodeC, 0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& inputInt = p.template getInputRef<NodeD, 0, int>();
            auto& inputDouble = p.template getInputRef<NodeD, 1, double>();
            std::cout << "[NodeD] Read NodeA out<0> = " << inputInt
                << ", NodeC out<0> = " << inputDouble << "\n";

            auto& outStr = p.template getOutputRef<NodeD, 0, std::string>();
            outStr = "Aggregated by NodeD: " + std::to_string(inputInt)
                + " and " + std::to_string(inputDouble);
            std::cout << "[NodeD] Writing out<0> = " << outStr << "\n";
        }
    };

    // NodeE: 无依赖
    // 输出：out<0, std::string>
    struct NodeE
        : NodeBase<NodeE,
        node_descriptor<>,
        node_descriptor< out_binding<0, std::string> >
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

    // NodeF: 依赖 NodeD 的 out<0, std::string> 和 NodeE 的 out<0, std::string>
    // Inputs: in<0, std::string>, in<1, std::string>
    // Outputs: out<0, bool>
    // Dependencies: node_dependency_map<0, NodeD, 0>, node_dependency_map<1, NodeE, 0>
    struct NodeF
        : NodeBase<NodeF,
        node_descriptor<
        in_binding<0, std::string>,
        in_binding<1, std::string>
        >,
        node_descriptor< out_binding<0, bool> >,
        node_dependency_map<0, NodeD, 0>,
        node_dependency_map<1, NodeE, 0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& inputStr1 = p.template getInputRef<NodeF, 0, std::string>();
            auto& inputStr2 = p.template getInputRef<NodeF, 1, std::string>();
            std::cout << "[NodeF] Read NodeD out<0> = " << inputStr1
                << ", NodeE out<0> = " << inputStr2 << "\n";

            auto& outBool = p.template getOutputRef<NodeF, 0, bool>();
            outBool = (inputStr1.find("Aggregated") != std::string::npos)
                && (inputStr2 == "NodeE Data");
            std::cout << "[NodeF] Writing out<0> = "
                << std::boolalpha << outBool << "\n";
        }
    };
}

// ==============================================================
// 并行测试入口：使用ParallelPipeline在分层拓扑下并行执行
// ==============================================================

int main()
{
    using namespace lux::cxx;
    using namespace demo;

    std::cout << "==== [Parallel Pipeline Test] ====\n";

    // 不用再手动指定顺序，而是让 layered_topological_sort 来找出分层
    // Unsorted: <NodeF, NodeD, NodeB, NodeC, NodeA, NodeE>
    using Unsorted = std::tuple<NodeF, NodeD, NodeB, NodeC, NodeA, NodeE>;

    // 进行分层拓扑排序
    using Layered = layered_topological_sort<Unsorted>::type;
    // 其结果（layers）预期类似：
    // Layer0: (NodeA, NodeE)
    // Layer1: (NodeB, NodeC)
    // Layer2: (NodeD)
    // Layer3: (NodeF)

    // 定义资源类型（可自定义）
    struct MyResource
    {
        // 可以在这里放置一些共享的数据或功能
        int resource_id = 777;
    };

    // 使用并行管线，指定线程池大小，例如 4
    ParallelPipeline<Layered, MyResource> pipeline(/*pool_size=*/4);

    // 放置节点
    pipeline.emplaceNode<NodeA>();
    pipeline.emplaceNode<NodeB>();
    pipeline.emplaceNode<NodeC>();
    pipeline.emplaceNode<NodeD>();
    pipeline.emplaceNode<NodeE>();
    pipeline.emplaceNode<NodeF>();

    // 运行管线（并行执行同一层的节点）
    pipeline.run();

    return 0;
}
