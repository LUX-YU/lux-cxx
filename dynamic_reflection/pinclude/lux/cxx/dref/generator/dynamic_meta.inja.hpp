#pragma once
#include <string_view>
#include <array>

/**
 * @file dynamic_meta_template.hpp
 * @brief Templated code for generating runtime reflection metadata (no default memcpy).
 *
 * This file is only a string_view-based template, used by the reflection generator.
 * It has been modified to:
 *  1. Remove default_object_setter_ / mover_ that used memcpy,
 *  2. Provide simple "copy_Xxx(...)" / "move_Xxx(...)" functions that do `operator=` or `std::move`,
 *  3. Set get_ptr_fn in FieldRuntimeMeta by default.
 */

 // ========== 1) 通用 Header 模板 ==========

static constexpr inline std::string_view dynamic_meta_template_header =
R"(#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <lux/cxx/dref/runtime/MetaRegistry.hpp>
#include "{{ include_path }}"

namespace lux::cxx::dref::runtime {
)";

// ========== 2) Fundamental 类型 ==========

static constexpr inline std::string_view dynamic_meta_template_fundamentals = R"(
{% for fundamental in fundamentals %}{% if not fundamental.basic_info.qualified_name == "void" %}
static void copy_{{ fundamental.basic_info.hash }}(void* dst, const void* src)
{
    auto& d = *reinterpret_cast<{{ fundamental.basic_info.qualified_name }}*>(dst); // or "TODO_TYPE&"
    const auto& s = *reinterpret_cast<const {{ fundamental.basic_info.qualified_name }}*>(src); 
    d = s; // if real type => d = s;
}

static void move_{{ fundamental.basic_info.hash }}(void* dst, void* src)
{
    auto& d = *reinterpret_cast<{{ fundamental.basic_info.qualified_name }}*>(dst); 
    auto& s = *reinterpret_cast<{{ fundamental.basic_info.qualified_name }}*>(src);
    d = std::move(s);
}
{% endif %}

static FundamentalRuntimeMeta s_fundamental_meta_{{ fundamental.basic_info.hash }} 
{
    .basic_info = {
        .name           = "{{ fundamental.basic_info.name }}",
        .qualified_name = "{{ fundamental.basic_info.qualified_name }}",
        .hash           = {{ fundamental.basic_info.hash }},
        .kind           = {{ fundamental.basic_info.kind }}
    },
    .object_info = {
        .size      = {{ fundamental.object_info.size }},
        .alignment = {{ fundamental.object_info.alignment }},
        {% if not fundamental.basic_info.qualified_name == "void" %}
        .copy_fn   = &copy_{{ fundamental.basic_info.hash }},
        .move_fn   = &move_{{ fundamental.basic_info.hash }}
        {% endif %}
    },
    .cv_qualifier = {
        .is_const    = {{ fundamental.cv_qualifier.is_const }},
        .is_volatile = {{ fundamental.cv_qualifier.is_volatile }}
    }
};
{% endfor %}
)";

// ========== 3) Pointer / Reference / PointerToDataMember / PointerToMethod 模板 ==========

static constexpr inline std::string_view dynamic_meta_template_ptr_ref = R"(
{% for pointer in pointers %}
static void copy_{{ pointer.basic_info.hash }}(void* dst, const void* src)
{
    // For pointers, we might do a direct assignment. 
    auto& d = *reinterpret_cast<void**>(dst); 
    auto& s = *reinterpret_cast<void* const*>(src);
    d = s; 
}

static void move_{{ pointer.basic_info.hash }}(void* dst, void* src)
{
    auto& d = *reinterpret_cast<void**>(dst);
    auto& s = *reinterpret_cast<void**>(src);
    d = s;
    s = nullptr; // or do something else
}

static PointerRuntimeMeta s_pointer_meta_{{ pointer.basic_info.hash }} 
{
    .basic_info = {
        .name           = "{{ pointer.basic_info.name }}",
        .qualified_name = "{{ pointer.basic_info.qualified_name }}",
        .hash           = {{ pointer.basic_info.hash }},
        .kind           = {{ pointer.basic_info.kind }}
    },
    .object_info = {
        .size      = {{ pointer.object_info.size }},
        .alignment = {{ pointer.object_info.alignment }},
        .copy_fn   = &copy_{{ pointer.basic_info.hash }},
        .move_fn   = &move_{{ pointer.basic_info.hash }}
    },
    .cv_qualifier = {
        .is_const    = {{ pointer.cv_qualifier.is_const }},
        .is_volatile = {{ pointer.cv_qualifier.is_volatile }}
    },
    .pointee_hash = {{ pointer.pointee_hash }}
};
{% endfor %}

