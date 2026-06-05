#pragma once
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <nlohmann/json.hpp>
#include <lux/cxx/visibility.h>

#include "MetaUnitImpl.hpp"

namespace lux::cxx::dref
{
    // ---------------------------------------------------------------
    // Single source of truth for declaration-kind ↔ JSON-string maps.
    // Adding a new EDeclKind requires only one new X-row in this list;
    // both serialisation and deserialisation pick it up automatically.
    // ---------------------------------------------------------------
    #define LUX_DREF_DECL_KIND_TABLE(X)                                   \
        X(ENUM_DECL,             "EnumDecl")                              \
        X(RECORD_DECL,           "RecordDecl")                            \
        X(CXX_RECORD_DECL,       "CXXRecordDecl")                         \
        X(FIELD_DECL,            "FieldDecl")                             \
        X(FUNCTION_DECL,         "FunctionDecl")                          \
        X(CXX_METHOD_DECL,       "CXXMethodDecl")                         \
        X(CXX_CONSTRUCTOR_DECL,  "CXXConstructorDecl")                    \
        X(CXX_CONVERSION_DECL,   "CXXConversionDecl")                     \
        X(CXX_DESTRUCTOR_DECL,   "CXXDestructorDecl")                     \
        X(PARAM_VAR_DECL,        "ParmVarDecl")                           \
        X(VAR_DECL,              "VarDecl")

    inline std::string declKindToString(EDeclKind k)
    {
        switch (k)
        {
        #define LUX_DREF_X(enumerator, str) case EDeclKind::enumerator: return str;
            LUX_DREF_DECL_KIND_TABLE(LUX_DREF_X)
        #undef LUX_DREF_X
        default:
            return "UnknownDecl";
        }
    }

    inline EDeclKind stringToDeclKind(const std::string& s)
    {
        #define LUX_DREF_X(enumerator, str) if (s == str) return EDeclKind::enumerator;
            LUX_DREF_DECL_KIND_TABLE(LUX_DREF_X)
        #undef LUX_DREF_X
        return EDeclKind::UNKNOWN;
    }

    // Kinds whose runtime type is a NamedDecl subclass.
    inline bool isNamedDeclKind(EDeclKind k)
    {
        switch (k)
        {
        #define LUX_DREF_X(enumerator, str) case EDeclKind::enumerator: return true;
            LUX_DREF_DECL_KIND_TABLE(LUX_DREF_X)
        #undef LUX_DREF_X
        default:
            return false;
        }
    }

    // ---------------------------------------------------------------
    // ETypeKinds ↔ JSON string table. The ordering matters only for
    // human readability; lookup is name-based.
    // ---------------------------------------------------------------
    #define LUX_DREF_TYPE_KIND_TABLE(X)                                       \
        X(Builtin,                  "BuiltinType")                            \
        X(Pointer,                  "PointerType")                            \
        X(PointerToDataMember,      "MemberDataPointerType")                  \
        X(PointerToMemberFunction,  "MemberFuncPointerType")                  \
        X(PointerToFunction,        "FuncPointerType")                        \
        X(PointerToObject,          "ObjectPointerType")                      \
        X(LvalueReference,          "LValueReferenceType")                    \
        X(RvalueReference,          "RValueReferenceType")                    \
        X(Record,                   "RecordType")                             \
        X(Enum,                     "EnumType")                               \
        X(ScopedEnum,               "ScopedEnumType")                         \
        X(UnscopedEnum,             "UnscopedEnumType")                       \
        X(Function,                 "FunctionType")

    inline std::string typeKindToString(ETypeKinds k)
    {
        switch (k)
        {
        #define LUX_DREF_X(enumerator, str) case ETypeKinds::enumerator: return str;
            LUX_DREF_TYPE_KIND_TABLE(LUX_DREF_X)
        #undef LUX_DREF_X
        default:
            return "UnsupportedType";
        }
    }

    inline ETypeKinds stringToTypeKind(const std::string& s)
    {
        #define LUX_DREF_X(enumerator, str) if (s == str) return ETypeKinds::enumerator;
            LUX_DREF_TYPE_KIND_TABLE(LUX_DREF_X)
        #undef LUX_DREF_X
        return ETypeKinds::Unknown;
    }

    // ---------------------------------------------------------------
    // TagDecl::ETagKind ↔ JSON string table.
    // ---------------------------------------------------------------
    #define LUX_DREF_TAG_KIND_TABLE(X)                                        \
        X(Struct, "Struct")                                                   \
        X(Class,  "Class")                                                    \
        X(Union,  "Union")                                                    \
        X(Enum,   "Enum")

    inline std::string tagKindToString(TagDecl::ETagKind k)
    {
        switch (k)
        {
        #define LUX_DREF_X(enumerator, str) case TagDecl::ETagKind::enumerator: return str;
            LUX_DREF_TAG_KIND_TABLE(LUX_DREF_X)
        #undef LUX_DREF_X
        }
        return "Struct";
    }

    inline TagDecl::ETagKind stringToTagKind(const std::string& s)
    {
        #define LUX_DREF_X(enumerator, str) if (s == str) return TagDecl::ETagKind::enumerator;
            LUX_DREF_TAG_KIND_TABLE(LUX_DREF_X)
        #undef LUX_DREF_X
        return TagDecl::ETagKind::Struct;
    }

    inline NamedDecl* asNamedDecl(Decl* d)
    {
        if (!d) return nullptr;
        return isNamedDeclKind(d->kind) ? static_cast<NamedDecl*>(d) : nullptr;
    }

    inline const NamedDecl* asNamedDecl(const Decl* d)
    {
        return asNamedDecl(const_cast<Decl*>(d));
    }

	LUX_CXX_PUBLIC nlohmann::json serializeMetaUnitData(const MetaUnitData& data);
	LUX_CXX_PUBLIC void deserializeMetaUnitData(const nlohmann::json& root, MetaUnitData& data);
}
