#pragma once
/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
static inline std::string_view dynamic_meta_template =
R"(#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <array>
#include "{{ include_path }}"

namespace lux::cxx::dref::runtime {
    {% for class in classes %}
    {% for field in class.fields %}
    {% if not field.is_const %}
    static void get_{{ class.extended_name }}_{{ field.name }}(void* obj, void* outVal)
    {
        {% if not field.is_static %}
        auto self = static_cast<{{ class.name }}*>(obj);
        if(!outVal) return;
        *static_cast<{{ field.type_name }}*>(outVal) = self->{{ field.name }};
        {% else %}
        *static_cast<{{ field.type_name }}*>(outVal) = {{ class.name }}::{{ field.name }};
        {% endif %}
    }
    
    static void set_{{ class.extended_name }}_{{ field.name }}(void* obj, void* inVal)
    {
        {% if not field.is_static %}
        auto self = static_cast<{{ class.name }}*>(obj);
        self->{{ field.name }} = *static_cast<{{ field.type_name }}*>(inVal);
        {% else %}
        {{ class.name }}::{{ field.name }} = *static_cast<{{ field.type_name }}*>(inVal);
        {% endif %}

    }
    {% endif %}
    {% endfor %}

    {% for method in class.methods -%}
    static void {{ class.extended_name }}_{{ method.name }}_invoker(void* obj, void** args, void* retVal)
    {
        auto self = static_cast<{{ class.name }}*>(obj);
        {% for p in method.parameters -%}
        auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
        {% endfor -%}
        {% if method.return_type == "void" %}
        self->{{ method.name }}({% for p in method.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
        {% else %}
        auto result = self->{{ method.name }}({% for p in method.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
        if(retVal) {
            *static_cast<{{ method.return_type }}*>(retVal) = result;
        }{% endif %}
    }
    {% endfor -%}

    {% for sm in class.static_methods %}
    static void {{ class.extended_name }}_{{ sm.name }}_static_invoker(void** args, void* retVal)
    {
        {% for p in sm.parameters -%}
        auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
        {% endfor %}
        {% if sm.return_type == "void" %}{{ class.name }}::{{ sm.name }}({% for p in sm.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});{% else %}
        auto result = {{ class.name }}::{{ sm.name }}({% for p in sm.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
        if(retVal) {
            *static_cast<{{ sm.return_type }}*>(retVal) = std::move(result);
        }{% endif %}
    }
    {% endfor -%}

    {% if class.constructor_num > 0 %}{% for ctor in class.constructors %}
    static void* {{ ctor.bridge_name }}(void** args)
    {
        {% for p in ctor.parameters -%}
        auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
        {% endfor %}
        return new {{ class.name }}(
            {% for p in ctor.parameters -%}
            arg{{ p.index }}{% if not loop.is_last %}, 
            {% endif %}{% endfor %}
        );
    }{% endfor -%}{% else %}
    static void* {{ class.extended_name }}_default_ctor(void** /*args*/)
    {
        return new {{ class.name }}();
    }
    {% endif %}

    static RecordRuntimeMeta s_meta_{{ class.extended_name }} {
        .name = "{{ class.name }}",
        .hash = {{ class.hash }},
        .ctor = std::vector<ConstructorRuntimeMeta>{
            {% if class.constructor_num > 0 %}{% for ctor in class.constructors -%}
            {
                .name = "{{ ctor.ctor_name }}",
                .qualified_name = "{{ ctor.qualified_name }}",
                .invoker = &{{ ctor.bridge_name }},
                .param_types = {
                    {% for p in ctor.parameters -%}
                    "{{ p.type_name }}"{% if not loop.is_last %},{% endif %}{% endfor %}
                },
                .return_type = "{{ ctor.ctor_name }}"
            }{% if not loop.is_last %},
            {% endif %}{% endfor %}{% else %}
            {
                .name           = "default_ctor",
                .qualified_name = "{{ class.name }}::{{ class.name }}",
                .invoker        = &{{ class.extended_name }}_default_ctor,
                .param_types    = {},
                .return_type    = "{{ class.name }}"
            }{% endif %}
        },
        .dtor = [](void* ptr){ delete static_cast<{{ class.name }}*>(ptr);},
        .fields = std::vector<FieldRuntimeMeta>{
            {% for field in class.fields -%}
            {
                .name       = "{{ field.name }}",
                .offset     = {{ field.offset }},
                .size       = {{ field.size }},
                .type_name  = "{{ field.type_name }}",{% if not field.is_const %}
                .getter     = &get_{{ class.extended_name }}_{{ field.name }},
                .setter     = &set_{{ class.extended_name }}_{{ field.name }},{% else %}
                .getter     = nullptr,
                .setter     = nullptr,{% endif %}
                .visibility = {{ field.visibility }},
                .is_static  = {{ field.is_static }},
            }{% if not loop.is_last %},
            {% endif %}{% endfor %}
        },
        .methods = std::vector<MethodRuntimeMeta>{
            {% for method in class.methods -%}
            {
                .name = "{{ method.name }}",
                .qualified_name = "{{ method.qualified_name }}",
                .mangling       = "{{ method.mangling }}",
                .invoker = &{{ class.extended_name }}_{{ method.name }}_invoker,
                .param_types = {
                    {% for p in method.parameters -%}
                    "{{ p.type_name }}"{% if not loop.is_last %}, {% endif %}{% endfor %}
                },
                .return_type = "{{ method.return_type }}",
                .is_virtual = {{ method.is_virtual }},
                .is_const   = {{ method.is_const }},
                .is_static  = false
            }{% if not loop.is_last %},
            {% endif %}{% endfor %}
        },
        .static_methods = std::vector<FunctionRuntimeMeta>{
            {% for sm in class.static_methods -%}
            {
                .name = "{{ sm.name }}",
                .qualified_name = "{{ sm.qualified_name }}",
                .mangling       = "{{ sm.mangling }}",
                .invoker = &{{ class.extended_name }}_{{ sm.name }}_static_invoker,
                .param_types = {
                    {% for p in sm.parameters -%}
                    "{{ p.type_name }}"{% if not loop.is_last %},{% endif %}{% endfor %}
                },  
                .return_type = "{{ sm.return_type }}"
            }{% if not loop.is_last %},
            {% endif %}{% endfor %}
        }
    };
    {% endfor -%}

    {% for enum in enums %}
    static EnumRuntimeMeta s_enum_meta_{{ enum.extended_name }} {
        .name = "{{ enum.name }}",
        .hash = {{ enum.hash }},
        .is_scoped = {{ enum.is_scoped }},
        .underlying_type_name = "{{ enum.underlying_type_name }}",
        .enumerators = {
            {% for enumerator in enum.enumerators -%}
            EnumRuntimeMeta::Enumerator{
                .name = "{{ enumerator.name }}",
                .value = {{ enumerator.value }}
            }{% if not loop.is_last %},
            {% endif %}{% endfor %}
        }
    };
    {% endfor -%}

    {% for fn in free_functions %}
    static void {{ fn.bridge_name }}(void** args, void* retVal)
    {
        {% for p in fn.parameters %}
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

    static FunctionRuntimeMeta s_func_meta_{{ fn.bridge_name }} {
        .name           = "{{ fn.name }}",
        .qualified_name = "{{ fn.qualified_name }}",
        .mangling       = "{{ fn.mangling }}",
        .hash		    = {{ fn.hash }},
        .invoker        = &{{ fn.bridge_name }},
        .param_types    = {
            {% for p in fn.parameters -%}
            "{{ p.type_name }}"{% if not loop.is_last %}, 
            {% endif %}{% endfor %}
        },
        .return_type = "{{ fn.return_type }}"
    };
    {% endfor %}

    void register_reflections_{{ file_hash }}(RuntimeRegistry& registry)
    {
        {% for class in classes -%}
        registry.registerMeta(&s_meta_{{ class.extended_name }});
        {% endfor %}

        {% for enum in enums -%}
        registry.registerMeta(&s_enum_meta_{{ enum.extended_name }});
        {% endfor %}

        {% for fn in free_functions -%}
        registry.registerMeta(&s_func_meta_{{ fn.bridge_name }});
        {% endfor %}
    }
} // end namespace lux::cxx::dref::runtime
)";