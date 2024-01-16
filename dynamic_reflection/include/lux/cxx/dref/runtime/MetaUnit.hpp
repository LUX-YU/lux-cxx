#pragma once
#include <lux/cxx/visibility.h>
#include <lux/cxx/lan_model/declaration.hpp>
#include <lux/cxx/lan_model/type.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace lux::cxx::dref
{
	class MetaUnitImpl;

	using TypeMeta			= ::lux::cxx::lan_model::Type;
	using ETypeMetaKind		= ::lux::cxx::lan_model::ETypeKind;

	using Declaration		= ::lux::cxx::lan_model::Declaration;
	using EDeclarationKind	= ::lux::cxx::lan_model::EDeclarationKind;

	class MetaUnit
	{
		friend class CxxParserImpl;
	public:
		LUX_CXX_PUBLIC MetaUnit();

		LUX_CXX_PUBLIC MetaUnit(MetaUnit&&) noexcept;
		LUX_CXX_PUBLIC MetaUnit& operator=(MetaUnit&&) noexcept;

		LUX_CXX_PUBLIC ~MetaUnit();

		LUX_CXX_PUBLIC static size_t calculateDeclarationId(EDeclarationKind kind, const char* name);
		LUX_CXX_PUBLIC static size_t calculateDeclarationId(Declaration* const decl);

		LUX_CXX_PUBLIC static size_t calculateTypeMetaId(const char* name);
		LUX_CXX_PUBLIC static size_t calculateTypeMetaId(TypeMeta* const meta);

		LUX_CXX_PUBLIC bool isValid() const;

		LUX_CXX_PUBLIC size_t id() const;

		LUX_CXX_PUBLIC const std::string& name() const;

		LUX_CXX_PUBLIC const std::string& version() const;

		LUX_CXX_PUBLIC const std::vector<Declaration*>& markedDeclarationList() const;

		LUX_CXX_PUBLIC Declaration* const findDeclarationByName(EDeclarationKind kind, const std::string& name);

		LUX_CXX_PUBLIC const std::vector<TypeMeta*>& typeMetaList() const;

		LUX_CXX_PUBLIC TypeMeta* const findTypeMetaByName(const std::string& name) const;

	private:
		std::unique_ptr<MetaUnitImpl> _impl;

		LUX_CXX_PUBLIC explicit MetaUnit(std::unique_ptr<MetaUnitImpl>);
	};
}
