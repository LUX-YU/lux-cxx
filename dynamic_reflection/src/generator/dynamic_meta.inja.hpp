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

#pragma once
static inline std::string_view dynamic_meta_template =
R"(#pragma once
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <array>

namespace lux::cxx::dref::runtime {
    {% for class in classes %}
    {% for method in class.methods %}
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
    {% endfor %}

    {% for sm in class.static_methods %}
    static void {{ class.extended_name }}_{{ sm.name }}_static_invoker(void** args, void* retVal)
    {
        {% for p in sm.parameters -%}
        auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
        {% endfor %}
        {% if sm.return_type == "void" %}
        {{ class.name }}::{{ sm.name }}({% for p in sm.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
        {% else %}
        auto result = {{ class.name }}::{{ sm.name }}({% for p in sm.parameters %}arg{{ p.index }}{% if not loop.is_last %}, {% endif %}{% endfor %});
        if(retVal) {
            *static_cast<{{ sm.return_type }}*>(retVal) = result;
        }{% endif %}
    }
    {% endfor %}

    {% if class.constructor_num > 0 %}{% for ctor in class.constructors %}
    static void* {{ ctor.bridge_name }}(void** args)
    {
        {% for p in ctor.parameters -%}
        auto arg{{ p.index }} = *static_cast<{{ p.type_name }}*>(args[{{ p.index }}]);
        {% endfor %}
        return new {{ class.name }}(
            {% for p in ctor.parameters %}
                arg{{ p.index }}{% if not loop.is_last %}, {% endif %}
            {% endfor %}
        );
    }{% endfor %}{% else %}
    static void* {{ class.extended_name }}_default_ctor(void** /*args*/)
    {
        return new {{ class.name }}();
    }
    {% endif %}

    namespace {
        {% for method in class.methods %}
        static MethodRuntimeMeta s_{{ class.extended_name }}_{{ method.name }}_method {
            .name = "{{ method.name }}",
            .invoker = &{{ class.extended_name }}_{{ method.name }}_invoker,
            .param_type_names = {
                {% for p in method.parameters -%}
                "{{ p.type_name }}"{% if not loop.is_last %}, {% endif %}{% endfor %}
            },
            .return_type = "{{ method.return_type }}",
            .is_virtual = {{ method.is_virtual }},
            .is_const   = {{ method.is_const }},
            .is_static  = false
        };
        {% endfor %}
        
        // -- (B') 静态方法 static 对象
        {% for sm in class.static_methods %}
        static FunctionRuntimeMeta s_{{ class.extended_name }}_{{ sm.name }}_static_method {
            .name = "{{ sm.name }}",
            .invoker = &{{ class.extended_name }}_{{ sm.name }}_static_invoker,
            .param_type_names = {
                {% for p in sm.parameters -%}
                "{{ p.type_name }}"{% if not loop.is_last %},{% endif %}{% endfor %}
            },
            .return_type = "{{ sm.return_type }}"
        };
        {% endfor %}
    } // end anonymous namespace

    static RecordRuntimeMeta s_meta_{{ class.extended_name }} {
        // 名字
        .name = "{{ class.name }}",
        .ctor = std::vector<RecordRuntimeMeta::ConstructorFn>{
            {% if class.constructor_num > 0 %}{% for ctor in class.constructors -%}
            &{{ ctor.bridge_name }},{% endfor %}{% else %}
            &{{ class.extended_name }}_default_ctor{% endif %}
        },
        .dtor = [](void* ptr){ delete static_cast<{{ class.name }}*>(ptr);},
        .fields = std::vector<FieldRuntimeMeta>{
            {% for field in class.fields -%}
            {
                .name       = "{{ field.name }}",
                .offset     = {{ field.offset }},
                .type_name  = "{{ field.type_name }}",
                .visibility = {{ field.visibility }},
                .is_static  = {{ field.is_static }},
            }{% if not loop.is_last %},
            {% endif %}{% endfor %}
        },
        .methods = std::vector<class MethodRuntimeMeta*>{
            {% for method in class.methods -%}
            &s_{{ class.extended_name }}_{{ method.name }}_method{% if not loop.is_last %},
            {% endif %}{% endfor %}
        },
        .static_methods = std::vector<class FunctionRuntimeMeta*>{
            {% for sm in class.static_methods -%}
            &s_{{ class.extended_name }}_{{ sm.name }}_static_method{% if not loop.is_last %},
            {% endif %}{% endfor %}
        }
    };

    static bool s_registered_{{ class.extended_name }} = [](){
        RuntimeRegistry::instance().registerRecord(&s_meta_{{ class.extended_name }});
        return true;
    }();
     // end for classes
    {% endfor %}

    {% for enum in enums %}
    static EnumRuntimeMeta s_enum_meta_{{ enum.extended_name }} {
        .name = "{{ enum.name }}",
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
    
    static bool s_enum_registered_{{ enum.extended_name }} = [](){
        RuntimeRegistry::instance().registerEnum(&s_enum_meta_{{ enum.extended_name }});
        return true;
    }();
    {% endfor %}

} // end namespace lux::cxx::dref::runtime

)";