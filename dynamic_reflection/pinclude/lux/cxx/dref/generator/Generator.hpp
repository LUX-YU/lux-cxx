#pragma once

#include <inja/inja.hpp>

#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include "tools.hpp"

class MetaGenerator
{
public:
	MetaGenerator(const GeneratorConfig& config);
	void generate();
};

class DynamicMetaGenerator : public lux::cxx::dref::DeclVisitor
{
public:
	DynamicMetaGenerator();
	
	virtual ~DynamicMetaGenerator() {};

	void visit(lux::cxx::dref::EnumDecl*) override;
	void visit(lux::cxx::dref::RecordDecl*) override;
	void visit(lux::cxx::dref::CXXRecordDecl*) override;
	void visit(lux::cxx::dref::FieldDecl*) override;
	void visit(lux::cxx::dref::FunctionDecl*) override;
	void visit(lux::cxx::dref::CXXMethodDecl*) override;
	void visit(lux::cxx::dref::ParmVarDecl*) override;

	void setConfig(Config& config);

private:
	nlohmann::json records_			= nlohmann::json::array();
	nlohmann::json enums_			= nlohmann::json::array();
	nlohmann::json free_functions_	= nlohmann::json::array();
};

class StaticMetaGenerator : public lux::cxx::dref::DeclVisitor
{
public:
	StaticMetaGenerator();

	virtual ~StaticMetaGenerator() {};
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