{% for reference in references %}
static ReferenceRuntimeMeta s_reference_meta_{{ reference.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ reference.basic_info.name }}",
        .qualified_name = "{{ reference.basic_info.qualified_name }}",
        .hash           = {{ reference.basic_info.hash }},
        .kind           = {{ reference.basic_info.kind }}
    },
    .pointee_hash = {{ reference.pointee_hash }}
};
{% endfor %}

{% for ptm_data in pointer_to_data_members %}
static void copy_{{ ptm_data.basic_info.hash }}(void* dst, const void* src)
{
    auto& d = *reinterpret_cast<void**>(dst);
    auto& s = *reinterpret_cast<void* const*>(src);
    d = s;
}
static void move_{{ ptm_data.basic_info.hash }}(void* dst, void* src)
{
    auto& d = *reinterpret_cast<void**>(dst);
    auto& s = *reinterpret_cast<void**>(src);
    d = s;
    s = nullptr;
}

static PointerToDataMemberRuntimeMeta s_ptm_data_meta_{{ ptm_data.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ ptm_data.basic_info.name }}",
        .qualified_name = "{{ ptm_data.basic_info.qualified_name }}",
        .hash           = {{ ptm_data.basic_info.hash }},
        .kind           = {{ ptm_data.basic_info.kind }}
    },
    .object_info = {
        .size      = {{ ptm_data.object_info.size }},
        .alignment = {{ ptm_data.object_info.alignment }},
        .copy_fn   = &copy_{{ ptm_data.basic_info.hash }},
        .move_fn   = &move_{{ ptm_data.basic_info.hash }}
    },
    .cv_qualifier = {
        .is_const    = {{ ptm_data.cv_qualifier.is_const }},
        .is_volatile = {{ ptm_data.cv_qualifier.is_volatile }}
    },
    .pointee_hash = {{ ptm_data.pointee_hash }},
    .record_hash  = {{ ptm_data.record_hash }}
};
{% endfor %}

{% for ptm_method in pointer_to_methods %}
static void copy_{{ ptm_method.basic_info.hash }}(void* dst, const void* src)
{
    auto& d = *reinterpret_cast<void**>(dst);
    auto& s = *reinterpret_cast<void* const*>(src);
    d = s;
}
static void move_{{ ptm_method.basic_info.hash }}(void* dst, void* src)
{
    auto& d = *reinterpret_cast<void**>(dst);
    auto& s = *reinterpret_cast<void**>(src);
    d = s;
    s = nullptr;
}

static PointerToMethodRuntimeMeta s_ptm_method_meta_{{ ptm_method.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ ptm_method.basic_info.name }}",
        .qualified_name = "{{ ptm_method.basic_info.qualified_name }}",
        .hash           = {{ ptm_method.basic_info.hash }},
        .kind           = {{ ptm_method.basic_info.kind }}
    },
    .object_info = {
        .size      = {{ ptm_method.object_info.size }},
        .alignment = {{ ptm_method.object_info.alignment }},
        .copy_fn   = &copy_{{ ptm_method.basic_info.hash }},
        .move_fn   = &move_{{ ptm_method.basic_info.hash }}
    },
    .cv_qualifier = {
        .is_const    = {{ ptm_method.cv_qualifier.is_const }},
        .is_volatile = {{ ptm_method.cv_qualifier.is_volatile }}
    },
    .pointee_hash = {{ ptm_method.pointee_hash }},
    .record_hash  = {{ ptm_method.record_hash }}
};
{% endfor %}
)";

// ========== 4) Array 类型 ==========

static constexpr inline std::string_view dynamic_meta_template_array = R"(
{% for array_type in arrays %}
static void copy_{{ array_type.basic_info.hash }}(void* dst, const void* src)
{
    // For an array, naive approach could be a for-loop or operator= if possible.
    // Here we demonstrate a for-loop (or we do the same as pointer).
    // Real code would do typed logic.
    auto* d = static_cast<char*>(dst);
    auto* s = static_cast<char*>(src);
    for(size_t i=0; i<{{ array_type.object_info.size }}; ++i) {
        d[i] = s[i];
    }
}

