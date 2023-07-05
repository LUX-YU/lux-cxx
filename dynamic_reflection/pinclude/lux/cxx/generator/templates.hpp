#pragma once
#include <string_view>

namespace lux::cxx::dref
{
	lux_generate_body_start = "#include <lux>\nnamespace lux::cxx::dref::detail{";
	lux_generate_body_end   = "}";
	lux_class_generate_mustache_temp = 
R"=(
	static ClassMeta {{class_name}}_class_meta;
	{{#field_init}}
		static FieldMeta {{class_name}}_{{field_name}};
		{{class_name}}_{{field_name}}.
	{{/field_init}}
	
)=";

}
