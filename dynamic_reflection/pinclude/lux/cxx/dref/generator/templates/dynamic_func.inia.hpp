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
static inline std::string_view dynamic_func_template =
R"(#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <array>
#include "{{ include_path }}"

namespace lux::cxx::dref::runtime {
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
};
)";