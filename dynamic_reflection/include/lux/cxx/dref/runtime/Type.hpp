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

namespace lux::cxx::dref {
    enum class ETypeKinds {
        Unknown = 0,            /**< Unknown type. */
        Imcomplete,             /**< Incomplete type. (Note: "Imcomplete" is a known spelling issue.) */
		Builtin,                /**< Built-in type. */
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
		Pointer,                /**< Pointer type. */
        PointerToObject,        /**< Pointer to an object. */
        PointerToFunction,      /**< Pointer to a function. */
        // Pointer-to-member types
        PointerToDataMember,    /**< Pointer to a data member. */
        PointerToMemberFunction,/**< Pointer to a member function. */
        Array,                  /**< Array type. */
        Function,               /**< Function type. */
        // Enumeration types
        Enum,
        UnscopedEnum,           /**< Unscoped enumeration. */
        ScopedEnum,             /**< Scoped enumeration (C++11). */
		Record,                /**< Record type (struct/class). */
        // Class types
        Class,                  /**< Non-union class type. */
        Union                   /**< Union type. */
    };

    /**
     * Forward declarations for specific type classes so we can reference them
     * inside the TypeVisitor before defining them.
     */
    class Type;
    class BuiltinType;
    class PointerType;
    class LValueReferenceType;
    class RValueReferenceType;
    class RecordType;
    class EnumType;
    class FunctionType;
    class UnsupportedType;

    /**
     * Interface for visiting different concrete Type classes.
     * This follows a Visitor pattern, similar to the DeclVisitor above.
     */
    class TypeVisitor
    {
    public:
		// virtual void visit(Type*) = 0;
        virtual void visit(BuiltinType*) = 0;
        virtual void visit(PointerType*) = 0;
        virtual void visit(LValueReferenceType*) = 0;
        virtual void visit(RValueReferenceType*) = 0;
        virtual void visit(RecordType*) = 0;
        virtual void visit(EnumType*) = 0;
        virtual void visit(FunctionType*) = 0;

		virtual void visit(UnsupportedType*) = 0;
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
        ETypeKinds  kind; ///< The classification of this type (e.g. BUILTIN, POINTER).
        bool        is_const{ false };    ///< True if the type is marked as const.
        bool        is_volatile{ false }; ///< True if the type is marked as volatile.
        int         size;  ///< Size in bytes (if known), otherwise could be set to -1 or 0 if unknown.
        int         align; ///< Alignment requirement in bytes (if known).
        size_t      index; ///< The index of this type in its parent container (if applicable).

        /**
         * Accepts a type visitor to allow external operations on this object
         * based on its concrete dynamic type.
         */
        virtual void accept(TypeVisitor* visitor) = 0;
    };

    /**
     * Represents a type that is unsupported by this reflection system.
     * This can serve as a fallback for types we have not (yet) implemented.
     */
    class UnsupportedType : public Type {
    public:
        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
    };

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
        static constexpr auto static_kind = ETypeKinds::Builtin;

        /**
         * EBuiltinKind enumerates a variety of built-in types recognized by Clang.
         * The mapping uses CXType_* values for direct correlation.
         */
        enum class EBuiltinKind {
            VOID,
            BOOL,
            CHAR_U,
            UCHAR,
            CHAR16_T,
            CHAR32_T,
            UNSIGNED_SHORT_INT,
            UNSIGNED_INT,
            UNSIGNED_LONG_INT,
            UNSIGNED_LONG_LONG_INT,
            EXTENDED_UNSIGNED,
            SIGNED_CHAR_S,
            SIGNED_SIGNED_CHAR,
            WCHAR_T,
            SHORT_INT,
            INT,
            LONG_INT,
            LONG_LONG_INT,
            EXTENDED_SIGNED,
            FLOAT,
            DOUBLE,
            LONG_DOUBLE,
            NULLPTR,
            OVERLOAD,
            DEPENDENT,
            OBJC_IDENTIFIER,
            OBJC_CLASS,
            OBJC_SEL,
            FLOAT_128,
            HALF,
            FLOAT16,
            SHORT_ACCUM,
            ACCUM,
            LONG_ACCUM,
            UNSIGNED_SHORT_ACCUM,
            UNSIGNED_ACCUM,
            UNSIGNED_LONG_ACCUM,
            BFLOAT16,
            IMB_128
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

        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
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
        static constexpr auto static_kind = ETypeKinds::Pointer;

        Type* pointee = nullptr;   ///< The type being pointed to.
        bool  is_pointer_to_member{ false }; ///< True if it's a pointer-to-member type (C++ feature).

        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
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
        static constexpr auto static_kind = ETypeKinds::LvalueReference;

        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
    };

    /**
     * RValueReferenceType: Represents an rvalue reference type T&&.
     */
    class RValueReferenceType final : public ReferenceType
    {
    public:
        static constexpr auto static_kind = ETypeKinds::RvalueReference;

        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
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
        static constexpr auto static_kind = ETypeKinds::Record;

        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
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
        static constexpr auto static_kind = ETypeKinds::Enum;

        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
    };

    //==================================================================
    // 6) FunctionType
    //==================================================================
    /**
     * FunctionType: Represents a function type, including the return type
     * and parameter types. Also indicates if it's variadic (...).
     */
    class FunctionDecl;
    class FunctionType final : public Type
    {
    public:
        static constexpr auto static_kind = ETypeKinds::Function;

        Type* result_type = nullptr;         ///< The function's return type.
        std::vector<Type*> param_types;      ///< The list of parameter types.
        bool is_variadic = false;            ///< True if this function accepts variadic arguments.

		void accept(TypeVisitor* visitor) override {
			visitor->visit(this);
		}
    };
} // namespace lux::cxx::dref
