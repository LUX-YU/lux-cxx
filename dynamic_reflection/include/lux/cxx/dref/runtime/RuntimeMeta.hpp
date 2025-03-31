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
#include <variant>
#include <unordered_map>
#include <cstddef>

namespace lux::cxx::dref::runtime
{
    /**
     * @enum ETypeKinds
     * @brief Enumerates the kinds of types supported for runtime metadata.
     *
     * This enumeration categorizes both fundamental and compound types, including pointers,
     * references, arrays, functions, enums, and classes.
     */
    enum class ETypeKinds {
		Unknown = 0,            /**< Unknown type. */
        Imcomplete,             /**< Incomplete type. (Note: "Imcomplete" is a known spelling issue.) */
        // Fundamental types
        Void,                   /**< void type. */
        Nullptr_t,              /**< std::nullptr_t type (C++11). */
        Bool,                   /**< Boolean type. */
        // Integer and character types
        Char,                   /**< char type. */
        SignedChar,             /**< signed char type. */
        UnsignedChar,           /**< unsigned char type. */
        Char8,                  /**< char8_t type (C++20). */
        Char16,                 /**< char16_t type (C++11). */
        Char32,                 /**< char32_t type (C++11). */
        WChar,                  /**< wchar_t type. */
        Short,                  /**< short integer type. */
        Int,                    /**< int type. */
        Long,                   /**< long integer type. */
        LongLong,               /**< long long integer type. */
        UnsignedShort,          /**< unsigned short integer type. */
        UnsignedInt,            /**< unsigned int type. */
        UnsignedLong,           /**< unsigned long integer type. */
        UnsignedLongLong,       /**< unsigned long long integer type. */
        // Floating-point types
        Float,                  /**< float type. */
        Double,                 /**< double type. */
        LongDouble,             /**< long double type. */
        // Compound types
        // Reference types
        LvalueReference,        /**< lvalue reference (T&). */
        RvalueReference,        /**< rvalue reference (T&&) (C++11). */
        // Pointer types
        PointerToObject,        /**< Pointer to an object. */
        PointerToFunction,      /**< Pointer to a function. */
        // Pointer-to-member types
        PointerToDataMember,    /**< Pointer to a data member. */
        PointerToMemberFunction,/**< Pointer to a member function. */
        Array,                  /**< Array type. */
        Function,               /**< Function type. */
        // Enumeration types
        UnscopedEnum,           /**< Unscoped enumeration. */
        ScopedEnum,             /**< Scoped enumeration (C++11). */
        // Class types
        Class,                  /**< Non-union class type. */
        Union                   /**< Union type. */
    };

    /**
     * @brief Converts an ETypeKinds enum value to a corresponding string.
     * @param kind The ETypeKinds value.
     * @return A constant C-style string representing the type kind.
     */
    static inline const char* typeKind2Str(ETypeKinds kind)
    {
        switch (kind)
        {
        case ETypeKinds::Imcomplete:              return "Imcomplete";
        case ETypeKinds::Void:                    return "Void";
        case ETypeKinds::Nullptr_t:               return "Nullptr_t";
        case ETypeKinds::Bool:                    return "Bool";
        case ETypeKinds::Char:                    return "Char";
        case ETypeKinds::SignedChar:              return "SignedChar";
        case ETypeKinds::UnsignedChar:            return "UnsignedChar";
        case ETypeKinds::Char8:                   return "Char8";
        case ETypeKinds::Char16:                  return "Char16";
        case ETypeKinds::Char32:                  return "Char32";
        case ETypeKinds::WChar:                   return "WChar";
        case ETypeKinds::Short:                   return "Short";
        case ETypeKinds::Int:                     return "Int";
        case ETypeKinds::Long:                    return "Long";
        case ETypeKinds::LongLong:                return "LongLong";
        case ETypeKinds::UnsignedShort:           return "UnsignedShort";
        case ETypeKinds::UnsignedInt:             return "UnsignedInt";
        case ETypeKinds::UnsignedLong:            return "UnsignedLong";
        case ETypeKinds::UnsignedLongLong:        return "UnsignedLongLong";
        case ETypeKinds::Float:                   return "Float";
        case ETypeKinds::Double:                  return "Double";
        case ETypeKinds::LongDouble:              return "LongDouble";
        case ETypeKinds::LvalueReference:         return "LvalueReference";
        case ETypeKinds::RvalueReference:         return "RvalueReference";
        case ETypeKinds::PointerToObject:         return "PointerToObject";
        case ETypeKinds::PointerToFunction:       return "PointerToFunction";
        case ETypeKinds::PointerToDataMember:     return "PointerToDataMember";
        case ETypeKinds::PointerToMemberFunction: return "PointerToMemberFunction";
        case ETypeKinds::Array:                   return "Array";
        case ETypeKinds::Function:                return "Function";
        case ETypeKinds::UnscopedEnum:            return "UnscopedEnum";
        case ETypeKinds::ScopedEnum:              return "ScopedEnum";
        case ETypeKinds::Class:                   return "Class";
        case ETypeKinds::Union:                   return "Union";
        default:                                  return "Unknown";
        }
    }