static void move_{{ array_type.basic_info.hash }}(void* dst, void* src)
{
    auto* d = static_cast<char*>(dst);
    auto* s = static_cast<char*>(src);
    for(size_t i=0; i<{{ array_type.object_info.size }}; ++i) {
        d[i] = s[i];
        s[i] = 0; // or do something else
    }
}

static ArrayRuntimeMeta s_array_meta_{{ array_type.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ array_type.basic_info.name }}",
        .qualified_name = "{{ array_type.basic_info.qualified_name }}",
        .hash           = {{ array_type.basic_info.hash }},
        .kind           = {{ array_type.kind }}
    },
    .object_info = {
        .size      = {{ array_type.object_info.size }},
        .alignment = {{ array_type.object_info.alignment }},
        .copy_fn   = &copy_{{ array_type.basic_info.hash }},
        .move_fn   = &move_{{ array_type.basic_info.hash }}
    },
    .cv_qualifier = {
        .is_const    = {{ array_type.cv_qualifier.is_const }},
        .is_volatile = {{ array_type.cv_qualifier.is_volatile }}
    },
    .element_hash = {{ array_type.element_hash }},
    .size         = {{ array_type.size }}
};
{% endfor %}
)";

// ========== 5) Free Function 模板 ==========

static constexpr inline std::string_view dynamic_meta_template_function = R"(
{% for fn in free_functions %}
static void s_func_meta_invoker_{{ fn.basic_info.hash }}(void** args, void* retVal)
{
    {% for p in fn.invokable_info.parameters -%}
    auto&& arg{{ p.index }} = *static_cast<{{ p.type_name_remove_ref }}*>(args[{{ p.index }}]);
    {% endfor %}

    {% if fn.invokable_info.return_type == "void" %}
    {{ fn.invokable_info.invokable_name }}(
        {% for p in fn.invokable_info.parameters -%}
        {% if p.is_rvalue_ref %}std::move(arg{{ p.index }}){% else %}arg{{ p.index }}{% endif %}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    );
    {% else %}
    auto ret = {{ fn.invokable_info.invokable_name }}(
        {% for p in fn.invokable_info.parameters -%}
        {% if p.is_rvalue_ref %}std::move(arg{{ p.index }}){% else %}arg{{ p.index }}{% endif %}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    );
    if(retVal) {
        *static_cast<{{ fn.invokable_info.return_type }}*>(retVal) = ret;
    }
    {% endif %}
}

static FunctionRuntimeMeta s_func_meta_{{ fn.basic_info.hash }} 
{
    .basic_info = {
        .name           = "{{ fn.basic_info.name }}",
        .qualified_name = "{{ fn.basic_info.qualified_name }}",
        .hash           = {{ fn.basic_info.hash }},
        .kind           = {{ fn.basic_info.kind }}
    },
    .invokable_info = {
        .mangling         = "{{ fn.invokable_info.mangling }}",
        .return_type_hash = {{ fn.invokable_info.return_type_hash }},
        .param_type_hashs = {
            {% for p in fn.invokable_info.param_type_hashs -%}
            {{ p }}{% if not loop.is_last %}, 
            {% endif %}{% endfor %}
        },
        .is_variadic      = {{ fn.invokable_info.is_variadic }},
        .invoker = (InvokableTypeMeta::function_t)&s_func_meta_invoker_{{ fn.basic_info.hash }}
    }
};
{% endfor %}
)";

// ========== 6) Record / Method / Field 模板 ==========

static constexpr inline std::string_view dynamic_meta_template_records = R"(
{% for method in methods %}
{% if method.is_constructor %}

