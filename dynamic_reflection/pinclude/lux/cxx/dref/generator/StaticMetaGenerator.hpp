#pragma once

#include <inja/inja.hpp>

#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include "tools.hpp"

#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>

#include <unordered_set>
#include <lux/cxx/visibility.h>

namespace lux::cxx::dref{
	class StaticMetaGenerator : public lux::cxx::dref::DeclVisitor
	{
	public:
		StaticMetaGenerator();
	
		virtual ~StaticMetaGenerator();
		void visit(lux::cxx::dref::EnumDecl*) override;
		void visit(lux::cxx::dref::RecordDecl*) override;
		void visit(lux::cxx::dref::CXXRecordDecl*) override;
		void visit(lux::cxx::dref::FieldDecl*) override;
		void visit(lux::cxx::dref::FunctionDecl*) override;
		void visit(lux::cxx::dref::CXXMethodDecl*) override;
		void visit(lux::cxx::dref::ParmVarDecl*) override;
		void setConfig(Config& config);
	
		void toJsonFile(nlohmann::json& json);
	
	private:
		nlohmann::json classes_ = nlohmann::json::array();
		nlohmann::json enums_   = nlohmann::json::array();
	};
}