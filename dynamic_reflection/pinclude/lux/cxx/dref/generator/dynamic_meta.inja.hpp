#pragma once
static inline std::string_view dynamic_meta_template =
R"(
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <array>
#include "{{ include_path }}"

namespace lux::cxx::dref::runtime {

//---------------------------------------------------------------------
//  1. 基础类型 (FundamentalRuntimeMeta)
//---------------------------------------------------------------------
{% for fundamental in fundamentals %}
static FundamentalRuntimeMeta s_fundamental_meta_{{ fundamental.basic_info.hash }} 
{
    // BasicTypeMeta
    {
        .name           = "{{ fundamental.basic_info.name }}",
        .qualified_name = "{{ fundamental.basic_info.qualified_name }}",
        .hash           = {{ fundamental.basic_info.hash }},
        .kind           = {{ fundamental.basic_info.kind }}
    },
    // ObjectTypeMeta
    {
        .size      = {{ fundamental.object_info.size }},
        .alignment = {{ fundamental.object_info.alignment }}
    },
    // CVQualifier
    {
        .is_const    = {{ fundamental.cv_qualifier.is_const }},
        .is_volatile = {{ fundamental.cv_qualifier.is_volatile }}
    }
};
{% endfor %}


//---------------------------------------------------------------------
//  2. 指针 (PointerRuntimeMeta)
//---------------------------------------------------------------------
{% for pointer in pointers %}
static PointerRuntimeMeta s_pointer_meta_{{ pointer.basic_info.hash }} 
{
    // BasicTypeMeta
    {
        .name           = "{{ pointer.basic_info.name }}",
        .qualified_name = "{{ pointer.basic_info.qualified_name }}",
        .hash           = {{ pointer.basic_info.hash }},
        .kind           = {{ pointer.basic_info.kind }}
    },
    // ObjectTypeMeta
    {
        .size      = {{ pointer.object_info.size }},
        .alignment = {{ pointer.object_info.alignment }}
    },
    // CVQualifier
    {
        .is_const    = {{ pointer.cv_qualifier.is_const }},
        .is_volatile = {{ pointer.cv_qualifier.is_volatile }}
    },
    // pointee_hash
    {{ pointer.pointee_hash }}
};
{% endfor %}


//---------------------------------------------------------------------
//  3. 引用 (ReferenceRuntimeMeta) - LValue / RValue
//---------------------------------------------------------------------
{% for reference in references %}
static ReferenceRuntimeMeta s_reference_meta_{{ reference.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ reference.basic_info.name }}",
        .qualified_name = "{{ reference.basic_info.qualified_name }}",
        .hash           = {{ reference.basic_info.hash }},
        .kind           = {{ reference.basic_info.kind }}
    },
    // pointee_hash
    {{ reference.pointee_hash }}
};
{% endfor %}


//---------------------------------------------------------------------
//  4. 指向数据成员的指针 (PointerToDataMemberRuntimeMeta)
//---------------------------------------------------------------------
{% for ptm_data in pointer_to_data_members %}
static PointerToDataMemberRuntimeMeta s_ptm_data_meta_{{ ptm_data.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ ptm_data.basic_info.name }}",
        .qualified_name = "{{ ptm_data.basic_info.qualified_name }}",
        .hash           = {{ ptm_data.basic_info.hash }},
        .kind           = {{ ptm_data.basic_info.kind }}
    },
    // ObjectTypeMeta
    {
        .size      = {{ ptm_data.object_info.size }},
        .alignment = {{ ptm_data.object_info.alignment }}
    },
    // CVQualifier
    {
        .is_const    = {{ ptm_data.cv_qualifier.is_const }},
        .is_volatile = {{ ptm_data.cv_qualifier.is_volatile }}
    },
    // pointee_hash
    {{ ptm_data.pointee_hash }},
    // record_hash
    {{ ptm_data.record_hash }}
};
{% endfor %}


//---------------------------------------------------------------------
//  5. 指向方法成员的指针 (PointerToMethodRuntimeMeta)
//---------------------------------------------------------------------
{% for ptm_method in pointer_to_methods %}
static PointerToMethodRuntimeMeta s_ptm_method_meta_{{ ptm_method.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ ptm_method.basic_info.name }}",
        .qualified_name = "{{ ptm_method.basic_info.qualified_name }}",
        .hash           = {{ ptm_method.basic_info.hash }},
        .kind           = {{ ptm_method.basic_info.kind }}
    },
    // ObjectTypeMeta
    {
        .size      = {{ ptm_method.object_info.size }},
        .alignment = {{ ptm_method.object_info.alignment }}
    },
    // CVQualifier
    {
        .is_const    = {{ ptm_method.cv_qualifier.is_const }},
        .is_volatile = {{ ptm_method.cv_qualifier.is_volatile }}
    },
    // pointee_hash
    {{ ptm_method.pointee_hash }},
    // record_hash
    {{ ptm_method.record_hash }}
};
{% endfor %}


