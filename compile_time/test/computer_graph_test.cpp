#include <lux/cxx/compile_time/computer_graph.hpp>
#include <string>

// 定义一个 Resource
struct MyResource {
    int resource_id = 999;
};

// 定义节点 A：无输入 => out_binding<0,int>
struct NodeA 
  : lux::cxx::NodeBase<NodeA,
                       lux::cxx::node_descriptor<>,   // 没有输入
                       lux::cxx::node_descriptor< lux::cxx::out_binding<0,int> >,
                       // mapping_tuple
                       lux::cxx::node_dependency_map<0,void,0> // 占位
                      >
{
    void execute()
    {
        std::cout << "NodeA执行，输出一个int=123\n";
        // 这里演示一下如何写输出？ 
        //   需要的是 getOutputRef<NodeA,0,int>() => 但是需要pipeline
        //   通常可以改成 NodeBaseWithPipeline那样带 pipeline_ 指针
        //   在此简化写成：this->xxx 
    }
};

// 定义节点 B：输入 <0,int> => 来自 NodeA<0>
//            输出 <0,std::string>
struct NodeB
  : lux::cxx::NodeBase<NodeB,
                       lux::cxx::node_descriptor< lux::cxx::in_binding<0,int> >,
                       lux::cxx::node_descriptor< lux::cxx::out_binding<0,std::string> >,
                       // mapping
                       lux::cxx::node_dependency_map<0, NodeA, 0>
                      >
{
    void execute()
    {
        std::cout << "NodeB执行：读取 NodeA 的 out<0>, 然后产生自己的 out<0>\n";
    }
};

int main()
{
    using namespace lux::cxx;

    // 未排序节点
    using Unsorted = std::tuple<NodeB, NodeA>;
    // 拓扑排序
    using Sorted   = typename topological_sort<Unsorted>::type;
    // => 可能是 <NodeA, NodeB>

    // 构建 Pipeline
    Pipeline<Sorted, MyResource> pipeline;
    pipeline.emplaceNode<NodeA>();
    pipeline.emplaceNode<NodeB>();

    pipeline.run(); // 先执行 NodeA，后执行 NodeB
    return 0;
}