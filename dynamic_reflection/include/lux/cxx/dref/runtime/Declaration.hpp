#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include "Attribute.hpp"

namespace lux::cxx::dref {
class Type;
class CXXConstructorDecl;
class CXXDestructorDecl;
/* Subset of complete c++ declaration system
 * Decl
 *	└── NamedDecl
 *	     ├── TypeDecl
 *	     │    └── TagDecl
 *	     │         ├── EnumDecl
 *	     │         └── RecordDecl
 *	     │              └── CXXRecordDecl
 *	     └── ValueDecl
 *	          └── DeclaratorDecl
 *	               ├── FieldDecl
 *	               ├── FunctionDecl
 *	               │    └── CXXMethodDecl
 *	               │         ├── CXXConstructorDecl
 *	               │         ├── CXXConversionDecl
 *	               │         └── CXXDestructorDecl
 *	               └── VarDecl
 *	                    ├── ImplicitParamDecl
 *	                    ├── NonTypeTemplateParmDecl
 *	                    └── ParmVarDecl
 */

enum class EDeclKind
{
    NAMED_DECL,
    TYPED_DECL,
    TAG_DECL,
    ENUM_DECL,
    RECORD_DECL,
    CXX_RECORD_DECL,
    VALUE_DECL,
    DECLARATOR_DECL,
    FIELD_DECL,
    FUNCTION_DECL,
    CXX_METHOD_DECL,
    CXX_CONSTRUCTOR_DECL,
    CXX_CONVERSION_DECL,
    CXX_DESTRUCTOR_DECL,
    VAR_DECL,
    IMPLICIT_PARAM_DECL,
    NONTYPE_TEMPLATE_PARAM_DECL,
    PARAM_VAR_DECL,
    UNKNOWN_PARAM_DECL
};

//-------------------------------------
// (1) Decl : 所有声明节点的基类
//-------------------------------------
// 访问器基类
class EnumDecl;
class RecordDecl;
class CXXRecordDecl;
class CXXMethodDecl;
class FieldDecl;
class ParmVarDecl;
class FunctionDecl;
class DeclVisitor
{
public:
    virtual ~DeclVisitor() = default;
    // 只对具体的类型进行访问
    virtual void visit(EnumDecl*) = 0;
    virtual void visit(RecordDecl*) = 0;
    virtual void visit(CXXRecordDecl*) = 0;
    virtual void visit(FieldDecl*) = 0;
    virtual void visit(FunctionDecl*) = 0;
    virtual void visit(CXXMethodDecl*) = 0;
    virtual void visit(ParmVarDecl*) = 0;
};

class Decl
{
public:
    std::string id;
    EDeclKind   kind;
    virtual ~Decl() = default;

    virtual void accept(DeclVisitor*) = 0;
};

//-------------------------------------
// (2) NamedDecl : 具有名字的声明
//-------------------------------------
class NamedDecl : public Decl
{
public:
    std::string name;                // 用户可见名称
    std::string full_qualified_name; // 可能需要时可存
    std::string spelling;            // 源码中的拼写
    std::vector<std::string> attributes;
    Type* type = nullptr;
};

//======================================================================
// Part A: TypeDecl => TagDecl => EnumDecl, RecordDecl => CXXRecordDecl
//======================================================================

/*
 * TypeDecl：用于“声明了一个类型名字”的实体，如 class/struct/union/enum/typedef/...
 * 在 Clang 中，TypeDecl 也是 NamedDecl 的子类。
 */
class TypeDecl : public NamedDecl{};

/*
 * TagDecl：C/C++ 中带“Tag”的类型声明，比如 struct/union/class/enum。
 * 这里我们只演示最常见的： Record(类/结构/联合) 与 Enum(枚举)。
 */
class TagDecl : public TypeDecl
{
public:
	enum class ETagKind
	{
		Struct,
		Class,
		Union,
		Enum
	};

	ETagKind tag_kind;
};

//-------------------------------------
// (2A) EnumDecl
//-------------------------------------
struct Enumerator
{
      std::string name;
      int64_t	  signed_value;
      uint64_t    unsigned_value;
};

class BuiltinType;
class EnumDecl final : public TagDecl
{
public:
    // 是否是 enum class (scoped enumeration)
    bool is_scoped = false;
    // 枚举的底层类型
    BuiltinType* underlying_type = nullptr;
    // 枚举项
    std::vector<Enumerator> enumerators;