// ---------- Constructor ----------
{% if (not method.is_public) or method.class_is_abstract %}
  static MethodRuntimeMeta s_method_meta_{{ method.basic_info.hash }}
  {
      .basic_info = {
          .name           = "{{ method.basic_info.name }}",
          .qualified_name = "{{ method.basic_info.qualified_name }}",
          .hash           = {{ method.basic_info.hash }},
          .kind           = {{ method.basic_info.kind }}
      },
      .invokable_info = {
          .mangling         = "{{ method.invokable_info.mangling }}",
          .return_type_hash = {{ method.invokable_info.return_type_hash }},
          .param_type_hashs = {
              {% for p in method.invokable_info.param_type_hashs -%}
              {{ p }}{% if not loop.is_last %}, {% endif %}
              {% endfor %}
          },
          .is_variadic      = {{ method.invokable_info.is_variadic }},
          .invoker = (InvokableTypeMeta::ctor_t)nullptr // 构造不可用
      },
      .cv_qualifier = {
          .is_const    = {{ method.cv_qualifier.is_const }},
          .is_volatile = {{ method.cv_qualifier.is_volatile }}
      },
      .visibility  = {{ method.visibility }},
      .is_virtual  = {{ method.is_virtual }}
  };

{% else %}
static void* s_ctor_invoke_{{ method.basic_info.hash }}(bool placement, void** args)
{
    // If placement==true, args[0] is the memory address
    // Then we forward the rest of arguments
    if(placement) {
        {% for p in method.invokable_info.parameters %}
        auto&& arg{{ p.index + 1 }} = *static_cast<{{ p.type_name_remove_ref }}*>(args[{{ p.index + 1 }}]);
        {% endfor %}

        auto memPtr = static_cast<unsigned char*>( args[0] );
        new(memPtr) {{ method.class_qualified_name }}(
            {% for p in method.invokable_info.parameters -%}
                {% if p.is_rvalue_ref %}std::move(arg{{ p.index + 1 }}){% else %}arg{{ p.index + 1 }}{% endif %}{% if not loop.is_last %}, 
            {% endif %}{% endfor %}
        );
        return memPtr;
    }
    else {
        {% for p in method.invokable_info.parameters %}
        auto&& arg{{ p.index }} = *static_cast<{{ p.type_name_remove_ref }}*>(args[{{ p.index }}]);
        {% endfor %}
        return new {{ method.class_qualified_name }}(
            {% for p in method.invokable_info.parameters -%}
                {% if p.is_rvalue_ref %}std::move(arg{{ p.index }}){% else %}arg{{ p.index }}{% endif %}{% if not loop.is_last %}, 
            {% endif %}{% endfor %}
        );
    }
}

static MethodRuntimeMeta s_method_meta_{{ method.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ method.basic_info.name }}",
        .qualified_name = "{{ method.basic_info.qualified_name }}",
        .hash           = {{ method.basic_info.hash }},
        .kind           = {{ method.basic_info.kind }}
    },
    .invokable_info = {
        .mangling         = "{{ method.invokable_info.mangling }}",
        .return_type_hash = {{ method.invokable_info.return_type_hash }},
        .param_type_hashs = {
            {% for p in method.invokable_info.param_type_hashs -%}
            {{ p }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        },
        .is_variadic      = {{ method.invokable_info.is_variadic }},
        .invoker = (InvokableTypeMeta::ctor_t)&s_ctor_invoke_{{ method.basic_info.hash }}
    },
    .cv_qualifier = {
        .is_const    = {{ method.cv_qualifier.is_const }},
        .is_volatile = {{ method.cv_qualifier.is_volatile }}
    },
    .visibility  = {{ method.visibility }},
    .is_virtual  = {{ method.is_virtual }}
};
{% endif %}  // end if (class_is_abstract or not public)

{% else %}  // Not constructor => normal or destructor or ?

// ---------- Normal Method ----------
{% if method.is_public %}
static void s_method_invoke_{{ method.basic_info.hash }}(void* obj, void** args, void* retVal)
{
    auto self = static_cast<{{ method.class_qualified_name }}*>(obj);

    {% for p in method.invokable_info.parameters %}
    auto&& arg{{ p.index }} = *static_cast<{{ p.type_name_remove_ref }}*>(args[{{ p.index }}]);
    {% endfor %}

    {% if method.invokable_info.return_type == "void" %}
    self->{{ method.invokable_info.invokable_name }}(
        {% for p in method.invokable_info.parameters -%}
        {% if p.is_rvalue_ref %}std::move(arg{{ p.index }}){% else %}arg{{ p.index }}{% endif %}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    );
    {% else %}
    auto ret = self->{{ method.invokable_info.invokable_name }}(
        {% for p in method.invokable_info.parameters -%}
        {% if p.is_rvalue_ref %}std::move(arg{{ p.index }}){% else %}arg{{ p.index }}{% endif %}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    );
    if(retVal) {
        *static_cast<{{ method.invokable_info.return_type }}*>(retVal) = ret;
    }
    {% endif %}
}
{% endif %}

