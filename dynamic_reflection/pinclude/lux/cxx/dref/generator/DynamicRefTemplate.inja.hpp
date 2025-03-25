#pragma once
#include <string_view>

static inline std::string_view fundamental_meta_template = R"(
{% for f in fundamentals %}

static void s_fundamental_setter_{{ f.name }}(void* obj, const void* inVal)
{
    *(static_cast<{{ f.name }}*>(obj)) = *(static_cast<const {{ f.name }}*>(inVal));
}

static void s_fundamental_mover_{{ f.name }}(void* dst, void* src)
{
    *(static_cast<{{ f.name }}*>(dst)) = std::move(*(static_cast<{{ f.name }}*>(src)));
}

static ::lux::cxx::dref::runtime::FundamentalRuntimeMeta s_fundamental_meta_{{ f.name }} = {
    .basic_info = {
        .name      = "{{ f.name }}",
        .hash      = {{ f.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ f.kind }},
        .type_info = {{ f.type_info }}
    },
    .object_info = {
        .size      = {{ f.size }},
        .alignment = {{ f.alignment }},
        .setter    = &s_fundamental_setter_{{ f.name }},
        .mover     = &s_fundamental_mover_{{ f.name }}
    },
    .cv_qualifier = {
        .is_const    = {{ f.is_const }},
        .is_volatile = {{ f.is_volatile }}
    }
};
{% endfor %}
)";

static inline std::string_view pointer_meta_template = R"(
{% for p in pointers %}
static void s_pointer_setter_{{ p.name }}(void* obj, const void* inVal)
{
    *(static_cast<{{ p.name }}*>(obj)) = *(static_cast<const {{ p.name }}*>(inVal));
}

static void s_pointer_mover_{{ p.name }}(void* dst, void* src)
{
    *(static_cast<{{ p.name }}*>(dst)) = std::move(*(static_cast<{{ p.name }}*>(src)));
}

static ::lux::cxx::dref::runtime::PointerRuntimeMeta s_pointer_meta_{{ p.name }} = {
    .basic_info = {
        .name      = "{{ p.name }}",
        .hash      = {{ p.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ p.kind }},
        .type_info = {{ p.type_info }}
    },
    .object_info = {
        .size      = {{ p.size }},
        .alignment = {{ p.alignment }},
        .setter    = s_pointer_setter_{{ p.name }},
        .mover     = s_pointer_mover_{{ p.name }}
    },
    .cv_qualifier = {
        .is_const    = {{ p.is_const }},
        .is_volatile = {{ p.is_volatile }}
    },
    .pointee_hash = {{ p.pointee_hash }}
};
{% endfor %}
)";

static inline std::string_view reference_meta_template = R"(
{% for r in references %}
static ::lux::cxx::dref::runtime::ReferenceRuntimeMeta s_reference_meta_{{ r.name }} = {
    .basic_info = {
        .name      = "{{ r.name }}",
        .hash      = {{ r.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ r.kind }},
        .type_info = {{ r.type_info }}
    },
    .pointee_hash = {{ r.pointee_hash }}
};
{% endfor %}
)";

static inline std::string_view array_meta_template = R"(
{% for a in arrays %}
static void s_array_setter_{{ a.name }}(void* obj, const void* inVal)
{
    *(static_cast<{{ a.name }}*>(obj)) = *(static_cast<const {{ a.name }}*>(inVal));
}

static void s_array_mover_{{ a.name }}(void* dst, void* src)
{
    *(static_cast<{{ a.name }}*>(dst)) = std::move(*(static_cast<{{ a.name }}*>(src)));
}

static ::lux::cxx::dref::runtime::ArrayRuntimeMeta s_array_meta_{{ a.name }} = {
    .basic_info = {
        .name      = "{{ a.name }}",
        .hash      = {{ a.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ a.kind }},
        .type_info = {{ a.type_info }}
    },
    .object_info = {
        .size      = {{ a.size }},
        .alignment = {{ a.alignment }},
        .setter    = &s_array_setter_{{ a.name }},
        .mover     = &s_array_mover_{{ a.name }}
    },
    .cv_qualifier = {
        .is_const    = {{ a.is_const }},
        .is_volatile = {{ a.is_volatile }}
    },
    .element_hash = {{ a.element_hash }},
    .size         = {{ a.size }}
};
{% endfor %}
)";

static inline std::string_view function_meta_template = R"(
{% for fn in functions %}

