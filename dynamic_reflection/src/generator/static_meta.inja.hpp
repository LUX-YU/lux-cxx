#pragma once
static inline std::string_view static_meta_template =
R"(#pragma once
#include <array>
#include <cstddef>
#include <tuple>
#include <string_view>

namespace lux::cxx::dref{
    {% for class in classes -%}
    template<>
    class type_meta<{{ class.name }}>
    {
    public:
        using type = {{ class.name }};
        static constexpr size_t size  = sizeof({{ class.name }});
        static constexpr size_t align = {{ class.align }};
        static constexpr const char* name = "{{ class.name }}";
        static constexpr EMetaType meta_type = {{ class.record_type }};

        static constexpr std::array<FieldInfo, {{ length(class.fields) }}> fields{
            {% for field in class.fields -%}
            lux::cxx::dref::FieldInfo{
                .name   = "{{ field.name }}",
                .offset = {{ field.offset }},
                .visibility = {{ field.visibility }},
                .index  = {{ field.index }}
            }{% if not loop.is_last %},
            {% endif %}{% endfor %}
        };

        static constexpr std::array<const char*, {{ length(class.attributes) }}> attributes{
            {% for attribute in class.attributes -%}
            "{{ attribute }}"{% if not loop.is_last %},
            {% endif %}{% endfor %}
        };

        static constexpr std::array<const char*, {{ length(class.funcs) }}> func_names{
            {% for func in class.funcs -%}
            "{{ func.name }}"{% if not loop.is_last %},
            {% endif %}{% endfor %}
        };

        static constexpr std::array<const char*, {{ length(class.static_funcs) }}> static_func_names{
            {% for static_func in class.static_funcs -%}
            "{{ static_func.name }}"{%- if not loop.is_last -%},
            {% endif -%}{% endfor %}
        };

        using field_types = std::tuple<
            {% for field_type in class.field_types -%}
            {{ field_type.value }} {% if not loop.is_last %},
            {% endif %}{% endfor %}
        >;

        using func_types = std::tuple<
            {% for func_type in class.func_types -%}
            decltype(static_cast<{{ func_type.return_type }} ({{ func_type.class_name }}::*)({% for param in func_type.parameters %}{{ param.type }}{% if not loop.is_last %}, {% endif %}{% endfor %}){% if func_type.is_const %} const{% endif %}>(&{{ func_type.class_name }}::{{ func_type.function_name }})){% if not loop.is_last %},
            {% endif %}{% endfor %}
        >;

        using static_func_types = std::tuple<
            {% for static_func_type in class.static_func_types -%}
            decltype(static_cast<{{ static_func_type.return_type }} (*)({% for param in static_func_type.parameters %}{{ param.type }}{% if not loop.is_last %}, {% endif %}{% endfor %})>(&{{ static_func_type.class_name }}::{{ static_func_type.function_name }})){% if not loop.is_last %},
            {% endif -%}{% endfor %}
        >;

        template<typename Func, typename Obj>
        static constexpr void foreach_field(Func&& func, Obj& object)
        {
            {% for field in class.fields -%}
            func(object.{{ field.name }}){% if not loop.is_last %};
            {% endif %}{% endfor %}
        }
    };
    {%- endfor %}

    {% for enum in enums -%}
    template<>
    class type_meta<{{ enum.name }}>
    {
    public:
        static constexpr size_t size = {{ enum.size }};
        using value_type   = std::underlying_type_t<{{ enum.name }}>;
        using element_type = std::pair<const char*, std::underlying_type_t<{{ enum.name }}>>;
        static constexpr EMetaType meta_type = EMetaType::ENUMERATOR;

        static constexpr std::array<const char*, size> keys{
            {% for key in enum.keys -%}
            "{{ key }}"{%- if not loop.is_last -%},
            {% endif %}{% endfor %}
        };

        static constexpr std::array<value_type, size> values{
            {% for value in enum.values -%}
            {{ value }}{%- if not loop.is_last -%},
            {% endif %}{% endfor %}
        };

        static constexpr std::array<element_type, size> elements{
            {% for element in enum.elements -%}
            element_type{"{{ element.key }}", {{ element.value }}}{% if not loop.is_last %},
            {% endif %}{% endfor %}
        };

        static constexpr const char* toString(const {{ enum.name }} value)
        {
            switch (value)
            {
                {% for case in enum.cases -%}
                case {{ enum.name }}::{{ case }}:
                    return "{{ case }}"{% if not loop.is_last %};
                {% endif %}{% endfor %}
            }
            return "";
        }
    };
    {%- endfor %}
}
)";