    /**
     * @enum EVisibility
     * @brief Enumerates the visibility (access specifiers) of a class member.
     */
    enum class EVisibility {
        INVALID = 0,  /**< Invalid visibility. */
        PUBLIC,       /**< Public access specifier. */
        PROTECTED,    /**< Protected access specifier. */
        PRIVATE       /**< Private access specifier. */
    };

    /**
     * @struct CVQualifier
     * @brief Represents the const and volatile qualifiers of a type.
     */
    struct CVQualifier
    {
        bool is_const;      /**< Indicates if the type is const-qualified. */
        bool is_volatile;   /**< Indicates if the type is volatile-qualified. */
    };

    /**
     * @struct BasicTypeMeta
     * @brief Contains basic metadata for a type.
     */
    struct BasicTypeMeta
    {
        std::string_view name;            /**< The simple name of the type. */
        std::string_view qualified_name;  /**< The fully qualified name of the type. */
        size_t           hash;            /**< A unique hash representing the type. */
        ETypeKinds       kind;            /**< The kind/category of the type. */
    };

    /**
     * @struct ObjectTypeMeta
     * @brief Metadata describing an object type's size, alignment, and copy/move operations.
     */
    struct ObjectTypeMeta
    {
        /// @typedef copy_func_t
        /// @brief Function pointer type for copying objects.
        using copy_func_t = void(*)(void* dst, const void* src);
        /// @typedef move_func_t
        /// @brief Function pointer type for moving objects.
        using move_func_t = void(*)(void* dst, void* src);

        size_t size;         /**< Size of the type in bytes. */
        size_t alignment;    /**< Alignment requirement of the type in bytes. */
        copy_func_t copy_fn  = nullptr; /**< Function to copy an object from one location to another. */
        move_func_t move_fn  = nullptr; /**< Function to move an object from one location to another. */
    };

    /**
     * @struct InvokableTypeMeta
     * @brief Metadata for types that can be invoked (functions, methods, constructors, and destructors).
     *
     * This structure holds information such as the mangled name, return type hash, parameter type hashes,
     * variadic flag, and a variant holding the actual invoker function.
     */
    struct InvokableTypeMeta
    {
        /// @typedef function_t
        /// @brief Function pointer type for free functions.
        using function_t = void(*)(void** args, void* retVal);
        /// @typedef method_t
        /// @brief Function pointer type for member methods.
        using method_t = void(*)(void* obj, void** args, void* retVal);
        /// @typedef ctor_t
		/// @brief Function pointer type for constructors. if placement is true, args[0] is the address.
        using ctor_t = void* (*)(bool placement, void** args);

        /// @typedef invokable_func_t
        /// @brief A variant type holding one of the invokable function pointers.
        using invokable_func_t = std::variant<
            function_t,
            method_t,
            ctor_t
        >;

        std::string_view            mangling;          /**< The mangled name representing the signature. */
        size_t                      return_type_hash;  /**< The unique hash of the return type. */
        std::vector<size_t>         param_type_hashs;  /**< A list of unique hashes for each parameter type. */
        bool                        is_variadic;       /**< True if the function/method is variadic. */
        invokable_func_t            invoker;           /**< The invoker function variant. */
    };

    /**
     * @struct FundamentalRuntimeMeta
     * @brief Runtime metadata for fundamental types.
     */
    struct FundamentalRuntimeMeta
    {
        BasicTypeMeta   basic_info;   /**< Basic type information. */
        ObjectTypeMeta  object_info; /**< Object-related metadata such as size and alignment. */
        CVQualifier     cv_qualifier;   /**< Const and volatile qualifiers for the type. */
    };

    /**
     * @struct ReferenceRuntimeMeta
     * @brief Metadata for lvalue and rvalue reference types.
     */
    struct ReferenceRuntimeMeta
    {
        BasicTypeMeta basic_info;  /**< Basic type information for the reference. */
        size_t pointee_hash;       /**< Unique hash of the referenced type. */
    };

    /**
     * @struct PointerRuntimeMeta
     * @brief Metadata for pointer types.
     */
    struct PointerRuntimeMeta
    {
        BasicTypeMeta basic_info;  /**< Basic type information for the pointer. */
        ObjectTypeMeta object_info;/**< Object-related metadata such as size and alignment. */
        CVQualifier cv_qualifier;  /**< Const and volatile qualifiers for the pointer. */
        size_t pointee_hash;       /**< Unique hash of the pointed-to type. */
    };

    /**
     * @struct PointerToDataMemberRuntimeMeta
     * @brief Metadata for pointers to data members.
     */
    struct PointerToDataMemberRuntimeMeta
    {
        BasicTypeMeta basic_info;  /**< Basic type information for the pointer to data member. */
        ObjectTypeMeta object_info;/**< Object-related metadata such as size and alignment. */
        CVQualifier cv_qualifier;  /**< Const and volatile qualifiers for the pointer. */
        size_t pointee_hash;       /**< Unique hash of the data member type. */
        size_t record_hash;        /**< Unique hash of the class/struct that contains the member. */
    };

