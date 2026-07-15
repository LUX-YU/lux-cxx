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

namespace lux::cxx::reflection {
    /**
     * Kind tag for @ref Type subclasses.
     *
     * @note Several "coarse" enumerators (@ref Pointer, @ref Enum, @ref Class,
     *       @ref Union) overlap with the fine-grained ones below. The parser
     *       only emits the fine-grained variants on actual @ref Type instances;
     *       the coarse names exist as switch-fallthrough labels in the
     *       serializer and as `static_kind` constants on the concrete C++
     *       classes (@ref PointerType, @ref EnumType, @ref RecordType). Treat
     *       the coarse values as "C++ class identity" and the fine values as
     *       "semantic subtype". A future major version may collapse them.
     */
    enum class ETypeKinds {
        Unknown = 0,            /**< Unknown type. */
        Incomplete,             /**< Incomplete type (e.g. forward declaration). */
        Builtin,                /**< Built-in type (corresponds to @ref BuiltinType). */
        // Fundamental enumerators below are reserved labels — the parser emits
        // @ref Builtin and stores the concrete kind on @ref BuiltinType::builtin_type.
        Void,                   /**< void type. */
        Nullptr_t,              /**< std::nullptr_t type (C++11). */
        Bool,                   /**< Boolean type. */
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
        Float,                  /**< float type. */
        Double,                 /**< double type. */
        LongDouble,             /**< long double type. */
        // Reference types.
        LvalueReference,        /**< lvalue reference (T&). */
        RvalueReference,        /**< rvalue reference (T&&) (C++11). */
        // Pointer types — runtime class identity is @ref Pointer; the parser
        // refines this into one of the four fine-grained values below.
        Pointer,                /**< Class identity for @ref PointerType. */
        PointerToObject,        /**< T* to a non-function object. */
        PointerToFunction,      /**< Pointer to a free function. */
        PointerToDataMember,    /**< Pointer to a data member (T C::*). */
        PointerToMemberFunction,/**< Pointer to a member function. */
        Array,                  /**< Array type. */
        Function,               /**< Function type. */
        // Enumeration types — class identity is @ref Enum; parser refines to
        // @ref ScopedEnum / @ref UnscopedEnum.
        Enum,                   /**< Class identity for @ref EnumType. */
        UnscopedEnum,           /**< Unscoped enumeration. */
        ScopedEnum,             /**< Scoped enumeration (C++11). */
        // Record types — class identity is @ref Record; distinguishing struct
        // / class / union is done via @ref TagDecl::tag_kind on the backing
        // declaration, not via these enumerators.
        Record,                 /**< Class identity for @ref RecordType. */
        Class,                  /**< Reserved — not emitted by the parser. */
        Union,                  /**< Reserved — not emitted by the parser. */
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
     *
     * @note Same caveat as DeclVisitor: the in-module serializer does NOT use
     *       this interface — it dispatches via switch on @ref Type::kind.
     *       Provided for downstream consumers.
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

        std::string name;                          ///< A descriptive name, used for debugging or display.
        std::string id;                            ///< A unique identifier for internal referencing.
        ETypeKinds  kind        = ETypeKinds::Unknown; ///< The classification of this type (e.g. BUILTIN, POINTER).
        bool        is_const    = false;           ///< True if the type is marked as const.
        bool        is_volatile = false;           ///< True if the type is marked as volatile.
        std::size_t size        = 0;               ///< Size in bytes (if known), otherwise 0.
        std::size_t align       = 0;               ///< Alignment requirement in bytes (if known).
        size_t      index       = static_cast<size_t>(-1); ///< Index of this type in MetaUnitData::types.

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
        static constexpr bool classof(ETypeKinds k) noexcept { return k == ETypeKinds::Builtin; }

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

        EBuiltinKind builtin_type = EBuiltinKind::VOID;

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
        static constexpr bool classof(ETypeKinds k) noexcept
        {
            // The parser refines Pointer into the four fine-grained values.
            return k == ETypeKinds::Pointer
                || k == ETypeKinds::PointerToObject
                || k == ETypeKinds::PointerToFunction
                || k == ETypeKinds::PointerToDataMember
                || k == ETypeKinds::PointerToMemberFunction;
        }

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
        static constexpr bool classof(ETypeKinds k) noexcept { return k == ETypeKinds::LvalueReference; }

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
        static constexpr bool classof(ETypeKinds k) noexcept { return k == ETypeKinds::RvalueReference; }

        void accept(TypeVisitor* visitor) override {
            visitor->visit(this);
        }
    };

    //==================================================================
    // 4) RecordType
    //==================================================================

    /**
     * TemplateArgument: Represents a single template argument of a template
     * specialization (e.g., `int` in `std::vector<int>` or `5` in `std::array<int, 5>`).
     */
    struct TemplateArgument
    {
        enum class Kind
        {
            Type,       ///< A type argument (e.g., `int` in `vector<int>`)
            Integral,   ///< A non-type integral argument (e.g., `5` in `array<int, 5>`)
        };

        Kind         kind = Kind::Type;
        Type*        type = nullptr;     ///< The type, for Kind::Type arguments.
        int64_t      integral_value = 0; ///< The value, for Kind::Integral arguments.
        std::string  type_id;            ///< Serialization helper: the type id string.
        std::string  spelling;           ///< Human-readable spelling of this argument.
    };

    /**
     * RecordType: Represents a class/struct/union type.
     * It references the corresponding RecordDecl or CXXRecordDecl via TagDecl.
     * For template specializations (e.g., `std::vector<int>`), it also stores
     * the template name and template arguments.
     */
    class RecordType final : public TagType
    {
    public:
        static constexpr auto static_kind = ETypeKinds::Record;
        static constexpr bool classof(ETypeKinds k) noexcept
        {
            // Class/Union are reserved labels whose class identity is RecordType.
            return k == ETypeKinds::Record
                || k == ETypeKinds::Class
                || k == ETypeKinds::Union;
        }

        /// The template name without arguments (e.g., "std::vector").
        /// Empty if this is not a template specialization.
        std::string template_name;

        /// The list of template arguments. Empty if not a template specialization.
        std::vector<TemplateArgument> template_arguments;

        /// Returns true if this record type is a template specialization.
        bool isTemplateSpecialization() const { return !template_arguments.empty(); }

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
        static constexpr bool classof(ETypeKinds k) noexcept
        {
            // The parser refines Enum into ScopedEnum / UnscopedEnum.
            return k == ETypeKinds::Enum
                || k == ETypeKinds::ScopedEnum
                || k == ETypeKinds::UnscopedEnum;
        }

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
        static constexpr bool classof(ETypeKinds k) noexcept { return k == ETypeKinds::Function; }

        Type* result_type = nullptr;         ///< The function's return type.
        std::vector<Type*> param_types;      ///< The list of parameter types.
        bool is_variadic = false;            ///< True if this function accepts variadic arguments.

		void accept(TypeVisitor* visitor) override {
			visitor->visit(this);
		}
    };

    /**
     * RTTI-free replacement for dynamic_cast within the Type hierarchy
     * (the project bans RTTI): dispatches on @ref Type::kind via
     * T::classof, mirroring LLVM's isa/dyn_cast idiom.
     *
     * Returns nullptr when @p t is null or is not a T.
     */
    template<typename T>
    [[nodiscard]] T* type_cast(Type* t) noexcept
    {
        return (t && T::classof(t->kind)) ? static_cast<T*>(t) : nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* type_cast(const Type* t) noexcept
    {
        return (t && T::classof(t->kind)) ? static_cast<const T*>(t) : nullptr;
    }
} // namespace lux::cxx::reflection
