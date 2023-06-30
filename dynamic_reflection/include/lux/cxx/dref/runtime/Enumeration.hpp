#pragma once
#include "MetaType.hpp"
#include <cstdint>

namespace lux::cxx::dref::runtime
{
	struct EnumMeta : public DataTypeMeta
	{
		struct ElemPair
		{
			std::string_view	name;
			size_t				value;
		};

		bool					_is_scope;
		std::vector<ElemPair*>	_elems_list;
	};

	class Enumeration : public TypeMeta
	{
	public:
		Enumeration(EnumMeta* const);

		bool is_scope() const;

		std::uint64_t	unsignedValue(const std::string & enum_name) const;

		std::int64_t	signedValue(const std::string& enum_name) const;

		EnumMeta* const enum_meta() const;

	private:
		EnumMeta* const _enum_meta;
	};
}