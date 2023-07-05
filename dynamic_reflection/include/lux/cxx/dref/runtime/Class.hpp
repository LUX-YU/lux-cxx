#pragma once
#include "MetaType.hpp"
#include "Instance.hpp"
#include "Method.hpp"
#include <vector>
#include <cstdlib>

namespace lux::cxx::dref::runtime
{
	class MethodMeta;

	struct FieldInfo
	{
		static constexpr MetaType self_meta_type = MetaType::FIELD ;

		std::string			field_name;
		size_t				offset;
		DataTypeMeta*		type_meta;
		ClassMeta*			owner_meta;
	};

	struct ClassMeta : public DataTypeMeta
	{
		static constexpr MetaType self_meta_type = MetaType::CLASS ;

		std::size_t						align;

		// remember : the type meta doesn't contain the name
		std::vector<FieldInfo>			field_meta_list;
		std::vector<ClassMeta*>			parents_list;
		std::vector<MethodMeta*>		method_meta_list;
	};

	class Field
	{
	public:
		Field(FieldInfo* const meta)
			: _meta(meta){}

	private:
		FieldInfo* const _meta;
	};

	class ClassInstance : public Instance
	{
		friend class Class;
	public:
		LUX_CXX_PUBLIC std::size_t			align() const;

		LUX_CXX_PUBLIC Field				getField(std::string_view name);

		LUX_CXX_PUBLIC Method				getMethod(std::string_view name);

		LUX_CXX_PUBLIC ClassMeta*  const	classMeta() const;

	private:
		LUX_CXX_PUBLIC ClassInstance(ClassMeta*, std::string_view, void** args);
	};

	class Class
	{
	public:
		Class(ClassMeta* const meta)
			: _meta(meta){}

		LUX_CXX_PUBLIC ClassInstance		createInstance(std::string_view, void** args);

		LUX_CXX_PUBLIC ClassMeta* const		classMeta() const;

	private:
		ClassMeta* const	_meta;
	};
}