    ~EnumDecl() override = default;

    void accept(DeclVisitor* visitor) override 
    {
		visitor->visit(this);
    }
};

//-------------------------------------
// (2B) RecordDecl : struct/class/union
//-------------------------------------
class RecordDecl : public TagDecl
{
public:
    // 基本的字段 or 方法可以放在这里
    // Clang 里 RecordDecl 不一定知道是否是 CXXRecordDecl(仅当是 C++ class/struct)
    void accept(DeclVisitor* visitor) override
    {
        visitor->visit(this);
    }
};

//-------------------------------------
// (2B.1) CXXRecordDecl : C++ 中的 class/struct
//-------------------------------------
class CXXRecordDecl final : public RecordDecl
{
public:
    // 基类
    // 原先在你的 ClassDeclaration 里：std::vector<ClassDeclaration*> base_class_decls;
    // 这里用 CXXRecordDecl* 代替
    std::vector<CXXRecordDecl*>      bases;

    // 构造函数、析构函数、普通方法、静态方法、字段声明
    // 注意：Clang 里通常会将他们单独存放在 Redeclarable chains 里
    // 也可把它们存在另外的数据结构中
    std::vector<CXXConstructorDecl*> constructor_decls;
    CXXDestructorDecl*               destructor_decl = nullptr;
    std::vector<CXXMethodDecl*>      method_decls;
    std::vector<CXXMethodDecl*>      static_method_decls;
    std::vector<FieldDecl*>          field_decls;

    void accept(DeclVisitor* visitor) override
    {
        visitor->visit(this);
    }
};

//======================================================================
// Part B: ValueDecl => DeclaratorDecl => FieldDecl, FunctionDecl, VarDecl ...
//======================================================================

/*
 * ValueDecl：表示“声明了一个可用作值的实体”，比如变量、函数
 * (Clang 中：ValueDecl 继承自 NamedDecl。)
 */
class ValueDecl : public NamedDecl{};

/*
 * DeclaratorDecl：继承自 ValueDecl，表示通过 C/C++ 的“声明符”语法
 * 声明的实体（如变量, 函数, 字段, 形参, 等）。
 */
class DeclaratorDecl : public ValueDecl{};

//-------------------------------------
// (3A) FieldDecl : 类/结构的字段
//-------------------------------------

class FieldDecl final : public DeclaratorDecl
{
public:
    EVisibility visibility = EVisibility::INVALID;
    std::size_t offset = 0; // 字段偏移(单位可自定义：字节或bit)

    void accept(DeclVisitor* visitor) override
    {
        visitor->visit(this);
    }
};

//-------------------------------------
// (3B) FunctionDecl
//-------------------------------------
class FunctionDecl : public DeclaratorDecl
{
public:
    // 函数签名相关
    Type* result_type = nullptr; // 返回类型
    // 形参
    std::vector<class ParmVarDecl*> params;

    // 替代你原先存的 "mangling"
    std::string mangling;

    void accept(DeclVisitor* visitor) override
    {
        visitor->visit(this);
    }
};

//-------------------------------------
// (3B.1) CXXMethodDecl
//-------------------------------------
class CXXMethodDecl : public FunctionDecl
{
public:
    EVisibility visibility = EVisibility::INVALID;

    bool is_static  = false;
    bool is_virtual = false;
    bool is_const   = false;

    void accept(DeclVisitor* visitor) override
    {
        visitor->visit(this);
    }
};

// (3B.1.1) CXXConstructorDecl
class CXXConstructorDecl final : public CXXMethodDecl{};
// (3B.1.2) CXXConversionDecl
class CXXConversionDecl final  : public CXXMethodDecl{};
// (3B.1.3) CXXDestructorDecl
class CXXDestructorDecl final  : public CXXMethodDecl{};

//-------------------------------------
// (3C) VarDecl
//-------------------------------------
class VarDecl : public DeclaratorDecl{};

//-------------------------------------
// (3C.1) ParmVarDecl
//-------------------------------------
class ParmVarDecl final : public VarDecl
{
public:
    std::size_t index = 0;  // 在函数形参中的顺序
};



} // namespace lux::cxx::dref
