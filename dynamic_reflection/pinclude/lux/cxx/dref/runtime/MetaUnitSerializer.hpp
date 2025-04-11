#pragma once
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <nlohmann/json.hpp>
#include <lux/cxx/visibility.h>

#include "MetaUnitImpl.hpp"

namespace lux::cxx::dref
{
    static inline std::string declKindToString(EDeclKind k)
    {
        switch (k)
        {
        case EDeclKind::ENUM_DECL:               return "EnumDecl";
        case EDeclKind::RECORD_DECL:             return "RecordDecl";
        case EDeclKind::CXX_RECORD_DECL:         return "CXXRecordDecl";
        case EDeclKind::FIELD_DECL:              return "FieldDecl";
        case EDeclKind::FUNCTION_DECL:           return "FunctionDecl";
        case EDeclKind::CXX_METHOD_DECL:         return "CXXMethodDecl";
        case EDeclKind::CXX_CONSTRUCTOR_DECL:    return "CXXConstructorDecl";
        case EDeclKind::CXX_CONVERSION_DECL:     return "CXXConversionDecl";
        case EDeclKind::CXX_DESTRUCTOR_DECL:     return "CXXDestructorDecl";
        case EDeclKind::PARAM_VAR_DECL:          return "ParmVarDecl";
        case EDeclKind::VAR_DECL:                return "VarDecl";
        default:
            return "UnknownDecl";
        }
    }

    static inline EDeclKind stringToDeclKind(const std::string& s)
    {
        // 简单示意
        if (s == "EnumDecl")                 return EDeclKind::ENUM_DECL;
        else if (s == "RecordDecl")          return EDeclKind::RECORD_DECL;
        else if (s == "CXXRecordDecl")       return EDeclKind::CXX_RECORD_DECL;
        else if (s == "FieldDecl")           return EDeclKind::FIELD_DECL;
        else if (s == "FunctionDecl")        return EDeclKind::FUNCTION_DECL;
        else if (s == "CXXMethodDecl")       return EDeclKind::CXX_METHOD_DECL;
        else if (s == "CXXConstructorDecl")  return EDeclKind::CXX_CONSTRUCTOR_DECL;
        else if (s == "CXXConversionDecl")   return EDeclKind::CXX_CONVERSION_DECL;
        else if (s == "CXXDestructorDecl")   return EDeclKind::CXX_DESTRUCTOR_DECL;
        else if (s == "ParmVarDecl")         return EDeclKind::PARAM_VAR_DECL;
        else if (s == "VarDecl")             return EDeclKind::VAR_DECL;
        return EDeclKind::UNKNOWN;
    }

    // 同理给 TypeKinds
    static inline std::string typeKindToString(ETypeKinds k)
    {
        switch (k)
        {
        case ETypeKinds::Builtin:                   return "BuiltinType";
        case ETypeKinds::Pointer:                   return "PointerType";
        case ETypeKinds::PointerToDataMember:       return "MemberDataPointerType";
        case ETypeKinds::PointerToMemberFunction:   return "MemberFuncPointerType";
        case ETypeKinds::PointerToFunction:         return "FuncPointerType";
        case ETypeKinds::PointerToObject:           return "ObjectPointerType";
        case ETypeKinds::LvalueReference:           return "LValueReferenceType";
        case ETypeKinds::RvalueReference:           return "RValueReferenceType";
        case ETypeKinds::Record:                    return "RecordType";
        case ETypeKinds::Enum:                      return "EnumType";
		case ETypeKinds::ScopedEnum:                return "ScopedEnumType";
		case ETypeKinds::UnscopedEnum:              return "UnscopedEnumType";
        case ETypeKinds::Function:                  return "FunctionType";
        default:
            return "UnsupportedType";
        }
    }

    static inline ETypeKinds stringToTypeKind(const std::string& s)
    {
        if (s == "BuiltinType")              return ETypeKinds::Builtin;
        else if (s == "PointerType")         return ETypeKinds::Pointer;
		else if (s == "MemberDataPointerType") return ETypeKinds::PointerToDataMember;
		else if (s == "MemberFuncPointerType") return ETypeKinds::PointerToMemberFunction;
		else if (s == "FuncPointerType")     return ETypeKinds::PointerToFunction;
		else if (s == "ObjectPointerType")   return ETypeKinds::PointerToObject;
        else if (s == "LValueReferenceType") return ETypeKinds::LvalueReference;
        else if (s == "RValueReferenceType") return ETypeKinds::RvalueReference;
        else if (s == "RecordType")          return ETypeKinds::Record;
        else if (s == "EnumType")            return ETypeKinds::Enum;
		else if (s == "ScopedEnumType")      return ETypeKinds::ScopedEnum;
		else if (s == "UnscopedEnumType")    return ETypeKinds::UnscopedEnum;
        else if (s == "FunctionType")        return ETypeKinds::Function;
        return ETypeKinds::Unknown;
    }

    static inline NamedDecl* asNamedDecl(Decl* d)
    {
        if (!d) return nullptr;

        switch (d->kind)
        {
        case EDeclKind::ENUM_DECL: [[fallthrough]];
        case EDeclKind::RECORD_DECL: [[fallthrough]];
        case EDeclKind::CXX_RECORD_DECL: [[fallthrough]];
        case EDeclKind::FIELD_DECL: [[fallthrough]];
        case EDeclKind::FUNCTION_DECL: [[fallthrough]];
        case EDeclKind::CXX_METHOD_DECL: [[fallthrough]];
        case EDeclKind::CXX_CONSTRUCTOR_DECL: [[fallthrough]];
        case EDeclKind::CXX_CONVERSION_DECL: [[fallthrough]];
        case EDeclKind::CXX_DESTRUCTOR_DECL: [[fallthrough]];
        case EDeclKind::PARAM_VAR_DECL: [[fallthrough]];
        case EDeclKind::VAR_DECL:
            // 这些都继承自 NamedDecl
            return static_cast<NamedDecl*>(d);
        default:
            return nullptr;
        }
    }

    static inline const NamedDecl* asNamedDecl(const Decl* d)
    {
        return asNamedDecl(const_cast<Decl*>(d));
    }

	LUX_CXX_PUBLIC nlohmann::json serializeMetaUnitData(const MetaUnitData& data);
	LUX_CXX_PUBLIC void deserializeMetaUnitData(const nlohmann::json& root, MetaUnitData& data);
}
