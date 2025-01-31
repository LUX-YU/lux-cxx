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
                 node_descriptor< out_binding<0,int> >>   // 输出: out<0,int>
    {
        // in_param_t = std::tuple<>  (无输入)
        // out_param_t = std::tuple<int&> (仅一个输出)
        bool execute(const in_param_t& /*in*/, out_param_t& out)
        {
            std::cout << "[NodeA] Writing out<0> = 100\n";
            // 取出 out 中的第0个引用
            std::get<0>(out) = 100;

            return true;
        }
    };

    //==============================================================================
    //  NodeB: 依赖 NodeA 的 out<0,int>
    //  Inputs: in<0,int>
    //  Outputs: out<0,std::string>
    //  Dependencies: node_dependency_map<0, NodeA, 0>
    //==============================================================================
    struct NodeB
      : NodeBase<NodeB,
                 node_descriptor< in_binding<0,int> >,
                 node_descriptor< out_binding<0,std::string> >,
                 node_dependency_map<0, NodeA, 0>>
    {
        // in_param_t = std::tuple<int&>
        // out_param_t = std::tuple<std::string&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& inputVal = std::get<0>(in);
            std::cout << "[NodeB] Read NodeA out<0> = " << inputVal << "\n";

            auto& outStr = std::get<0>(out);
            outStr = "Processed by NodeB: " + std::to_string(inputVal);
            std::cout << "[NodeB] Writing out<0> = " << outStr << "\n";

            return true;
        }
    };

    //==============================================================================
    //  NodeC: 依赖 NodeB 的 out<0,std::string>
    //  Inputs: in<0,std::string>
    //  Outputs: out<0,double>
    //  Dependencies: node_dependency_map<0, NodeB, 0>
    //==============================================================================
    struct NodeC
      : NodeBase<NodeC,
                 node_descriptor< in_binding<0,std::string> >,
                 node_descriptor< out_binding<0,double> >,
                 node_dependency_map<0, NodeB, 0>>
    {
        // in_param_t = std::tuple<std::string&>
        // out_param_t = std::tuple<double&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& inputStr = std::get<0>(in);
            std::cout << "[NodeC] Read NodeB out<0> = " << inputStr << "\n";

            auto& outDouble = std::get<0>(out);
            outDouble = 3.14;
            std::cout << "[NodeC] Writing out<0> = " << outDouble << "\n";

            return true;
        }
    };

    //==============================================================================
    //  NodeD: 依赖 NodeA out<0,int> + NodeC out<0,double>
    //  Inputs: in<0,int>, in<1,double>
    //  Outputs: out<0,std::string>
    //  Dependencies: node_dependency_map<0,NodeA,0>, node_dependency_map<1,NodeC,0>
    //==============================================================================
    struct NodeD
      : NodeBase<NodeD,
                 node_descriptor< in_binding<0,int>, in_binding<1,double> >,
                 node_descriptor< out_binding<0,std::string> >,
                 node_dependency_map<0, NodeA, 0>,
                 node_dependency_map<1, NodeC, 0>>
    {
        // in_param_t = std::tuple<int&, double&>
        // out_param_t = std::tuple<std::string&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& inputInt    = std::get<0>(in);
            auto& inputDouble = std::get<1>(in);
            std::cout << "[NodeD] Read NodeA out<0> = " << inputInt
                      << ", NodeC out<0> = " << inputDouble << "\n";

            auto& outStr = std::get<0>(out);
            outStr = "Aggregated by NodeD: " + std::to_string(inputInt)
                   + " and " + std::to_string(inputDouble);
            std::cout << "[NodeD] Writing out<0> = " << outStr << "\n";

            return true;
        }
    };

    //==============================================================================
    //  NodeE: 无依赖 => mapping_tuple 为空
    //  输出：out<0,std::string>
    //==============================================================================
    struct NodeE
      : NodeBase<NodeE,
                 node_descriptor<>,
                 node_descriptor< out_binding<0,std::string> >>
    {
        // in_param_t = std::tuple<>
        // out_param_t = std::tuple<std::string&>
        bool execute(const in_param_t& /*in*/, out_param_t& out)
        {
            std::cout << "[NodeE] Writing out<0> = \"NodeE Data\"\n";
            std::get<0>(out) = "NodeE Data";

            return true;
        }
    };

    //==============================================================================
    //  NodeF: 依赖 NodeD out<0,std::string> + NodeE out<0,std::string>
    //  Inputs: in<0,std::string>, in<1,std::string>
    //  Outputs: out<0,bool>
    //  Dependencies: node_dependency_map<0,NodeD,0>, node_dependency_map<1,NodeE,0>
    //==============================================================================
    struct NodeF
      : NodeBase<NodeF,
                 node_descriptor< in_binding<0,std::string>,
                                  in_binding<1,std::string> >,
                 node_descriptor< out_binding<0,bool> >,
                 node_dependency_map<0, NodeD, 0>,
                 node_dependency_map<1, NodeE, 0>>
    {
        // in_param_t = std::tuple<std::string&, std::string&>
        // out_param_t = std::tuple<bool&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& inputStr1 = std::get<0>(in);
            auto& inputStr2 = std::get<1>(in);
            std::cout << "[NodeF] Read NodeD out<0> = " << inputStr1
                      << ", NodeE out<0> = " << inputStr2 << "\n";

            auto& outBool = std::get<0>(out);
            outBool = (inputStr1.find("Aggregated") != std::string::npos)
                   && (inputStr2 == "NodeE Data");
            std::cout << "[NodeF] Writing out<0> = " << std::boolalpha << outBool << "\n";

            return true;
        }
    };

    //==============================================================================
    //  NodeG: 依赖 NodeB/C/A
    //  Inputs: in<0,std::string>, in<1,double>, in<2,int>
    //  Outputs: out<0,float>, out<1,std::string>
    //  Dependencies: node_dependency_map<0,NodeB,0>, node_dependency_map<1,NodeC,0>, node_dependency_map<2,NodeA,0>
    //==============================================================================
    struct NodeG
      : NodeBase<NodeG,
                 node_descriptor< in_binding<0,std::string>, in_binding<1,double>, in_binding<2,int> >,
                 node_descriptor< out_binding<0,float>, out_binding<1,std::string> >,
                 node_dependency_map<0, NodeB, 0>,
                 node_dependency_map<1, NodeC, 0>,
                 node_dependency_map<2, NodeA, 0>>
    {
        // in_param_t = std::tuple<std::string&, double&, int&>
        // out_param_t = std::tuple<float&, std::string&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& strVal = std::get<0>(in);
            auto& dblVal = std::get<1>(in);
            auto& intVal = std::get<2>(in);

            std::cout << "[NodeG] Read from NodeB out<0>: " << strVal << "\n";
            std::cout << "[NodeG] Read from NodeC out<0>: " << dblVal << "\n";
            std::cout << "[NodeG] Read from NodeA out<0>: " << intVal << "\n";

            auto& out0 = std::get<0>(out); // float&
            out0 = static_cast<float>(dblVal + intVal);

            auto& out1 = std::get<1>(out); // std::string&
            out1 = strVal + " + extra";

            std::cout << "[NodeG] Writing out<0> = " << out0
                      << ", out<1> = " << out1 << "\n";

            return true;
        }
    };

    //==============================================================================
    //  NodeH: 依赖 NodeD out<0,std::string> + NodeG out<0,float>
    //  Inputs: in<0,std::string>, in<1,float>
    //  Outputs: out<0,int>
    //  Dependencies: node_dependency_map<0,NodeD,0>, node_dependency_map<1,NodeG,0>
    //==============================================================================
    struct NodeH
      : NodeBase<NodeH,
                 node_descriptor< in_binding<0,std::string>, in_binding<1,float> >,
                 node_descriptor< out_binding<0,int> >,
                 node_dependency_map<0, NodeD, 0>,
                 node_dependency_map<1, NodeG, 0>>
    {
        // in_param_t = std::tuple<std::string&, float&>
        // out_param_t = std::tuple<int&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& dStr   = std::get<0>(in);
            auto& gFloat = std::get<1>(in);

            std::cout << "[NodeH] Read NodeD out<0>: " << dStr
                      << ", NodeG out<0>: " << gFloat << "\n";

            auto& out0 = std::get<0>(out);
            out0 = static_cast<int>(gFloat) + static_cast<int>( dStr.size() );

            std::cout << "[NodeH] Writing out<0> = " << out0 << "\n";

            return true;
        }
    };

    //==============================================================================
    //  NodeI: 依赖 NodeE out<0,std::string> + NodeH out<0,int>
    //  Inputs: in<0,std::string>, in<1,int>
    //  Outputs: out<0,bool>
    //  Dependencies: node_dependency_map<0, NodeE, 0>, node_dependency_map<1, NodeH, 0>
    //==============================================================================
    struct NodeI
      : NodeBase<NodeI,
                 node_descriptor< in_binding<0,std::string>, in_binding<1,int> >,
                 node_descriptor< out_binding<0,bool> >,
                 node_dependency_map<0, NodeE, 0>,
                 node_dependency_map<1, NodeH, 0>>
    {
        // in_param_t = std::tuple<std::string&, int&>
        // out_param_t = std::tuple<bool&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& eStr = std::get<0>(in);
            auto& hInt = std::get<1>(in);

            std::cout << "[NodeI] Read NodeE out<0>: " << eStr
                      << ", NodeH out<0>: " << hInt << "\n";

            auto& out0 = std::get<0>(out);
            out0 = (eStr == "NodeE Data") && (hInt > 20);

            std::cout << "[NodeI] Writing out<0> = " << std::boolalpha << out0 << "\n";

            return true;
        }
    };

    //==============================================================================
    //  NodeJ: 依赖 NodeH out<0,int> + NodeF out<0,bool>
    //  Inputs: in<0,int>, in<1,bool>
    //  Outputs: out<0,std::string>
    //  Dependencies: node_dependency_map<0, NodeH, 0>, node_dependency_map<1, NodeF, 0>
    //==============================================================================
    struct NodeJ
      : NodeBase<NodeJ,
                 node_descriptor< in_binding<0,int>, in_binding<1,bool> >,
                 node_descriptor< out_binding<0,std::string> >,
                 node_dependency_map<0, NodeH, 0>,
                 node_dependency_map<1, NodeF, 0>>
    {
        // in_param_t = std::tuple<int&, bool&>
        // out_param_t = std::tuple<std::string&>
        bool execute(const in_param_t& in, out_param_t& out)
        {
            auto& hInt  = std::get<0>(in);
            auto& fBool = std::get<1>(in);

            std::cout << "[NodeJ] Read NodeH out<0>: " << hInt
                      << ", NodeF out<0>: " << fBool << "\n";

            auto& out0 = std::get<0>(out);
            out0 = "NodeJ says: H=" + std::to_string(hInt)
                 + (fBool ? ", F=TRUE" : ", F=FALSE");

            std::cout << "[NodeJ] Writing out<0> = " << out0 << "\n";

            return true;
        }
    };
}

int main()
{
    using namespace lux::cxx;
    using namespace demo;

    std::cout << "==== [Parallel Pipeline Test - More Complex / Auto-Injected Style] ====\n";

    // 把所有节点都放进 unsorted 里，让 layered_topological_sort 自动处理
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
