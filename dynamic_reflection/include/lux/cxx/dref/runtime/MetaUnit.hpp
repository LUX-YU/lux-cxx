#pragma once
#include <deque>
#include <lux/cxx/visibility.h>
#include <lux/cxx/lan_model/declaration.hpp>
#include <lux/cxx/lan_model/type.hpp>
#include <string_view>
#include <memory>

namespace lux::cxx::dref
{
	class MetaUnitImpl;

	using TypeMeta			= ::lux::cxx::lan_model::Type;
	using ETypeMetaKind		= ::lux::cxx::lan_model::ETypeKind;

	using Declaration		= ::lux::cxx::lan_model::Declaration;
	using EDeclarationKind	= ::lux::cxx::lan_model::EDeclarationKind;

	class LUX_CXX_PUBLIC MetaUnit
	{
		friend class CxxParserImpl;
	public:
		MetaUnit();
		MetaUnit(MetaUnit&&) noexcept;
		MetaUnit& operator=(MetaUnit&&) noexcept;
		~MetaUnit();

		static size_t calculateDeclarationId(EDeclarationKind kind, std::string_view name);
		static size_t calculateDeclarationId(const Declaration* decl);

		static size_t calculateTypeMetaId(std::string_view name);
		static size_t calculateTypeMetaId(TypeMeta* meta);

		[[nodiscard]] bool isValid() const;

		[[nodiscard]] size_t id() const;

		[[nodiscard]] const std::string& name() const;

		[[nodiscard]] const std::string& version() const;

		[[nodiscard]] const std::deque<lan_model::ClassDeclaration>&
		markedClassDeclarationList() const;

		[[nodiscard]] const std::deque<lan_model::FunctionDeclaration>&
		markedFunctionDeclarationList() const;

		[[nodiscard]] const std::deque<lan_model::EnumerationDeclaration>&
		markedEnumerationDeclarationList() const;

		[[nodiscard]] const std::deque<lan_model::ClassDeclaration>&
		unmarkedClassDeclarationList() const;

		[[nodiscard]] const std::deque<lan_model::FunctionDeclaration>&
		unmarkedFunctionDeclarationList() const;

		[[nodiscard]] const std::deque<lan_model::EnumerationDeclaration>&
		unmarkedEnumerationDeclarationList() const;

		[[nodiscard]] const Declaration* findDeclarationByName(EDeclarationKind kind, std::string_view name) const;

		[[nodiscard]] const std::deque<TypeMeta>& typeMetaList() const;

		[[nodiscard]] const TypeMeta* findTypeMetaByName(std::string_view name) const;

		static std::vector<std::byte> serializeBinary(const MetaUnit& mu);
		static MetaUnit deserializeBinary(const std::byte* data, size_t size);

	private:
		std::unique_ptr<MetaUnitImpl> _impl;

		explicit MetaUnit(std::unique_ptr<MetaUnitImpl>);
	};
}