static void s_function_invoker_{{ fn.bridge_name }}(void** args, void* retVal)
{
    {% for p in fn.parameters -%}
    auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
    {% endfor %}

    {% if fn.return_type == "void" %}
        {{ fn.qualified_name }}(
            {% for p in fn.parameters %}
            arg{{ p.index }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        );
        {% else %}
        auto result = {{ fn.qualified_name }}(
            {% for p in fn.parameters %}
            arg{{ p.index }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        );
        if(retVal) {
            *static_cast<{{ fn.return_type }}*>(retVal) = result;
        }
    {% endif %}
}

static ::lux::cxx::dref::runtime::FunctionRuntimeMeta s_function_meta_{{ fn.bridge_name }} = {
    .basic_info = {
        .name      = "{{ fn.name }}",
        .hash      = {{ fn.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ fn.kind }},
        .type_info = {{ fn.type_info }}
    },
    .invokable_info = {
        .qualifed_name     = "{{ fn.qualifed_name }}",
        .mangling          = "{{ fn.mangling }}",
        .return_type_hash  = {{ fn.return_type_hash }},
        .param_type_hashs  = {
            {% for ph in fn.param_type_hashs %}
            {{ ph }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        },
        .is_variadic       = {{ fn.is_variadic }},
        .invoker           = &s_function_invoker_{{ fn.bridge_name }}
    }
};
{% endfor %}
)";

static inline std::string_view method_meta_template = R"(
{% for m in methods %}
static ::lux::cxx::dref::runtime::MethodRuntimeMeta s_method_meta_{{ m.name }} = {
    .basic_info = {
        .name      = "{{ m.name }}",
        .hash      = {{ m.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ m.kind }},
        .type_info = {{ m.type_info }}
    },
    .invokable_info = {
        .qualifed_name     = "{{ m.qualifed_name }}",
        .mangling          = "{{ m.mangling }}",
        .return_type_hash  = {{ m.return_type_hash }},
        .param_type_hashs  = {
            {% for ph in m.param_type_hashs %}
            {{ ph }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        },
        .is_variadic       = {{ m.is_variadic }},
        .invoker           = {{ m.invoker }}
    },
    .cv_qualifier = {
        .is_const    = {{ m.is_const }},
        .is_volatile = {{ m.is_volatile }}
    },
    .visibility = ::lux::cxx::dref::runtime::EVisibility::{{ m.visibility }},
    .is_virtual = {{ m.is_virtual }}
};
{% endfor %}
)";

static inline std::string_view record_meta_template = R"(
{% for rcd in records %}

{% for base in rcd.bases %}
{% endfor %}

{% if rcd.ctors.size() > 0 %}
{% for ctor in rcd.ctors %}

{% endfor %}
{% else %}

{% endif %}

{% for m in rcd.methods %}
{% endfor %}

{% for sm in rcd.static_methods %}
{% endfor %}

{% for fd in rcd.fields %}
void s_field_setter_{{ rcd.bridge_name }}_{{ fd.name }}(void* obj, const void* inVal)
{
    *(static_cast<{{ fd.type_name }}*>(obj)) = *(static_cast<const {{ fd.type_name }}*>(inVal));
}

void s_field_mover_{{ rcd.bridge_name }}_{{ fd.name }}(void* dst, void* src)
{
    *(static_cast<{{ fd.type_name }}*>(dst)) = std::move(*(static_cast<{{ fd.type_name }}*>(src)));
}

static ::lux::cxx::dref::runtime::FieldRuntimeMeta s_field_meta_{{ fd.name }} = {
    .basic_info = {
        .name      = "{{ fd.name }}",
        .hash      = {{ fd.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ fd.kind }},
        .type_info = {{ fd.type_info }}
    },
    .object_info = {
        .size      = {{ fd.size }},
        .alignment = {{ fd.alignment }},
        .setter    = &s_field_setter_{{ rcd.bridge_name }}_{{ fd.name }},
        .mover     = &s_field_mover_{{ rcd.bridge_name }}_{{ fd.name }}
    },
    .cv_qualifier = {
        .is_const    = {{ fd.is_const }},
        .is_volatile = {{ fd.is_volatile }}
    },
    .offset     = {{ fd.offset }},
    .visibility = ::lux::cxx::dref::runtime::EVisibility::{{ fd.visibility }}
};
{% endfor %}

static void s_get_field_{{ rcd.bridge_name }}(void* obj, size_t index, void** field)
{
    auto record = static_cast<{{ rcd.name }}*>(obj);
    
}

static ::lux::cxx::dref::runtime::RecordRuntimeMeta s_record_meta_{{ rcd.name }} = {
    .basic_info = {
        .name      = "{{ rcd.name }}",
        .hash      = {{ rcd.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ rcd.kind }},
        .type_info = {{ rcd.type_info }}
    },
    .object_info = {
        .size      = {{ rcd.size }},
        .alignment = {{ rcd.alignment }},
        .getter    = {{ rcd.getter }},
        .setter    = {{ rcd.setter }},
        .mover     = {{ rcd.mover }}
    },
    .base_hashs = {
        {% for bh in rcd.bases %}
        bh.hash{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .ctor = {
        {% for ctor_fn in rcd.ctor %}
        s_function_meta_{{ ctor_fn }},
        {% endfor %}
    },
    .dtor = s_function_meta_{{ rcd.dtor }},
    .fields = {
        {% for fd in rcd.fields %}
        s_field_meta_{{ fd }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .methods = {
        {% for m in rcd.methods %}
        s_method_meta_{{ m }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    },
    .static_methods = {
        {% for sm in rcd.static_methods %}
        s_function_meta_{{ sm }}{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    }
};
{% endfor %}
)";

static inline std::string_view enum_meta_template = R"(
{% for en in enums %}
static ::lux::cxx::dref::runtime::EnumRuntimeMeta s_enum_meta_{{ en.name }} = {
    .basic_info = {
        .name      = "{{ en.name }}",
        .hash      = {{ en.hash }},
        .kind      = ::lux::cxx::dref::runtime::ETypeKinds::{{ en.kind }},
        .type_info = {{ en.type_info }}
    },
    .is_scoped             = {{ en.is_scoped }},
    .underlying_type_hash  = {{ en.underlying_type_hash }},
    .enumerators = {
        {% for enumerator in en.enumerators %}
        {
            .name           = "{{ enumerator.name }}",
            .signed_value   = {{ enumerator.signed_value }},
            .unsigned_value = {{ enumerator.unsigned_value }}
        }{% if not loop.is_last %}, {% endif %}
        {% endfor %}
    }
};
{% endfor %}
)";