static MethodRuntimeMeta s_method_meta_{{ method.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ method.basic_info.name }}",
        .qualified_name = "{{ method.basic_info.qualified_name }}",
        .hash           = {{ method.basic_info.hash }},
        .kind           = {{ method.basic_info.kind }}
    },
    .invokable_info = {
        .mangling         = "{{ method.invokable_info.mangling }}",
        .return_type_hash = {{ method.invokable_info.return_type_hash }},
        .param_type_hashs = {
            {% for p in method.invokable_info.param_type_hashs -%}
            {{ p }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        },
        .is_variadic      = {{ method.invokable_info.is_variadic }},
        {% if method.is_public %}
        .invoker = (InvokableTypeMeta::method_t)&s_method_invoke_{{ method.basic_info.hash }}
        {% else %}
        .invoker = (InvokableTypeMeta::method_t)nullptr
        {% endif %}
    },
    .cv_qualifier = {
        .is_const    = {{ method.cv_qualifier.is_const }},
        .is_volatile = {{ method.cv_qualifier.is_volatile }}
    },
    .visibility  = {{ method.visibility }},
    .is_virtual  = {{ method.is_virtual }}
};
{% endif %}  // end if is_constructor
{% endfor %}

// ---------- Fields ----------
{% for field in fields %}{% if field.visibility == "EVisibility::PUBLIC" %}
static void* get_ptr_{{ field.basic_info.hash }}(void* obj)
{
    auto self = static_cast<{{ field.class_qualified_name }}*>(obj);
    return &self->{{ field.basic_info.name }};
}
static void get_{{ field.basic_info.hash }}(void* obj, void* outVal)
{
    auto self = static_cast<{{ field.class_qualified_name }}*>(obj);
    auto& d = *reinterpret_cast<{{ field.basic_info.type_qualified_name }}*>(outVal);
    d = self->{{ field.basic_info.name }};
}
static void set_{{ field.basic_info.hash }}(void* obj, void* inVal)
{
    auto self = static_cast<{{ field.class_qualified_name }}*>(obj);
    auto& s = *reinterpret_cast<{{ field.basic_info.type_qualified_name }}*>(inVal);
    self->{{ field.basic_info.name }} = s;
}{% endif %}

static FieldRuntimeMeta s_field_meta_{{ field.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ field.basic_info.name }}",
        .qualified_name = "{{ field.basic_info.qualified_name }}",
        .hash           = {{ field.basic_info.hash }},
        .kind           = {{ field.basic_info.kind }}
    },
    .object_info = {
        .size      = {{ field.object_info.size }},
        .alignment = {{ field.object_info.alignment }}
    },
    .cv_qualifier = {
        .is_const    = {{ field.cv_qualifier.is_const }},
        .is_volatile = {{ field.cv_qualifier.is_volatile }}
    },
    .offset     = {{ field.offset }},
    .visibility = {{ field.visibility }},
    {% if field.visibility == "EVisibility::PUBLIC" %}
    .get_ptr_fn = &get_ptr_{{ field.basic_info.hash }},
    .get_fn     = &get_{{ field.basic_info.hash }},
    .set_fn     = &set_{{ field.basic_info.hash }}
    {% endif %}
};

{% endfor %}

// ---------- Records ----------
{% for record in records %}
static void dtor_{{ record.basic_info.hash }}(bool placement, void* obj)
{
    using T = {{ record.basic_info.qualified_name }};
    if(!placement) {
        delete static_cast<T*>(obj);
    } else {
        static_cast<T*>(obj)->~T();
    }
}

