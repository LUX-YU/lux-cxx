#pragma once

#include <inja/inja.hpp>

#include <lux/cxx/dref/parser/MetaUnit.hpp>
#include "tools.hpp"

#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <lux/cxx/dref/parser/Type.hpp>
#include <lux/cxx/dref/parser/Declaration.hpp>

#include <unordered_set>
#include <lux/cxx/visibility.h>

class DynamicMetaGenerator : 
	public lux::cxx::dref::DeclVisitor, 
	public lux::cxx::dref::TypeVisitor
{
public:
	DynamicMetaGenerator();
	
	virtual ~DynamicMetaGenerator();

	void visit(lux::cxx::dref::EnumDecl*) override;
	void visit(lux::cxx::dref::RecordDecl*) override;
	void visit(lux::cxx::dref::CXXRecordDecl*) override;
	void visit(lux::cxx::dref::FieldDecl*) override;
	void visit(lux::cxx::dref::FunctionDecl*) override;
	void visit(lux::cxx::dref::CXXMethodDecl*) override;
	void visit(lux::cxx::dref::ParmVarDecl*) override;

	void visit(lux::cxx::dref::BuiltinType*) override;
	void visit(lux::cxx::dref::PointerType*) override;
	void visit(lux::cxx::dref::LValueReferenceType*) override;
	void visit(lux::cxx::dref::RValueReferenceType*) override;
	void visit(lux::cxx::dref::RecordType*) override;
	void visit(lux::cxx::dref::EnumType*) override;
	void visit(lux::cxx::dref::FunctionType*) override;
	void visit(lux::cxx::dref::UnsupportedType*) override;

	void visit(lux::cxx::dref::Type*);

	void toJsonFile(nlohmann::json& json);

	void setConfig(Config& config);

	bool checkAndMarkVisited(const std::string& uniqueKey)
	{
		if (type_map_.find(uniqueKey) != type_map_.end()) {
			return true;
		}
		type_map_.insert(uniqueKey);
		return false;
	}

private:
	nlohmann::json records_			 = nlohmann::json::array();
	nlohmann::json enums_			 = nlohmann::json::array();
	nlohmann::json function_		 = nlohmann::json::array();

	nlohmann::json fundamental_		 = nlohmann::json::array();
	nlohmann::json ptr_				 = nlohmann::json::array();
	nlohmann::json ptr_to_memb_data_ = nlohmann::json::array();
	nlohmann::json ptr_to_memb_func_ = nlohmann::json::array();
	nlohmann::json ref_				 = nlohmann::json::array();
	nlohmann::json array_			 = nlohmann::json::array();
	
	
	// related to record
	nlohmann::json method_			 = nlohmann::json::array();
	nlohmann::json field_			 = nlohmann::json::array();
	// nlohmann::json static_method_	 = nlohmann::json::array(); record in function_

	std::unordered_set<std::string> type_map_;
};

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

private:
	nlohmann::json classes_ = nlohmann::json::array();
	nlohmann::json enums_   = nlohmann::json::array();
};