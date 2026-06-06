#pragma once
// Test/example scaffolding ONLY.
//
// `test_meta<T>` is the compile-time meta emitted by the example template
// `templates/test_meta.template.inja` and exercised by `test/generator_test.cpp`.
// It demonstrates the codegen pipeline; it is NOT the production reflection
// contract. Real, purpose-built compile-time metas live in their own modules
// (e.g. `lux::cxx::ser::meta_info` in the serialization module).
#include <cstddef>
#include "Declaration.hpp"

namespace lux::cxx::reflection
{
	// Example runtime field descriptor used by the demo template.
	class FieldInfo
	{
	public:
		std::string_view	name;
		size_t				offset;
		EVisibility			visibility;
		size_t				index;
		bool				is_const;
	};

	// Primary template for the demo compile-time meta (specialised by the generator).
	template <typename T> class test_meta;
}