//---------------------------------------------------------------------
//  6. 数组 (ArrayRuntimeMeta)
//---------------------------------------------------------------------
{% for array_type in arrays %}
static ArrayRuntimeMeta s_array_meta_{{ array_type.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ array_type.basic_info.name }}",
        .qualified_name = "{{ array_type.basic_info.qualified_name }}",
        .hash           = {{ array_type.basic_info.hash }},
        .kind           = {{ array_type.basic_info.kind }}
    },
    // ObjectTypeMeta
    {
        .size      = {{ array_type.object_info.size }},
        .alignment = {{ array_type.object_info.alignment }}
    },
    // CVQualifier
    {
        .is_const    = {{ array_type.cv_qualifier.is_const }},
        .is_volatile = {{ array_type.cv_qualifier.is_volatile }}
    },
    // element_hash
    {{ array_type.element_hash }},
    // total size in elements or in bytes, 具体根据你需要
    {{ array_type.size }}
};
{% endfor %}


//---------------------------------------------------------------------
//  7. 函数 (FunctionRuntimeMeta) - 包含 invokable_info
//---------------------------------------------------------------------
{% for fn in free_functions %}
// 这里可选：若需要像类方法那样生成 bridge/invoker，可写类似：
static void {{ fn.bridge_name }}(void** args, void* retVal)
{
    {% for p in fn.invokable_info.parameters %}
    auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
    {% endfor %}

    {% if fn.invokable_info.return_type == "void" %}
    {{ fn.invokable_info.invokable_name }}({% for p in fn.invokable_info.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
    {% else %}
    auto ret = {{ fn.invokable_info.invokable_name }}({% for p in fn.invokable_info.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
    if(retVal) {
        *static_cast<{{ fn.invokable_info.return_type }}*>(retVal) = ret;
    }
    {% endif %}
}

static FunctionRuntimeMeta s_func_meta_{{ fn.basic_info.hash }} 
{
    // BasicTypeMeta
    {
        .name           = "{{ fn.basic_info.name }}",
        .qualified_name = "{{ fn.basic_info.qualified_name }}",
        .hash           = {{ fn.basic_info.hash }},
        .kind           = {{ fn.basic_info.kind }}
    },
    // InvokableTypeMeta
    {
        .mangling         = "{{ fn.invokable_info.mangling }}",
        .is_variadic      = {{ fn.invokable_info.is_variadic }},
        .return_type_hash = {{ fn.invokable_info.return_type_hash }},
        .param_type_hashs = {
            {% for p in fn.invokable_info.param_type_hashs %}
            {{ p }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        },
        .invoker = &{{ fn.bridge_name }}
    }
};
{% endfor %}


//---------------------------------------------------------------------
//  8. 方法 (MethodRuntimeMeta) - 如果你单独存放，不跟 RecordRuntimeMeta 的 methods 合在一起
//---------------------------------------------------------------------
{% for method in methods %}
static void {{ method.bridge_name }}(void* obj, void** args, void* retVal)
{
    // 生成和类内方法调用类似的 invoker...
    // ...
}

static MethodRuntimeMeta s_method_meta_{{ method.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ method.basic_info.name }}",
        .qualified_name = "{{ method.basic_info.qualified_name }}",
        .hash           = {{ method.basic_info.hash }},
        .kind           = {{ method.basic_info.kind }}
    },
    // InvokableTypeMeta
    {
        .mangling         = "{{ method.invokable_info.mangling }}",
        .is_variadic      = {{ method.invokable_info.is_variadic }},
        .return_type_hash = {{ method.invokable_info.return_type_hash }},
        .param_type_hashs = {
            {% for p in method.invokable_info.param_type_hashs %}
            {{ p }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        },
        // 这里 method 的 invoker 签名和 FunctionRuntimeMeta 不太一样
        .invoker = &{{ method.bridge_name }}
    },
    // CVQualifier
    {
        .is_const    = {{ method.cv_qualifier.is_const }},
        .is_volatile = {{ method.cv_qualifier.is_volatile }}
    },
    .visibility = {{ method.visibility }},
    .is_virtual = {{ method.is_virtual }}
};
{% endfor %}


//---------------------------------------------------------------------
//  9. 字段 (FieldRuntimeMeta)
//---------------------------------------------------------------------
{% for field in fields %}
static void get_{{ field.basic_info.hash }}(void* obj, void* outVal)
{
    // ...
}
static void set_{{ field.basic_info.hash }}(void* obj, void* inVal)
{
    // ...
}

static FieldRuntimeMeta s_field_meta_{{ field.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ field.basic_info.name }}",
        .qualified_name = "{{ field.basic_info.qualified_name }}",
        .hash           = {{ field.basic_info.hash }},
        .kind           = {{ field.basic_info.kind }}
    },
    // ObjectTypeMeta
    {
        .size      = {{ field.object_info.size }},
        .alignment = {{ field.object_info.alignment }}
    },
    // CVQualifier
    {
        .is_const    = {{ field.cv_qualifier.is_const }},
        .is_volatile = {{ field.cv_qualifier.is_volatile }}
    },
    // offset
    .offset     = {{ field.offset }},
    .visibility = {{ field.visibility }},

    // 如果需要静态判断
    .getter = &get_{{ field.basic_info.hash }},
    .setter = &set_{{ field.basic_info.hash }}
};
{% endfor %}


//---------------------------------------------------------------------
// 10. 记录 (RecordRuntimeMeta) - 类/结构体
//---------------------------------------------------------------------
{% for record in records %}
// 你也可以在这里给每个字段/method 生成独立的 invoker 函数；
// 或者只记录 meta，具体看你需求。

static RecordRuntimeMeta s_record_meta_{{ record.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ record.basic_info.name }}",
        .qualified_name = "{{ record.basic_info.qualified_name }}",
        .hash           = {{ record.basic_info.hash }},
        .kind           = {{ record.basic_info.kind }}
    },
    // ObjectTypeMeta
    {
        .size      = {{ record.object_info.size }},
        .alignment = {{ record.object_info.alignment }}
    },
    // base_hashs
    {
        {% for base_hash in record.base_hashs %}
        {{ base_hash }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    // ctor_hashs
    {
        {% for chash in record.ctor_hashs %}
        {{ chash }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    // dtor_hash, 这里若你有

    // field_meta_hashs
    {
        {% for fh in record.field_meta_hashs %}
        {{ fh }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    // method_meta_hashs
    {
        {% for mh in record.method_meta_hashs %}
        {{ mh }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    // static_method_metax_hashs
    {
        {% for smh in record.static_method_metax_hashs %}
        {{ smh }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },

    // get_field_func_t
    // 如果你有自定义 get_field_func，需要实现后赋值；否则给 nullptr
    nullptr
};
{% endfor %}


//---------------------------------------------------------------------
// 11. 枚举 (EnumRuntimeMeta)
//---------------------------------------------------------------------
{% for en in enums %}
static EnumRuntimeMeta s_enum_meta_{{ en.basic_info.hash }}
{
    // BasicTypeMeta
    {
        .name           = "{{ en.basic_info.name }}",
        .qualified_name = "{{ en.basic_info.qualified_name }}",
        .hash           = {{ en.basic_info.hash }},
        .kind           = {{ en.basic_info.kind }}
    },
    // is_scoped
    {{ en.is_scoped }},
    // underlying_type_hash
    {{ en.underlying_type_hash }},
    // enumerators
    {
        {% for enumerator in en.enumerators %}
        EnumRuntimeMeta::Enumerator {
            .name           = "{{ enumerator.name }}",
            .signed_value   = {{ enumerator.signed_value }},
            .unsigned_value = {{ enumerator.unsigned_value }}
        }{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    }
};
{% endfor %}


//---------------------------------------------------------------------
//  12. 注册入口函数
//---------------------------------------------------------------------
void register_reflections_{{ file_hash }}(RuntimeRegistry& registry)
{
    // 先注册 fundamentals
    {% for fundamental in fundamentals %}
    registry.registerMeta(&s_fundamental_meta_{{ fundamental.basic_info.hash }});
    {% endfor %}

    // 指针
    {% for pointer in pointers %}
    registry.registerMeta(&s_pointer_meta_{{ pointer.basic_info.hash }});
    {% endfor %}

    // 引用
    {% for reference in references %}
    registry.registerMeta(&s_reference_meta_{{ reference.basic_info.hash }});
    {% endfor %}

    // 指向数据成员的指针
    {% for ptm_data in pointer_to_data_members %}
    registry.registerMeta(&s_ptm_data_meta_{{ ptm_data.basic_info.hash }});
    {% endfor %}

    // 指向方法成员的指针
    {% for ptm_method in pointer_to_methods %}
    registry.registerMeta(&s_ptm_method_meta_{{ ptm_method.basic_info.hash }});
    {% endfor %}

    // 数组
    {% for array_type in arrays %}
    registry.registerMeta(&s_array_meta_{{ array_type.basic_info.hash }});
    {% endfor %}

    // 独立函数
    {% for fn in free_functions %}
    registry.registerMeta(&s_func_meta_{{ fn.basic_info.hash }});
    {% endfor %}

    // 方法(如果你单独管理, 不放在 record 里)
    {% for method in methods %}
    registry.registerMeta(&s_method_meta_{{ method.basic_info.hash }});
    {% endfor %}

    // 字段(如果你单独管理, 不放在 record 里)
    {% for field in fields %}
    registry.registerMeta(&s_field_meta_{{ field.basic_info.hash }});
    {% endfor %}

    // 记录
    {% for record in records %}
    registry.registerMeta(&s_record_meta_{{ record.basic_info.hash }});
    {% endfor %}

    // 枚举
    {% for en in enums %}
    registry.registerMeta(&s_enum_meta_{{ en.basic_info.hash }});
    {% endfor %}
}

} // end namespace lux::cxx::dref::runtime
)";