    /**
     * @struct PointerToMethodRuntimeMeta
     * @brief Metadata for pointers to member functions.
     */
    struct PointerToMethodRuntimeMeta
    {
        BasicTypeMeta basic_info;  /**< Basic type information for the pointer to member function. */
        ObjectTypeMeta object_info;/**< Object-related metadata such as size and alignment. */
        CVQualifier cv_qualifier;  /**< Const and volatile qualifiers for the pointer. */
        size_t pointee_hash;       /**< Unique hash of the member function type. */
        size_t record_hash;        /**< Unique hash of the class/struct that contains the member function. */
    };

    /**
     * @struct ArrayRuntimeMeta
     * @brief Metadata for array types.
     */
    struct ArrayRuntimeMeta
    {
        BasicTypeMeta basic_info;  /**< Basic type information for the array. */
        ObjectTypeMeta object_info;/**< Object-related metadata such as size and alignment. */
        CVQualifier cv_qualifier;  /**< Const and volatile qualifiers for the array. */
        size_t element_hash;       /**< Unique hash of the array element type. */
        size_t size;               /**< Number of elements in the array. */
    };

    /**
     * @struct FunctionRuntimeMeta
     * @brief Metadata for free function types.
     */
    struct FunctionRuntimeMeta
    {
        BasicTypeMeta basic_info;       /**< Basic type information for the function. */
        InvokableTypeMeta invokable_info; /**< Invokable metadata including signature and invoker function. */
    };

    /**
     * @struct MethodRuntimeMeta
     * @brief Metadata for member function (method) types.
     */
    struct MethodRuntimeMeta
    {
        BasicTypeMeta basic_info;        /**< Basic type information for the method. */
        InvokableTypeMeta invokable_info;
        CVQualifier cv_qualifier;          /**< Const and volatile qualifiers for the method. */
        EVisibility visibility;            /**< Visibility (public, protected, or private) of the method. */
        bool is_virtual;                   /**< Indicates if the method is virtual. */
    };

    /**
     * @struct FieldRuntimeMeta
     * @brief Metadata for class/struct fields.
     */
    struct FieldRuntimeMeta
    {
        /// @typedef get_field_func_t
        /// @brief Function pointer type to get a field's value.
        using get_field_ptr_func_t = void* (*)(void* obj);
		using get_field_func_t     = void(*)(void* obj, void* outVal);
        /// @typedef set_field_func_t
        /// @brief Function pointer type to set a field's value.
        using set_field_func_t     = void(*)(void* obj, void* inVal);

        BasicTypeMeta           basic_info;  /**< Basic type information for the field. */
        ObjectTypeMeta          object_info;/**< Object-related metadata such as size and alignment for the field. */
        CVQualifier             cv_qualifier;  /**< Const and volatile qualifiers for the field. */
        std::ptrdiff_t          offset;     /**< Offset of the field within the class/struct. */
        EVisibility             visibility;    /**< Visibility (public, protected, or private) of the field. */
        get_field_ptr_func_t    get_ptr_fn = nullptr; /**< Function to retrieve the field value. */
		get_field_func_t	    get_fn = nullptr; /**< Function to retrieve the field value. */
        set_field_func_t        set_fn = nullptr; /**< Function to set the field value. */
    };

    /**
     * @struct RecordRuntimeMeta
     * @brief Metadata for record types (classes and structs).
     */
    struct RecordRuntimeMeta
    {
		using dtor_func_t = void(*)(bool placement, void* obj);

        BasicTypeMeta           basic_info;                 /**< Basic type information for the record. */
        ObjectTypeMeta          object_info;                /**< Object-related metadata such as size and alignment. */
        std::vector<size_t>     base_hashs;                 /**< List of unique hashes for base classes. */
        std::vector<size_t>     ctor_hashs;                 /**< List of unique hashes for constructors. */
        dtor_func_t             dtor;                       /**< Unique hash for the destructor. */
        std::vector<size_t>     field_meta_hashs;           /**< List of unique hashes for field metadata. */
        std::vector<size_t>     method_meta_hashs;          /**< List of unique hashes for method metadata. */
        std::vector<size_t>     static_method_meta_hashs;   /**< List of unique hashes for static method metadata. */
		bool					is_abstract;
    };

    /**
     * @struct EnumRuntimeMeta
     * @brief Metadata for enumeration types.
     */
    struct EnumRuntimeMeta
    {
        /**
         * @struct Enumerator
         * @brief Represents a single enumerator within an enum.
         */
        struct Enumerator
        {
            std::string_view name;    /**< Name of the enumerator. */
            long long signed_value;   /**< Value of the enumerator (stored as a signed long long). */
            unsigned long long unsigned_value; /**< Value of the enumerator (stored as an unsigned long long). */
        };

        BasicTypeMeta basic_info;            /**< Basic type information for the enum. */
        bool is_scoped;                      /**< True if the enum is scoped (e.g., enum class). */
        size_t underlying_type_hash;         /**< Unique hash of the underlying type (e.g., int, unsigned int). */
        std::vector<Enumerator> enumerators; /**< List of enumerators in the enum. */
    };
} // namespace lux::cxx::dref::runtime
