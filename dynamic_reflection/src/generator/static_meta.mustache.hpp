#pragma once
static inline std::string_view static_meta_template =
R"(#pragma once
#include <array>
#include <cstddef>
#include <tuple>
#include <string_view>

namespace lux::cxx::dref{
    {{#classes}}
    template<>
    class type_meta<{{name}}>
    {
    public:
        using type = {{name}};
        static constexpr size_t size  = sizeof({{name}});
        static constexpr size_t align = {{align}};
        static constexpr const char* name = "{{name}}";
        static constexpr EMetaType meta_type = {{record_type}};

        static constexpr std::array<FieldInfo, {{fields_count}}> fields{
            {{#fields}}
            lux::cxx::dref::FieldInfo{
                .name   = "{{name}}",
                .offset = {{offset}},
                .visibility = {{visibility}},
                .index  = {{index}}
            },
            {{/fields}}
        };

        static constexpr std::array<const char*, {{attributes_count}}> attributes{
            {{#attributes}}
            "{{.}}",
            {{/attributes}}
        };

        static constexpr std::array<const char*, {{funcs_count}}> func_names{
            {{#funcs}}
            "{{name}}",
            {{/funcs}}
        };

        static constexpr std::array<const char*, {{static_funcs_count}}> static_func_names{
            {{#static_funcs}}
            "{{name}}",
            {{/static_funcs}}
        };

        using field_types = std::tuple<
            {{#field_types}}
            {{value}}{{^last}},{{/last}}
            {{/field_types}}
        >;

        using func_types = std::tuple<
            {{#func_types}}
            decltype(static_cast<{{return_type}} ({{class_name}}::*)({{#parameters}}{{type}}{{^last}}, {{/last}}{{/parameters}}){{#is_const}} const{{/is_const}}>(&{{class_name}}::{{function_name}})){{^last}},{{/last}}
            {{/func_types}}
        >;

        using static_func_types = std::tuple<
            {{#static_func_types}}
            decltype(static_cast<{{return_type}} (*)({{#parameters}}{{type}}{{^last}}, {{/last}}{{/parameters}})>(&{{class_name}}::{{function_name}})){{^last}},{{/last}}
            {{/static_func_types}}
        >;

        template<typename Func, typename Obj>
        static constexpr void foreach_field(Func&& func, Obj& object)
        {
            {{#fields}}
            func(object.{{name}});
            {{/fields}}
        }
    };
    {{/classes}}

    {{#enums}}
    template<>
    class type_meta<{{name}}>
    {
    public:
        static constexpr size_t size = {{size}};
        using value_type   = std::underlying_type_t<{{name}}>;
        using element_type = std::pair<const char*, std::underlying_type_t<{{name}}>>;
        static constexpr EMetaType meta_type = EMetaType::ENUMERATOR;

        static constexpr std::array<const char*, size> keys{
            {{#keys}}
            "{{.}}",
            {{/keys}}
        };

        static constexpr std::array<value_type, size> values{
            {{#values}}
            {{.}},
            {{/values}}
        };

        static constexpr std::array<element_type, size> elements{
            {{#elements}}
            element_type{"{{key}}", {{value}}},
            {{/elements}}
        };

        static constexpr const char* toString(const {{name}} value)
        {
            switch (value)
            {
                {{#cases}}
                case {{name}}::{{.}}:
                    return "{{.}}";
                {{/cases}}
            }
            return "";
        }
    };
    {{/enums}}
}
)";
