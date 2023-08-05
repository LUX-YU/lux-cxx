#include "MetaUnitSerializer.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <mstch/mstch.hpp>

static const char* _template_constructor = R"(
static void new_func_{{type_id}}(void** args)
{
    void** ret = static_cast<void**>(args[0]);
    *ret = new {{type_name}}{
		{{#parameters}}
		(*reinterpret_cast<std::add_pointer_t<{{parameter}}>>(_args[{{index}}])),
		{{/parameters}}
	};
})";

static const char* _template_destructor = R"(
static void delete_func_{{type_id}}(void** args)
{
	void** object = static_cast<void**>(args[1]);
	delete static_cast<{{type_name}}*>(*object);
})";

namespace lux::cxx::dref
{	
	/*
	static inline std::string renderText(mstch::map& context, const char* mstch_template)
	{
		return mstch::render(mstch_template, context);
	}

	static std::string renderConstructor(DataTypeMeta* const meta)
	{
		mstch::array parameter_array;
		int i = 0;
		const auto& parameter_list = meta->constructor_meta->parameters_list;
		for (lux::cxx::dref::TypeMeta* parameter : parameter_list)
		{
			parameter_array.push_back(
				mstch::map{{"name", parameter->type_name}, { "index" , std::to_string(i)}}
			);
			i++;
		}
		mstch::map context{
			{"type_name",	meta->type_name},
			{"type_id",		std::to_string(meta->type_id)},
			{"parameters",	parameter_array}
		};
		return renderText(context, _template_constructor);
	}

	static std::string renderDestructor(DataTypeMeta* const meta)
	{
		mstch::map context{
			{"type_name", meta->type_name},
			{"type_id",   std::to_string(meta->type_id) }
		};

		return renderText(context, _template_destructor);
	}

	template<typename T>
	static std::string metaToString(T* const) requires std::is_base_of_v<TypeMeta, T>;

	template<>
	static std::string metaToString<DataTypeMeta>(DataTypeMeta* const meta)
	{
		return std::string{};
	}

	template<>
	static std::string metaToString<ClassMeta>(ClassMeta* const meta)
	{
		std::stringstream ss;
		ss << renderConstructor(meta) << std::endl;
		ss << renderDestructor(meta) << std::endl;
		return ss.str();;
	}

	template<>
	static std::string metaToString<EnumMeta>(EnumMeta* const meta)
	{
		return std::string{};
	}

	template<>
	static std::string metaToString<FunctionTypeMeta>(FunctionTypeMeta* const meta)
	{



		return std::string{};
	}

	bool MetaUnitSerializer::serialize(const std::filesystem::path& path, const MetaUnit& unit)
	{


		return true;
	}
	*/
}
