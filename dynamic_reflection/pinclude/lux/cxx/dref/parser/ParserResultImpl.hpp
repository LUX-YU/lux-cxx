#pragma once
#include "libclang.hpp"
#include <lux/cxx/dref/runtime/Class.hpp>
#include <lux/cxx/dref/runtime/Enumeration.hpp>
#include <vector>

namespace lux::cxx::dref
{
	class ParserResultImpl
	{
		friend class ParserResult;
		friend class CxxParser;
	public:
		ParserResultImpl();

		~ParserResultImpl();

		void appendClassMeta(const runtime::ClassMeta& mark);
		void appendClassMeta(runtime::ClassMeta&& mark);

		void bufferMeta(const std::string& name, runtime::TypeMeta* meta);

		runtime::TypeMeta* getBufferedMeta(const std::string&);

		bool hasBufferedMeta(const std::string&);

	private:
		std::vector<runtime::ClassMeta>						_class_meta_list;
		std::vector<runtime::EnumMeta>						_enum_meta_list;
		std::unordered_map<std::string, runtime::TypeMeta*>	_meta_map;
	};
}
