#include <lux/cxx/dref/runtime/Class.hpp>
#include <lux/cxx/dref/runtime/Method.hpp>

namespace lux::cxx::dref::runtime
{
	std::size_t	ClassInstance::align() const
	{
		return classMeta()->align;
	}

	Field ClassInstance::getField(std::string_view name)
	{
		auto  class_meta = classMeta();
		auto& list = class_meta->field_meta_list;
		auto  iter = std::find_if(list.begin(), list.end(),
			[name](FieldMeta* meta)
			{
				return meta->field_name == name;
			}
		);
		return iter == list.end() ? Field(nullptr) : Field(*iter);
	}

	Method ClassInstance::getMethod(std::string_view name)
	{
		auto  class_meta = classMeta();
		auto& list = class_meta->method_meta_list;
		auto  iter = std::find_if(list.begin(), list.end(),
			[name](MethodMeta* meta)
			{
				return meta->func_name == name;
			}
		);
		return iter == list.end() ? Method(nullptr) : Method(*iter);
	}

	void ClassInstance::invokeConstMethod(std::string_view name, FuncArg result, void* args) const
	{

	}

	void ClassInstance::invokeMethod(std::string_view name, FuncArg result, void* args)
	{

	}

	void ClassInstance::invokeStaticMethod(std::string_view name, FuncArg result, void* args)
	{

	}

	ClassMeta* const ClassInstance::classMeta() const
	{
		return static_cast<ClassMeta* const>(typeMeta());
	}

	ClassInstance::ClassInstance(ClassMeta* _c, std::string_view name, void** args)
	: Instance(_c){

	}

	ClassInstance Class::createInstance(std::string_view name, void** args)
	{
		return ClassInstance(classMeta(), name, args);
	}

	void Class::invokeStaticMethod(std::string_view, FuncArg result, void* args)
	{

	}

	ClassMeta* const	Class::classMeta() const
	{
		return _meta;
	}
}