#pragma once
static inline std::string_view dynamic_meta_template =
R"(#pragma once
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/RuntimeRegistry.hpp>

namespace lux::cxx::dref::runtime {

{% for class in classes %}
{% for method in class.methods %}
// (A) 成员函数桥接
static void {{ class.name }}_{{ method.name }}_invoker(void* obj, void** args, void* retVal)
{
    auto self = static_cast<{{ class.name }}*>(obj);
    {% for p in method.parameters %}
    auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
    {% endfor %}
    
    {% if method.return_type == "void" %}
    self->{{ method.name }}({% for p in method.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
    {% else %}
    auto result = self->{{ method.name }}(
        {% for p in method.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %}
    );
    if(retVal) {
        *static_cast<{{ method.return_type }}*>(retVal) = result;
    }
    {% endif %}
}
{% endfor %}

{% for sm in class.static_methods %}
// (B) 静态函数桥接
static void {{ class.name }}_{{ sm.name }}_static_invoker(void** args, void* retVal)
{
    {% for p in sm.parameters %}
    auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
    {% endfor %}

    {% if sm.return_type == "void" %}
    {{ class.name }}::{{ sm.name }}({% for p in sm.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
    {% else %}
    auto result = {{ class.name }}::{{ sm.name }}(
        {% for p in sm.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %}
    );
    if(retVal) {
        *static_cast<{{ sm.return_type }}*>(retVal) = result;
    }
    {% endif %}
}
{% endfor %}

{% for ctor in class.constructors %}
// 对应签名: {{ ctor.ctor_name }}
static void* {{ ctor.bridge_name }}(void** args)
{
    {% for p in ctor.parameters %}
    auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
    {% endfor %}
    return new {{ class.name }}(
        {% for p in ctor.parameters %}
            arg{{ p.index }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    );
}
{% endfor %}

//===========================
// 2) 为每个函数信息生成“静态”MethodRuntimeMeta
//===========================
namespace {
// -- (A') 成员方法 static 对象
{% for method in class.methods %}
static MethodRuntimeMeta s_{{ class.name }}_{{ method.name }}_method {
    .name = "{{ method.name }}",
    .invoker = &{{ class.name }}_{{ method.name }}_invoker,
    .param_type_names = {
        {% for p in method.parameters %}
        "{{ p.type_name }}"{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .return_type = "{{ method.return_type }}",
    .is_virtual = {{ method.is_virtual }},
    .is_const   = {{ method.is_const }},
    .is_static  = false
};
{% endfor %}

// -- (B') 静态方法 static 对象
{% for sm in class.static_methods %}
static MethodRuntimeMeta s_{{ class.name }}_{{ sm.name }}_static_method {
    .name = "{{ sm.name }}",
    // 如果要统一invoker签名,则你可以将 invoker声明为MethodInvokerFn
    // 也可以把静态和实例分开
    .invoker = reinterpret_cast<MethodRuntimeMeta::MethodInvokerFn>(
        &{{ class.name }}_{{ sm.name }}_static_invoker
    ),
    .param_type_names = {
        {% for p in sm.parameters %}
        "{{ p.type_name }}"{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .return_type = "{{ sm.return_type }}",
    .is_virtual = false, // 对静态函数无意义
    .is_const   = false,
    .is_static  = true
};
{% endfor %}
} // end anonymous namespace


//===========================
// 3) 生成该类的 RecordRuntimeMeta
//===========================
static RecordRuntimeMeta s_meta_{{ class.name }} {
    // 名字
    .name = "{{ class.name }}",

    // 构造函数指针数组
    // 多个构造函数 => vector
    .ctor = {
        {% for ctor in class.constructors %}
        &{{ ctor.bridge_name }},
        {% endfor %}
    },

    // 如果没有析构函数, 也许你要提供一个默认?
    .dtor = [](void* ptr){ delete static_cast<{{ class.name }}*>(ptr); },

    // fields
    .fields = {
        {% for field in class.fields %}
        {
            .name       = "{{ field.name }}",
            .offset     = {{ field.offset }},
            .type_name  = "{{ field.type_name }}",
            .visibility = {{ field.visibility }},
            .is_static  = {{ field.is_static }},
        },
        {% endfor %}
    },

    // methods (vector<MethodRuntimeMeta*>)
    .methods = {
        {% for method in class.methods %}
        &s_{{ class.name }}_{{ method.name }}_method,
        {% endfor %}
    },

    // static_methods
    .static_methods = {
        {% for sm in class.static_methods %}
        &s_{{ class.name }}_{{ sm.name }}_static_method,
        {% endfor %}
    }
};

//-----------------------------
// 4) 注册到全局 Registry
//-----------------------------
static bool s_registered_{{ class.name }} = [](){
    RuntimeRegistry::instance().registerRecord(&s_meta_{{ class.name }});
    return true;
}();

 // end for classes
{% endfor %}


//--------------------------------------------
// 5) 枚举处理
//--------------------------------------------
{% for enum in enums %}
static EnumRuntimeMeta s_enum_meta_{{ enum.name }} {
    .name = "{{ enum.name }}",
    .is_scoped = {{ enum.is_scoped }},
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

} // end namespace lux::cxx::dref::runtime

)";