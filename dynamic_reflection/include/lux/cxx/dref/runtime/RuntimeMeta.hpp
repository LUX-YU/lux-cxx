#pragma once
/**
 * @file runtime_meta.hpp
 * @brief Runtime metadata structures and registry for dynamic reflection.
 *
 * This header defines the runtime metadata for functions, fields, methods, records (classes/structs),
 * and enums. It also implements a global registry for storing and retrieving these metadata objects
 * via a unified MetaInfo mechanism.
 *
 * You can register `RecordRuntimeMeta`, `EnumRuntimeMeta`, or `FunctionRuntimeMeta` objects
 * into the registry. Each registered metadata is associated with a unique hash, which can come from
 * either a name (for classes/enums) or a mangled symbol (for functions). The registry supports
 * looking up metadata by either string or hash, as well as enumerating all registered metadata.
 *
 * @copyright
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <cstddef>

namespace lux::cxx::dref::runtime
{
    enum class ETypeKinds {
		Imcomplete,     // 不完整类型
        // 基本类型 (Fundamental types)
        Void,           // void
        Nullptr_t,      // std::nullptr_t (C++11)
        Bool,           // bool
        // 整数和字符类型
        Char,           // char
        SignedChar,     // signed char
        UnsignedChar,   // unsigned char
        Char8,          // char8_t (C++20)
        Char16,         // char16_t (C++11)
        Char32,         // char32_t (C++11)
        WChar,          // wchar_t
        Short,          // short
        Int,            // int
        Long,           // long
        LongLong,       // long long
        UnsignedShort,  // unsigned short
        UnsignedInt,    // unsigned int
        UnsignedLong,   // unsigned long
        UnsignedLongLong, // unsigned long long
        // 浮点类型
        Float,          // float
        Double,         // double
        LongDouble,     // long double

        // 复合类型 (Compound types)
        // 引用类型
        LvalueReference,   // lvalue reference (T&)
        RvalueReference,   // rvalue reference (T&&) (C++11)
        // 指针类型
        PointerToObject,   // 指向对象的指针
        PointerToFunction, // 指向函数的指针
        // 指向成员的指针类型
        PointerToDataMember,     // 指向数据成员的指针
        PointerToMemberFunction, // 指向成员函数的指针
        Array,           // 数组类型
        Function,        // 函数类型
        // 枚举类型
        UnscopedEnum,    // 非作用域枚举
        ScopedEnum,      // 作用域枚举 (C++11)
        // 类类型
        Class,           // 非联合类
        Union            // 联合体
    };

    static inline const char* typeKind2Str(ETypeKinds kind)
    {
        switch (kind)
        {
        case ETypeKinds::Imcomplete:             return "Imcomplete";
        case ETypeKinds::Void:                   return "Void";
        case ETypeKinds::Nullptr_t:              return "Nullptr_t";
        case ETypeKinds::Bool:                   return "Bool";
        case ETypeKinds::Char:                   return "Char";
        case ETypeKinds::SignedChar:             return "SignedChar";
        case ETypeKinds::UnsignedChar:           return "UnsignedChar";
        case ETypeKinds::Char8:                  return "Char8";
        case ETypeKinds::Char16:                 return "Char16";
        case ETypeKinds::Char32:                 return "Char32";
        case ETypeKinds::WChar:                  return "WChar";
        case ETypeKinds::Short:                  return "Short";
        case ETypeKinds::Int:                    return "Int";
        case ETypeKinds::Long:                   return "Long";
        case ETypeKinds::LongLong:               return "LongLong";
        case ETypeKinds::UnsignedShort:          return "UnsignedShort";
        case ETypeKinds::UnsignedInt:            return "UnsignedInt";
        case ETypeKinds::UnsignedLong:           return "UnsignedLong";
        case ETypeKinds::UnsignedLongLong:       return "UnsignedLongLong";
        case ETypeKinds::Float:                  return "Float";
        case ETypeKinds::Double:                 return "Double";
        case ETypeKinds::LongDouble:             return "LongDouble";
        case ETypeKinds::LvalueReference:        return "LvalueReference";
        case ETypeKinds::RvalueReference:        return "RvalueReference";
        case ETypeKinds::PointerToObject:        return "PointerToObject";
        case ETypeKinds::PointerToFunction:      return "PointerToFunction";
        case ETypeKinds::PointerToDataMember:    return "PointerToDataMember";
        case ETypeKinds::PointerToMemberFunction:return "PointerToMemberFunction";
        case ETypeKinds::Array:                  return "Array";
        case ETypeKinds::Function:               return "Function";
        case ETypeKinds::UnscopedEnum:           return "UnscopedEnum";
        case ETypeKinds::ScopedEnum:             return "ScopedEnum";
        case ETypeKinds::Class:                  return "Class";
        case ETypeKinds::Union:                  return "Union";
        default:                                 return "Unknown";
        }
    }

	enum class EVisibility {
        INVALID = 0,
        PUBLIC,
        PROTECTED,
        PRIVATE
	};

	struct CVQualifier
	{
		bool is_const;
		bool is_volatile;
	};

    struct BasicTypeMeta
    {
        std::string_view name;
		std::string_view qualified_name;
	    size_t           hash;
		ETypeKinds	     kind;
    };

	struct ObjectTypeMeta
	{
        using copy_func_t   = void(*)(void* dst, const void* src);
		using move_func_t   = void(*)(void* dst, void* src);

        size_t 		     size;
		size_t           alignment;

        copy_func_t      setter = nullptr;
		move_func_t      mover  = nullptr;
	};

    struct InvokableTypeMeta
    {
        using invokable_func_t = void(*)(void** args, void* retVal);
        
		std::string_view            mangling;
        size_t                      return_type_hash;
		std::vector<size_t>         param_type_hashs;
        bool						is_variadic;

        invokable_func_t invoker = nullptr;
    };

    // 1
    struct FundamentalRuntimeMeta
    {
        BasicTypeMeta       basic_info;
		ObjectTypeMeta	    object_info;
		CVQualifier	        cv_qualifier;
    };

	// 2 Pointer and Reference
    struct ReferenceRuntimeMeta
    {
        BasicTypeMeta       basic_info;
		size_t              pointee_hash;
    };

    struct PointerRuntimeMeta
    {
        BasicTypeMeta       basic_info;
		ObjectTypeMeta	    object_info;
		CVQualifier		    cv_qualifier;
        size_t              pointee_hash;
    };

	struct PointerToDataMemberRuntimeMeta
	{
		BasicTypeMeta       basic_info;
        ObjectTypeMeta	    object_info;
        CVQualifier		    cv_qualifier;
        size_t              pointee_hash;
        size_t              record_hash;
	};

    struct PointerToMethodRuntimeMeta
    {
        BasicTypeMeta       basic_info;
        ObjectTypeMeta	    object_info;
        CVQualifier		    cv_qualifier;
        size_t              pointee_hash;
        size_t              record_hash;
    };

    // 3
    struct ArrayRuntimeMeta
    {
        BasicTypeMeta       basic_info;
		ObjectTypeMeta	    object_info;
		CVQualifier	        cv_qualifier;
        size_t              element_hash;
        size_t              size;
    };

    // 4
    struct FunctionRuntimeMeta
    {
		BasicTypeMeta       basic_info;
        InvokableTypeMeta   invokable_info;
    };

    // 5
    struct MethodRuntimeMeta
    {
        using invokable_func_t = void(*)(void* obj, void** args, void* retVal);

        BasicTypeMeta       basic_info;
		InvokableTypeMeta   invokable_info; // invoker in invokable_info is disabled, which will be nullptrs
		CVQualifier		    cv_qualifier;

		EVisibility         visibility;
		bool				is_virtual;

		invokable_func_t    invoker = nullptr;
    };

    // 6
    struct FieldRuntimeMeta
    {
		BasicTypeMeta        basic_info;
		ObjectTypeMeta	     object_info;
        CVQualifier	         cv_qualifier;

        std::ptrdiff_t       offset;
        EVisibility          visibility;
    };

    // 7
    struct RecordRuntimeMeta
    {
		using get_field_func_t = void(*)(void* obj, size_t, void** field);
		using set_field_func_t = void(*)(void* obj, size_t, const void* inVal);

        BasicTypeMeta         basic_info;
        ObjectTypeMeta	      object_info;

		std::vector<size_t>   base_hashs;
        std::vector<size_t>   ctor_hashs;
        size_t                dtor_hash;
        std::vector<size_t>   field_meta_hashs;
        std::vector<size_t>   method_meta_hashs;
        std::vector<size_t>   static_method_metax_hashs;

		get_field_func_t      get_field_func;
    };

    // 8
    struct EnumRuntimeMeta
    {
        /**
         * @struct Enumerator
         * @brief Represents a single enumerator within an enum.
         */
        struct Enumerator
        {
            /// @brief Name of the enumerator.
            std::string_view    name;
            /// @brief Value of the enumerator (stored as a long long).
            long long           signed_value;
			/// @brief Value of the enumerator (stored as an unsigned long long).
			unsigned long long  unsigned_value;
        };

		BasicTypeMeta           basic_info;
        /// @brief Flag indicating if the enum is scoped (e.g., enum class).
        bool                    is_scoped;
        /// @brief Underlying type name of the enum (e.g., "int", "unsigned int").
        size_t                  underlying_type_hash;
        /// @brief List of enumerators in the enum.
        std::vector<Enumerator> enumerators;
    };
} // namespace lux::cxx::dref::runtime