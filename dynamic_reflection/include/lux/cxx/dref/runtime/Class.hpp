#pragma once
#include "MetaType.hpp"
#include "Instance.hpp"
#include "Method.hpp"
#include <vector>
#include <cstdlib>

namespace lux::cxx::dref::runtime
{
	class MethodMeta;

	struct FieldMeta : public DataTypeMeta
	{
		static constexpr MetaType self_meta_type = MetaType::FIELD ;

		std::string			field_name;
		size_t				offset;
		ClassMeta*			owner_info;
	};

	struct ClassMeta : public DataTypeMeta
	{
		static constexpr MetaType self_meta_type = MetaType::CLASS ;

		std::size_t						align;

		// remember : the type meta doesn't contain the name
		std::vector<FieldMeta*>			field_meta_list;
		std::vector<ClassMeta*>			parents_list;
		std::vector<MethodMeta*>		method_meta_list;
	};

	class Field
	{
	public:
		Field(FieldMeta* const meta)
			: _meta(meta){}

	private:
		FieldMeta* const _meta;
	};

	class ClassInstance : public Instance
	{
		friend class Class;
	public:
		LUX_CXX_PUBLIC std::size_t			align() const;

		LUX_CXX_PUBLIC Field				getField(std::string_view name);

		LUX_CXX_PUBLIC Method				getMethod(std::string_view name);

		LUX_CXX_PUBLIC void					invokeConstMethod(std::string_view name, FuncArg result, void* args) const;

		LUX_CXX_PUBLIC void					invokeMethod(std::string_view name, FuncArg result, void* args);

		LUX_CXX_PUBLIC void					invokeStaticMethod(std::string_view name, FuncArg result, void* args);

		LUX_CXX_PUBLIC Class  const			classType() const;

	private:
		LUX_CXX_PUBLIC ClassInstance(Class*, std::string_view, void** args);
	};

	class Class
	{
	public:
		Class(ClassMeta* const meta)
			: _meta(meta){}

		LUX_CXX_PUBLIC ClassInstance		createInstance(std::string_view, void** args);

		LUX_CXX_PUBLIC void					invokeStaticMethod(std::string_view, FuncArg result, void* args);

		LUX_CXX_PUBLIC ClassMeta* const		classMeta() const;

	private:
		ClassMeta* const	_meta;
	};
}
