#pragma once
/*
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

#include <string>
#include <vector>
#include <clang-c/Index.h>

namespace lux::cxx::dref {

    /**
     * Enumeration of different type categories in C/C++.
     * Used to classify whether a type is builtin, pointer, reference, etc.
     */
    enum class ETypeKind {
        BUILTIN,
        POINTER,
        LVALUE_REFERENCE,
        RVALUE_REFERENCE,
        RECORD,     ///< Class/struct/union
        ENUM,       ///< Enumeration
        FUNCTION,   ///< Function type
        ARRAY,
        UNSUPPORTED ///< Fallback for unhandled/unrecognized types
    };

    /**
     * Forward declarations for specific type classes so we can reference them
     * inside the TypeVisitor before defining them.
     */
    class BuiltinType;
    class PointerType;
    class LValueReferenceType;
    class RValueReferenceType;
    class RecordType;
    class EnumType;
    class FunctionType;

    /**
     * Interface for visiting different concrete Type classes.
     * This follows a Visitor pattern, similar to the DeclVisitor above.
     */
    class TypeVisitor
    {
    public:
        virtual void visit(class Type*) = 0;
        virtual void visit(BuiltinType*) = 0;
        virtual void visit(PointerType*) = 0;
        virtual void visit(LValueReferenceType*) = 0;
        virtual void visit(RValueReferenceType*) = 0;
        virtual void visit(RecordType*) = 0;
        virtual void visit(EnumType*) = 0;
        virtual void visit(FunctionType*) = 0;
    };

    /**
     * The base class for all type representations.
     * Stores name, ID, kind, and flags like const/volatile.
     */
    class Type
    {
    public:
        virtual ~Type() = default;

        std::string name; ///< A descriptive name, used for debugging or display.
        std::string id;   ///< A unique identifier for internal referencing.
        ETypeKind   kind; ///< The classification of this type (e.g. BUILTIN, POINTER).
        bool is_const{ false };    ///< True if the type is marked as const.
        bool is_volatile{ false }; ///< True if the type is marked as volatile.
        int  size;  ///< Size in bytes (if known), otherwise could be set to -1 or 0 if unknown.
        int  align; ///< Alignment requirement in bytes (if known).

        /**
         * Accepts a type visitor to allow external operations on this object
         * based on its concrete dynamic type.
         */
        virtual void accept(TypeVisitor* visitor) {
            visitor->visit(this);
        }
    };

    /**
     * Represents a type that is unsupported by this reflection system.
     * This can serve as a fallback for types we have not (yet) implemented.
     */
    class UnsupportedType : public Type {};

    class TagDecl;
    /**
     * TagType: A base for types that are backed by a TagDecl
     * (struct, class, union, enum). This links the type to the declaration.
     */
    class TagType : public Type
    {
    public:
        TagDecl* decl = nullptr; ///< Reference to the corresponding TagDecl node.
    };

    //==================================================================
    // 1) BuiltinType: e.g. int, float, bool, char, double, etc.
    //==================================================================

    class BuiltinType final : public Type
    {
    public:
        static constexpr auto static_kind = ETypeKind::BUILTIN;

        /**
         * EBuiltinKind enumerates a variety of built-in types recognized by Clang.
         * The mapping uses CXType_* values for direct correlation.
         */
        enum class EBuiltinKind {
            VOID = CXType_Void,
            BOOL = CXType_Bool,
            CHAR_U = CXType_Char_U,
            UCHAR = CXType_UChar,
            CHAR16_T = CXType_Char16,
            CHAR32_T = CXType_Char32,
            UNSIGNED_SHORT_INT = CXType_UShort,
            UNSIGNED_INT = CXType_UInt,
            UNSIGNED_LONG_INT = CXType_ULong,
            UNSIGNED_LONG_LONG_INT = CXType_ULongLong,
            EXTENDED_UNSIGNED = CXType_UInt128,
            SIGNED_CHAR_S = CXType_Char_S,
            SIGNED_SIGNED_CHAR = CXType_SChar,
            WCHAR_T = CXType_WChar,
            SHORT_INT = CXType_Short,
            INT = CXType_Int,
            LONG_INT = CXType_Long,
            LONG_LONG_INT = CXType_LongLong,
            EXTENDED_SIGNED = CXType_Int128,
            FLOAT = CXType_Float,
            DOUBLE = CXType_Double,
            LONG_DOUBLE = CXType_LongDouble,
            NULLPTR = CXType_NullPtr,
            OVERLOAD = CXType_Overload,
            DEPENDENT = CXType_Dependent,
            OBJC_IDENTIFIER = CXType_ObjCId,
            OBJC_CLASS = CXType_ObjCClass,
            OBJC_SEL = CXType_ObjCSel,
            FLOAT_128 = CXType_Float128,
            HALF = CXType_Half,
            FLOAT16 = CXType_Float16,
            SHORT_ACCUM = CXType_ShortAccum,
            ACCUM = CXType_Accum,
            LONG_ACCUM = CXType_LongAccum,
            UNSIGNED_SHORT_ACCUM = CXType_UShortAccum,
            UNSIGNED_ACCUM = CXType_UAccum,
            UNSIGNED_LONG_ACCUM = CXType_UShortAccum,
            BFLOAT16 = CXType_BFloat16,
            IMB_128 = CXType_Ibm128,
        };

        /**
         * Checks whether this built-in type is a signed integer type (e.g., int, long).
         */
        bool isSignedInteger() const {
            switch (builtin_type) {
            case EBuiltinKind::SIGNED_CHAR_S:
            case EBuiltinKind::SIGNED_SIGNED_CHAR:
            case EBuiltinKind::SHORT_INT:
            case EBuiltinKind::INT:
            case EBuiltinKind::LONG_INT:
            case EBuiltinKind::LONG_LONG_INT:
            case EBuiltinKind::EXTENDED_SIGNED:
                return true;
            default:
                return false;
            }
        }

        /**
         * Checks whether this built-in type is a floating-point type
         * (float, double, long double, etc.).
         */
        bool isFloat() const {
            switch (builtin_type) {
            case EBuiltinKind::FLOAT:
            case EBuiltinKind::DOUBLE:
            case EBuiltinKind::LONG_DOUBLE:
            case EBuiltinKind::FLOAT_128:
            case EBuiltinKind::HALF:
            case EBuiltinKind::FLOAT16:
            case EBuiltinKind::BFLOAT16:
                return true;
            default:
                return false;
            }
        }

        /**
         * Checks whether this built-in type is any integral type,
         * including bool, char, and all forms of signed/unsigned integers.
         */
        bool isIntegral() const {
            switch (builtin_type) {
                // Boolean and character-like types also fall under integral.
            case EBuiltinKind::BOOL:
            case EBuiltinKind::CHAR_U:
            case EBuiltinKind::UCHAR:
            case EBuiltinKind::CHAR16_T:
            case EBuiltinKind::CHAR32_T:
            case EBuiltinKind::WCHAR_T:
                // Unsigned integer types
            case EBuiltinKind::UNSIGNED_SHORT_INT:
            case EBuiltinKind::UNSIGNED_INT:
            case EBuiltinKind::UNSIGNED_LONG_INT:
            case EBuiltinKind::UNSIGNED_LONG_LONG_INT:
            case EBuiltinKind::EXTENDED_UNSIGNED:
                // Signed integer types
            case EBuiltinKind::SIGNED_CHAR_S:
            case EBuiltinKind::SIGNED_SIGNED_CHAR:
            case EBuiltinKind::SHORT_INT:
            case EBuiltinKind::INT:
            case EBuiltinKind::LONG_INT:
            case EBuiltinKind::LONG_LONG_INT:
            case EBuiltinKind::EXTENDED_SIGNED:
                return true;
            default:
                return false;
            }
        }

        /**
         * Checks whether this built-in type is some variant of fixed-point
         * fractional type (accum).
         */
        bool isAccum() const
        {
            switch (builtin_type)
            {
            case EBuiltinKind::SHORT_ACCUM:
            case EBuiltinKind::ACCUM:
            case EBuiltinKind::LONG_ACCUM:
            case EBuiltinKind::UNSIGNED_SHORT_ACCUM:
            case EBuiltinKind::UNSIGNED_ACCUM:
                return true;
            default:
                return false;
            }
        }

        EBuiltinKind builtin_type;
    };

    //==================================================================
    // 2) PointerType
    //==================================================================

    /**
     * PointerType: Represents a pointer T*.
     */
    class PointerType final : public Type
    {
    public:
        static constexpr auto static_kind = ETypeKind::POINTER;

        Type* pointee = nullptr;   ///< The type being pointed to.
        bool  is_pointer_to_member{ false }; ///< True if it's a pointer-to-member type (C++ feature).
    };

    //==================================================================
    // 3) ReferenceType -> LValueReferenceType / RValueReferenceType
    //==================================================================

    /**
     * ReferenceType: Base class for both LValue and RValue reference types,
     * holding the "referred" type.
     */
    class ReferenceType : public Type
    {
    public:
        Type* referred_type = nullptr; ///< The type that is referenced.
    };

    /**
     * LValueReferenceType: Represents an lvalue reference type T&.
     */
    class LValueReferenceType final : public ReferenceType
    {
    public:
        static constexpr auto static_kind = ETypeKind::LVALUE_REFERENCE;
    };

    /**
     * RValueReferenceType: Represents an rvalue reference type T&&.
     */
    class RValueReferenceType final : public ReferenceType
    {
    public:
        static constexpr auto static_kind = ETypeKind::RVALUE_REFERENCE;
    };

    //==================================================================
    // 4) RecordType
    //==================================================================

    /**
     * RecordType: Represents a class/struct/union type.
     * It references the corresponding RecordDecl or CXXRecordDecl via TagDecl.
     */
    class RecordType final : public TagType
    {
    public:
        static constexpr auto static_kind = ETypeKind::RECORD;
    };

    //==================================================================
    // 5) EnumType
    //==================================================================

    /**
     * EnumType: Represents an enumerated type.
     * It references the corresponding EnumDecl via TagDecl.
     */
    class EnumType final : public TagType
    {
    public:
        static constexpr auto static_kind = ETypeKind::ENUM;
    };

    //==================================================================
    // 6) FunctionType
    //==================================================================

    /**
     * FunctionType: Represents a function type, including the return type
     * and parameter types. Also indicates if it's variadic (...).
     */
    class FunctionType final : public Type
    {
    public:
        static constexpr auto static_kind = ETypeKind::FUNCTION;

        Type* result_type = nullptr;         ///< The function's return type.
        std::vector<Type*> param_types;      ///< The list of parameter types.
        bool is_variadic = false;            ///< True if this function accepts variadic arguments.
    };

} // namespace lux::cxx::dref
