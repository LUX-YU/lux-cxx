#pragma once
#include <lux/cxx/dref/meta/MetaType.hpp>
#include <cstdint>

namespace lux::cxx::dref
{
	class Enumeration : public TypeMeta
	{
	public:
		explicit Enumeration(EnumMeta* const meta)
			: _enum_meta(meta) {}

		bool is_scope() const
		{
			return _enum_meta->is_scope;
		}

		// if not find, return std::numeric_limits<std::uint64_t>::max();
		bool findElem(const std::string& enum_name, EnumMeta::ElemPair& out) const
		{
			auto& list = _enum_meta->elems_list;
			auto  iter = std::find_if(
				list.begin(), list.end(),
				[&enum_name](const EnumMeta::ElemPair& elem)
				{
					return elem.name == enum_name;
				}
			);
			iter == list.end() ? false : (out = *iter, true);
		}

		const std::vector<EnumMeta::ElemPair>& elemInfoList() const
		{
			return _enum_meta->elems_list;
		}

		EnumMeta* const enum_meta() const
		{
			return _enum_meta;
		}

	private:
		EnumMeta* const _enum_meta;
	};
}