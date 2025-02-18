#pragma once
#include <deque>
#include <lux/cxx/visibility.h>
#include <string_view>
#include <memory>

#include "Declaration.hpp"

namespace lux::cxx::dref
{
	class MetaUnitImpl;
	class MetaUnitData;
	class LUX_CXX_PUBLIC MetaUnit final
	{
		friend class CxxParserImpl;
	public:
		MetaUnit();
		MetaUnit(MetaUnit&&) noexcept;
		MetaUnit& operator=(MetaUnit&&) noexcept;
		~MetaUnit();

		static size_t calculateTypeMetaId(std::string_view name);

		[[nodiscard]] bool isValid() const;

		[[nodiscard]] size_t id() const;

		[[nodiscard]] const std::string& name() const;

		[[nodiscard]] const std::string& version() const;

		[[nodiscard]] const std::vector<Decl*>& markedDeclarations() const;

	private:
		std::unique_ptr<MetaUnitImpl> _impl;

		explicit MetaUnit(std::unique_ptr<MetaUnitImpl>);
	};
}