static RecordRuntimeMeta s_record_meta_{{ record.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ record.basic_info.name }}",
        .qualified_name = "{{ record.basic_info.qualified_name }}",
        .hash           = {{ record.basic_info.hash }},
        .kind           = {{ record.basic_info.kind }}
    },
    .object_info = {
        .size      = {{ record.object_info.size }},
        .alignment = {{ record.object_info.alignment }}
    },
    .base_hashs = {
        {% for base_hash in record.base_hashs -%}
        {{ base_hash }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .ctor_hashs = {
        {% for ch in record.ctor_hashs -%}
        {{ ch }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .dtor = &dtor_{{ record.basic_info.hash }},
    .field_meta_hashs = {
        {% for fh in record.field_meta_hashs -%}
        {{ fh }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .method_meta_hashs = {
        {% for mh in record.method_meta_hashs -%}
        {{ mh }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .static_method_meta_hashs = {
        {% for smh in record.static_method_meta_hashs -%}
        {{ smh }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .is_abstract = {{ record.is_abstract }}
};
{% endfor %}
)";

// ========== 7) Enum 模板 ==========

static constexpr inline std::string_view dynamic_meta_template_enum = R"(
{% for en in enums %}
static EnumRuntimeMeta s_enum_meta_{{ en.basic_info.hash }}
{
    .basic_info = {
        .name           = "{{ en.basic_info.name }}",
        .qualified_name = "{{ en.basic_info.qualified_name }}",
        .hash           = {{ en.basic_info.hash }},
        .kind           = {{ en.basic_info.kind }}
    },
    .is_scoped = {{ en.is_scoped }},
    .underlying_type_hash = {{ en.underlying_type_hash }},
    .enumerators = {
        {% for enumerator in en.enumerators -%}
        EnumRuntimeMeta::Enumerator {
            .name           = "{{ enumerator.name }}",
            .signed_value   = {{ enumerator.signed_value }},
            .unsigned_value = {{ enumerator.unsigned_value }}
        }{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    }
};
{% endfor %}
)";

// ========== 8) 注册函数 ==========

static constexpr inline std::string_view dynamic_meta_template_register_end = R"(
void register_reflections_{{ file_hash }}(RuntimeMetaRegistry& registry)
{
    {% for fundamental in fundamentals -%}
    registry.registerMeta(&s_fundamental_meta_{{ fundamental.basic_info.hash }});
    {% endfor %}

    {% for pointer in pointers -%}
    registry.registerMeta(&s_pointer_meta_{{ pointer.basic_info.hash }});
    {% endfor %}

    {% for reference in references -%}
    registry.registerMeta(&s_reference_meta_{{ reference.basic_info.hash }});
    {% endfor %}

    {% for ptm_data in pointer_to_data_members -%}
    registry.registerMeta(&s_ptm_data_meta_{{ ptm_data.basic_info.hash }});
    {% endfor %}

    {% for ptm_method in pointer_to_methods -%}
    registry.registerMeta(&s_ptm_method_meta_{{ ptm_method.basic_info.hash }});
    {% endfor %}

    {% for array_type in arrays -%}
    registry.registerMeta(&s_array_meta_{{ array_type.basic_info.hash }});
    {% endfor %}

    {% for fn in free_functions -%}
    registry.registerMeta(&s_func_meta_{{ fn.basic_info.hash }});
    {% endfor %}

    {% for method in methods -%}
    registry.registerMeta(&s_method_meta_{{ method.basic_info.hash }});
    {% endfor %}

    {% for field in fields -%}
    registry.registerMeta(&s_field_meta_{{ field.basic_info.hash }});
    {% endfor %}

    {% for record in records -%}
    registry.registerMeta(&s_record_meta_{{ record.basic_info.hash }});
    {% endfor %}

    {% for en in enums -%}
    registry.registerMeta(&s_enum_meta_{{ en.basic_info.hash }});
    {% endfor %}
}

} // end namespace lux::cxx::dref::runtime
)";

// ========== 9) 最终整体模板数组 ==========

static constexpr inline std::array<std::string_view, 7> dynamic_meta_template_strs = {
    dynamic_meta_template_header,
    dynamic_meta_template_fundamentals,
    dynamic_meta_template_ptr_ref,
    // dynamic_meta_template_array,
    dynamic_meta_template_function,
    dynamic_meta_template_records,
    dynamic_meta_template_enum,
    dynamic_meta_template_register_end
};

// ========== 10) 合并多个文件的 register 函数头 ==========

static inline std::string_view register_file_header =
R"(#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <lux/cxx/dref/runtime/MetaRegistry.hpp>

namespace lux::cxx::dref::runtime {

{% for fh in file_hashs -%}
extern void register_reflections_{{ fh }}(RuntimeMetaRegistry& registry);
{% endfor %}

static inline void {{ register_function_name }}(RuntimeMetaRegistry& registry)
{
    {% for fh in file_hashs -%}
    register_reflections_{{ fh }}(registry);
    {% endfor %}
}

}

)";