#include <mstch/mstch.hpp>
#include <fstream>
#include <string>
#include "reflection_template.hpp" // 这就是你把上面mustache字符串内置也行，或者读外部文件

using namespace lux::cxx::dref;

// 伪代码：将 lan_model::ClassDeclaration -> mustache::node
mstch::array buildClassesContext(const MetaUnit& mu)
{
    mstch::array classesArr;

    // 假设我们只处理 "markedClassDeclarationList()" 
    // 也可以合并 unmarked+marked
    auto& classDecls = mu.markedClassDeclarationList();

    for (auto& cd : classDecls) {
        mstch::map classObj;

        // class_name 
        classObj["class_name"] = cd.name;
        classObj["qualified_name"] = cd.full_qualified_name.empty() ? cd.name : cd.full_qualified_name;

        // "class_decl_pointer" 
        //   这里需要保证在别处(或同文件) 有:
        //   static ::lux::cxx::lan_model::ClassDeclaration g_xxx
        //   让 pointer = &g_xxx
        //   这里举例写死:
        // classObj["class_decl_pointer"] = "g_decl_" + cd.name; 
        // 具体做法可视需求
        classObj["class_decl_pointer"] = std::string("g_decl_") + cd.name;

        // now fields
        mstch::array fieldArr;
        for (auto& fd : cd.field_decls) {
            mstch::map fieldObj;
            fieldObj["field_name"] = fd.name;
            // field_decl_pointer (同理)
            fieldObj["field_decl_pointer"] = std::string("g_field_") + cd.name + "_" + fd.name;
            fieldArr.push_back(fieldObj);
        }
        classObj["fields"] = fieldArr;

        // now methods (只演示member function)
        mstch::array methodArr;
        for (auto& mfd : cd.method_decls) {
            mstch::map methodObj;
            methodObj["method_name"] = mfd.name;
            methodObj["method_decl_pointer"] = std::string("g_method_") + cd.name + "_" + mfd.name;

            // example for param 
            mstch::array paramArr;
            for (size_t i = 0; i < mfd.parameter_decls.size(); i++) {
                auto& param = mfd.parameter_decls[i];
                mstch::map pm;
                pm["index"] = i;
                pm["type_name"] = param.type ? param.type->name : "/*unknown*/";
                pm["has_next"] = (i < mfd.parameter_decls.size() - 1);
                paramArr.push_back(pm);
            }
            methodObj["params"] = paramArr;

            // has_return_value => (mfd.result_type != void?)
            bool hasRet = true;
            if (mfd.result_type && mfd.result_type->type_kind == lan_model::ETypeKind::VOID_TYPE) {
                hasRet = false;
            }
            methodObj["has_return_value"] = hasRet;
            if (hasRet && mfd.result_type) {
                methodObj["return_type_name"] = mfd.result_type->name;
            }

            methodArr.push_back(methodObj);
        }
        classObj["methods"] = methodArr;

        // push to classes
        classesArr.push_back(classObj);
    }
    return classesArr;
}


int main(int argc, char* argv[])
{
    MetaUnit mu = /* ... 解析得到 ... */;

    // 1) 准备 mustache 数据 (root)
    mstch::map root;
    root["generated_namespace"] = std::string("lux::cxx::dref::generated");

    // 2) build classes array
    auto classesArr = buildClassesContext(mu);
    root["classes"] = classesArr;

    // 3) 加载模板(可以用文件I/O，也可以硬编码到字符串)
    std::string templateStr = R"(
        {{> reflection_auto_gen }}
    )";

    // 也可以 read file reflection_template.mustache 
    // e.g. std::ifstream ifs("reflection_template.mustache");
    // std::string templateStr( (std::istreambuf_iterator<char>(ifs)),
    //                          (std::istreambuf_iterator<char>()) );

    // 4) 渲染
    auto rendered = mstch::render(templateStr, root);

    // 5) 写到某个 .cpp 文件
    std::ofstream out("reflection_auto_gen.cpp");
    out << rendered;
    out.close();

    return 0;
}
