#include <iostream>
#include <string>
#include <lux/cxx/concurrent/ParallelPipeline.hpp>

namespace demo
{
    using namespace lux::cxx;

    //==============================================================================
    //  NodeA: 无依赖 => mapping_tuple 为空
    //  输出：out<0, int>
    //==============================================================================
    struct NodeA
        : NodeBase<NodeA,
        node_descriptor<>,                      // 无输入
        node_descriptor< out_binding<0, int> >   // 输出： out<0, int>
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

    //==============================================================================
    //  NodeB: 依赖 NodeA 的 out<0, int>
    //  Inputs: in<0, int>
    //  Outputs: out<0, std::string>
    //  Dependencies: node_dependency_map<0, NodeA, 0>
    //==============================================================================
    struct NodeB
        : NodeBase<NodeB,
        node_descriptor< in_binding<0, int> >,
        node_descriptor< out_binding<0, std::string> >,
        node_dependency_map<0, NodeA, 0>>
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

    //==============================================================================
    //  NodeC: 依赖 NodeB 的 out<0, std::string>
    //  Inputs: in<0, std::string>
    //  Outputs: out<0, double>
    //  Dependencies: node_dependency_map<0, NodeB, 0>
    //==============================================================================
    struct NodeC
        : NodeBase<NodeC,
        node_descriptor<in_binding<0, std::string>>,
        node_descriptor<out_binding<0, double>>,
        node_dependency_map<0, NodeB, 0>>
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

    //==============================================================================
    //  NodeD: 依赖 NodeA out<0, int> + NodeC out<0, double>
    //  Inputs: in<0, int>, in<1, double>
    //  Outputs: out<0, std::string>
    //  Dependencies: node_dependency_map<0, NodeA, 0>, node_dependency_map<1, NodeC, 0>
    //==============================================================================
    struct NodeD
        : NodeBase<NodeD,
        node_descriptor< in_binding<0, int>,
        in_binding<1, double> >,
        node_descriptor< out_binding<0, std::string> >,
        node_dependency_map<0, NodeA, 0>,
        node_dependency_map<1, NodeC, 0>>
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

