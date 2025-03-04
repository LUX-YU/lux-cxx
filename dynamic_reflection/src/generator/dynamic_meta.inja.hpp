#pragma once
static inline std::string_view static_meta_template =
R"(#pragma once
{%--  RuntimeMetaGen.inja --%}
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/RuntimeRegistry.hpp>

// 此文件由工具自动生成，请勿手动修改。
// 你可以在此文件顶部按需加入更多的 #include

namespace lux::cxx::dref::runtime {
//--------------------------------------------
// 1) 针对所有的类/结构，先生成“桥接函数”
//--------------------------------------------
    {% for record in records %}
    // {{ record.name }}
    {% for method in record.methods %}
    static void {{ record.name }}_{{ method.name }}_invoker(void* obj, void** args, void* retVal)
    {
        auto self = static_cast<{{ record.name }}*>(obj);
        {%-- 假设 method.parameters 是一个列表，每个元素有type_name, index等 --%}
        // 解包参数
        {% for p in method.parameters %}
        auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
        {% endfor %}
        
        {% if method.return_type_name == "void" %}
        // 如果返回类型是 void，就直接调用
        self->{{ method.name }}({% for p in method.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
        {% else %}
        auto result = self->{{ method.name }}(
            {% for p in method.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %}
        );
        if(retVal) {
            *static_cast<{{ method.return_type_name }}*>(retVal) = result;
        }
        {% endif %}
    }
    {% endfor %}

    //--------------------------------------------
    // 2) 生成该类的 RecordRuntimeMeta
    //--------------------------------------------
    static RecordRuntimeMeta s_meta_{{ record.name }} {
        // 这里也可以存个 name
        .name = "{{ record.name }}",
    
        // ctor / dtor 如果是无参构造
        .ctor = []() -> void* {
            return new {{ record.name }}();
        },
        .dtor = [](void* ptr){
            delete static_cast<{{ record.name }}*>(ptr);
        },
    
        // fields
        .fields = {
            {% for field in record.fields %}
            {
                .name = "{{ field.name }}",
                .offset = {{ field.offset }},
                .type_name = "{{ field.type_name }}",
                .visibility = {{ field.visibility }},
                .is_static = {{ field.is_static|to_bool }},
            },
            {% endfor %}
        },
    
        // methods
        .methods = {
            {% for method in record.methods %}
            new MethodRuntimeMeta {
                .name = "{{ method.name }}",
                .invoker = &{{ record.name }}_{{ method.name }}_invoker,
                .param_type_names = {
                    {% for p in method.parameters %}
                    "{{ p.type_name }}"{% if not loop.is_last %}, {% endif %}
                    {% endfor %}
                },
                .return_type_name = "{{ method.return_type_name }}",
                .is_virtual = {{ method.is_virtual|to_bool }},
                .is_const   = {{ method.is_const|to_bool }},
                .is_static  = false // 这里假设是实例方法
            },
            {% endfor %}
        },
    
        // static_methods
        .static_methods = {
            {% for s_method in record.static_methods %}
            // 跟上面类似，或者另外生成 invoker
            new MethodRuntimeMeta{
                .name = "{{ s_method.name }}",
                // ...
                .is_static = true
            },
            {% endfor %}
        }
    };
    
    //--------------------------------------------
    // 3) 把它注册到全局 RuntimeRegistry
    //--------------------------------------------
    static bool s_registered_{{ record.name }} = [](){
        RuntimeRegistry::instance().registerRecord(&s_meta_{{ record.name }});
        return true;
    }();
    
    {% endfor %}

    //--------------------------------------------
    // 4) 枚举处理
    //--------------------------------------------
    {% for enum in enums %}
    static EnumRuntimeMeta s_enum_meta_{{ enum.name }} {
        .name = "{{ enum.name }}",
        .is_scoped = {{ enum.is_scoped|to_bool }},
        .underlying_type_name = "{{ enum.underlying_type_name }}",
        .enumerators = {
            {% for enumerator in enum.enumerators %}
            EnumRuntimeMeta::Enumerator{
                .name = "{{ enumerator.name }}",
                .value = {{ enumerator.value }}
            },
            {% endfor %}
        }
    };
    
    static bool s_enum_registered_{{ enum.name }} = [](){
        RuntimeRegistry::instance().registerEnum(&s_enum_meta_{{ enum.name }});
        return true;
    }();
    
   {% endfor %}
} // end namespace
";