    //==============================================================================
    //  NodeE: 无依赖 => mapping_tuple 为空
    //  输出：out<0, std::string>
    //==============================================================================
    struct NodeE
        : NodeBase<NodeE,
        node_descriptor<>,
        node_descriptor< out_binding<0, std::string> >>
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            std::cout << "[NodeE] Writing out<0> = \"NodeE Data\"\n";
            auto& outStr = p.template getOutputRef<NodeE, 0, std::string>();
            outStr = "NodeE Data";
        }
    };

    //==============================================================================
    //  NodeF: 依赖 NodeD out<0, std::string> + NodeE out<0, std::string>
    //  Inputs: in<0, std::string>, in<1, std::string>
    //  Outputs: out<0, bool>
    //  Dependencies: node_dependency_map<0, NodeD, 0>, node_dependency_map<1, NodeE, 0>
    //==============================================================================
    struct NodeF
        : NodeBase<NodeF,
        node_descriptor< in_binding<0, std::string>,
        in_binding<1, std::string> >,
        node_descriptor< out_binding<0, bool> >,
        node_dependency_map<0, NodeD, 0>,
        node_dependency_map<1, NodeE, 0>>
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

    //==============================================================================
    //  NodeG: 依赖 NodeB out<0, std::string> + NodeC out<0, double> + NodeA out<0, int>
    //  Inputs: in<0, std::string>, in<1, double>, in<2, int>
    //  Outputs: out<0, float>, out<1, std::string>
    //  Dependencies: node_dependency_map<0, NodeB, 0>, node_dependency_map<1, NodeC, 0>, node_dependency_map<2, NodeA, 0>
    //==============================================================================
    struct NodeG
        : NodeBase<NodeG,
            node_descriptor<in_binding<0, std::string>,in_binding<1, double>,in_binding<2, int> >,
            node_descriptor<out_binding<0, float>, out_binding<1, std::string>>,
            node_dependency_map<0, NodeB, 0>,
            node_dependency_map<1, NodeC, 0>,
            node_dependency_map<2, NodeA, 0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& strVal = p.template getInputRef<NodeG, 0, std::string>();
            auto& dblVal = p.template getInputRef<NodeG, 1, double>();
            auto& intVal = p.template getInputRef<NodeG, 2, int>();

            std::cout << "[NodeG] Read from NodeB out<0>: " << strVal << "\n";
            std::cout << "[NodeG] Read from NodeC out<0>: " << dblVal << "\n";
            std::cout << "[NodeG] Read from NodeA out<0>: " << intVal << "\n";

            auto& out0 = p.template getOutputRef<NodeG, 0, float>();
            out0 = static_cast<float>(dblVal + intVal);

            auto& out1 = p.template getOutputRef<NodeG, 1, std::string>();
            out1 = strVal + " + extra";

            std::cout << "[NodeG] Writing out<0> = " << out0
                << ", out<1> = " << out1 << "\n";
        }
    };

    //==============================================================================
    //  NodeH: 依赖 NodeD out<0, std::string> + NodeG out<0, float>
    //  Inputs: in<0, std::string>, in<1, float>
    //  Outputs: out<0, int>
    //  Dependencies: node_dependency_map<0, NodeD, 0>, node_dependency_map<1, NodeG, 0>
    //==============================================================================
    struct NodeH
        : NodeBase<NodeH,
            node_descriptor<in_binding<0, std::string>, in_binding<1, float>>,
            node_descriptor<out_binding<0, int>>,
            node_dependency_map<0, NodeD, 0>,
            node_dependency_map<1, NodeG, 0>
        >
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& dStr = p.template getInputRef<NodeH, 0, std::string>();
            auto& gFloat = p.template getInputRef<NodeH, 1, float>();

            std::cout << "[NodeH] Read NodeD out<0>: " << dStr
                << ", NodeG out<0>: " << gFloat << "\n";

            auto& out0 = p.template getOutputRef<NodeH, 0, int>();
            out0 = static_cast<int>(gFloat) + (int)dStr.size();

            std::cout << "[NodeH] Writing out<0> = " << out0 << "\n";
        }
    };

    //==============================================================================
    //  NodeI: 依赖 NodeE out<0, std::string> + NodeH out<0, int>
    //  Inputs: in<0, std::string>, in<1, int>
    //  Outputs: out<0, bool>
    //  Dependencies: node_dependency_map<0, NodeE, 0>, node_dependency_map<1, NodeH, 0>
    //==============================================================================
    struct NodeI
        : NodeBase<NodeI,
        node_descriptor< in_binding<0, std::string>,
        in_binding<1, int> >,
        node_descriptor< out_binding<0, bool> >,
        node_dependency_map<0, NodeE, 0>,
        node_dependency_map<1, NodeH, 0>>
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& eStr = p.template getInputRef<NodeI, 0, std::string>();
            auto& hInt = p.template getInputRef<NodeI, 1, int>();

            std::cout << "[NodeI] Read NodeE out<0>: " << eStr
                << ", NodeH out<0>: " << hInt << "\n";

            auto& out0 = p.template getOutputRef<NodeI, 0, bool>();
            out0 = (eStr == "NodeE Data") && (hInt > 20);

            std::cout << "[NodeI] Writing out<0> = " << std::boolalpha << out0 << "\n";
        }
    };

    //==============================================================================
    //  NodeJ: 依赖 NodeH out<0, int> + NodeF out<0, bool>
    //  Inputs: in<0, int>, in<1, bool>
    //  Outputs: out<0, std::string>
    //  Dependencies: node_dependency_map<0, NodeH, 0>, node_dependency_map<1, NodeF, 0>
    //==============================================================================
    struct NodeJ
        : NodeBase<NodeJ,
        node_descriptor< in_binding<0, int>,
        in_binding<1, bool> >,
        node_descriptor< out_binding<0, std::string> >,
        node_dependency_map<0, NodeH, 0>,
        node_dependency_map<1, NodeF, 0>>
    {
        template <typename PipelineT>
        void execute(PipelineT& p)
        {
            auto& hInt = p.template getInputRef<NodeJ, 0, int>();
            auto& fBool = p.template getInputRef<NodeJ, 1, bool>();

            std::cout << "[NodeJ] Read NodeH out<0>: " << hInt
                << ", NodeF out<0>: " << fBool << "\n";

            auto& out0 = p.template getOutputRef<NodeJ, 0, std::string>();
            out0 = "NodeJ says: H=" + std::to_string(hInt)
                + (fBool ? ", F=TRUE" : ", F=FALSE");

            std::cout << "[NodeJ] Writing out<0> = " << out0 << "\n";
        }
    };
}

//==============================================================
// 并行测试入口：使用ParallelPipeline在分层拓扑下并行执行
//==============================================================
int main()
{
    using namespace lux::cxx;
    using namespace demo;

    std::cout << "==== [Parallel Pipeline Test - More Complex] ====\n";

    // 我们把所有节点都放进 unsorted 里，让 layered_topological_sort 自动处理
    // NodeA, NodeB, NodeC, NodeD, NodeE, NodeF, NodeG, NodeH, NodeI, NodeJ
    using Unsorted = std::tuple<
        NodeF, NodeD, NodeB, NodeC, NodeA, NodeE, NodeG, NodeH, NodeI, NodeJ
    >;

    // 进行分层拓扑排序
    using Layered = layered_topological_sort<Unsorted>::type;

    // 定义资源类型（可自定义）
    struct MyResource
    {
        int resource_id = 999;
    };

    // 使用并行管线，指定线程池大小
    ParallelPipeline<Layered, MyResource> pipeline(/*pool_size=*/4);

    // emplace 所有节点
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

    // 运行管线（并行执行同一层的节点）
    pipeline.run();

    return 0;
}
