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
#include <cstddef>
#include <cstdint>

namespace lux::cxx::dref {
    enum class EVisibility {
        INVALID = 0,  /**< Invalid visibility. */
        PUBLIC,       /**< Public access specifier. */
        PROTECTED,    /**< Protected access specifier. */
        PRIVATE       /**< Private access specifier. */
    };

    /**
     * Forward declarations to allow referencing these classes below.
     * They will be defined later in this header.
     */
    class Type;
    class CXXConstructorDecl;
    class CXXDestructorDecl;

    /**
     * A subset of a complete C++ declaration system.
     *
     * Decl (Base declaration)
     *  └── NamedDecl
     *       ├── TypeDecl
     *       │    └── TagDecl
     *       │         ├── EnumDecl
     *       │         └── RecordDecl
     *       │              └── CXXRecordDecl
     *       └── ValueDecl
     *            └── DeclaratorDecl
     *                 ├── FieldDecl
     *                 ├── FunctionDecl
     *                 │    └── CXXMethodDecl
     *                 │         ├── CXXConstructorDecl
     *                 │         ├── CXXConversionDecl
     *                 │         └── CXXDestructorDecl
     *                 └── VarDecl
     *                      ├── ImplicitParamDecl
     *                      ├── NonTypeTemplateParmDecl
     *                      └── ParmVarDecl
     *
     * This hierarchy is used to represent different kinds of declarations
     * (classes, fields, methods, parameters, etc.) in a structured manner
     * similar to Clang's AST.
     */
    enum class EDeclKind
    {
        UNKNOWN,
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

    /**
     * Visitor interface to traverse various concrete declaration nodes.
     * Subclasses that implement this interface can be used to perform
     * operations (e.g., printing, code generation, analysis) on specific
     * declaration types.
     */
    class EnumDecl;
    class RecordDecl;
    class CXXRecordDecl;
    class CXXMethodDecl;
    class FieldDecl;
    class VarDecl;
    class ParmVarDecl;
    class FunctionDecl;
	class CXXConstructorDecl;
	class CXXConversionDecl;
	class CXXDestructorDecl;

    class DeclVisitor
    {
    public:
        virtual ~DeclVisitor() = default;

        // Visit methods for each concrete decl type.
        virtual void visit(EnumDecl*) {};
        virtual void visit(RecordDecl*) {};
        virtual void visit(CXXRecordDecl*) {};
        virtual void visit(FieldDecl*) {};
        virtual void visit(FunctionDecl*) {};
        virtual void visit(CXXMethodDecl*) {};
        virtual void visit(ParmVarDecl*) {};

        virtual void visit(CXXConstructorDecl* decl) {};
        virtual void visit(CXXConversionDecl* decl) {};
        virtual void visit(CXXDestructorDecl* decl) {};
        virtual void visit(VarDecl* decl) {};
    };

    /**
     * Base class for all declaration nodes.
     */
    class Decl
    {
    public:
        std::string id;     ///< A unique identifier for internal bookkeeping.
        uint64_t    hash;
        EDeclKind   kind;   ///< The specific kind of declaration (see EDeclKind).
		size_t      index;  ///< The index of this declaration in its parent container (if applicable).

        virtual ~Decl() = default;

        /**
         * Accepts a visitor, which then calls the appropriate overloaded
         * visit method based on the dynamic type of this declaration.
         */
        virtual void accept(DeclVisitor*) {};
    };

    /**
     * NamedDecl: Declarations that have an associated name (e.g., a class name, a function name).
     */
    class NamedDecl : public Decl
    {
    public:
        std::string name;                    ///< The user-visible name of the declaration.
        std::string full_qualified_name;     ///< The fully qualified name (including namespaces, classes, etc.).
        std::string spelling;                ///< The exact spelling in the source code.
        std::vector<std::string> attributes; ///< Additional attributes or annotations on the declaration.
		bool is_anonymous = false;           ///< True if this declaration is anonymous (e.g., unnamed struct).
        Type* type = nullptr;                ///< Optional type information if applicable.
    };

    //======================================================================
    // Part A: TypeDecl => TagDecl => EnumDecl, RecordDecl => CXXRecordDecl
    //======================================================================

    /**
     * TypeDecl: Declarations that introduce a type name, such as class, struct,
     * union, enum, typedef, etc. In Clang, TypeDecl also inherits from NamedDecl.
     */
    class TypeDecl : public NamedDecl {};

    /**
     * TagDecl: Declarations for tagged types in C/C++ such as struct, union,
     * class, and enum. For simplicity, here we only showcase Record (class/struct/union)
     * and Enum.
     */
    class TagDecl : public TypeDecl
    {
    public:
        /**
         * The specific kind of tagged type: struct, class, union, or enum.
         */
        enum class ETagKind
        {
            Struct,
            Class,
            Union,
            Enum
        };

        ETagKind tag_kind;
    };

    /**
     * A helper structure representing each enumerator inside an EnumDecl.
     */
    struct Enumerator
    {
        std::string name;       ///< The name of the enumerator.
        int64_t     signed_value;    ///< The signed integer value it represents.
        uint64_t    unsigned_value;  ///< The unsigned integer value it represents.
    };

    class BuiltinType; // Forward declaration for use within EnumDecl.

    /**
     * EnumDecl: Represents an enumeration type in C++.
     */
    class EnumDecl final : public TagDecl
    {
    public:
        bool is_scoped = false;            ///< True if this enum is declared as 'enum class'.
        BuiltinType* underlying_type = nullptr; ///< The underlying integer type of the enum.
        std::vector<Enumerator> enumerators;    ///< The list of enumerators in this enum.

        ~EnumDecl() override = default;

        void accept(DeclVisitor* visitor) override
        {
            visitor->visit(this);
        }
    };

    /**
     * RecordDecl: Represents struct/class/union in C/C++. In Clang, RecordDecl doesn't
     * necessarily know if it is a CXXRecordDecl (only in C++). It's a more general
     * node for record-like types.
     */
    class RecordDecl : public TagDecl
    {
    public:
        // Basic fields or methods can be stored here.
        // For plain C structs or unions, you might not have C++-specific info.

        void accept(DeclVisitor* visitor) override
        {
            visitor->visit(this);
        }
    };

    /**
     * CXXRecordDecl: Represents a C++ class or struct, containing additional
     * information such as base classes, constructors, destructor, and methods.
     */
    class CXXRecordDecl final : public RecordDecl
    {
    public:
        /// List of direct base classes (C++ inheritance).
        std::vector<CXXRecordDecl*> bases;

        /**
         * Declarations for constructors, destructor, normal (non-static) methods,
         * static methods, and fields in this class.
         */
        std::vector<CXXConstructorDecl*> constructor_decls;
        CXXDestructorDecl*               destructor_decl = nullptr;
        std::vector<CXXMethodDecl*>      method_decls;
        std::vector<CXXMethodDecl*>      static_method_decls;
        std::vector<class FieldDecl*>    field_decls;
		bool is_abstract = false; ///< True if this class is abstract (has pure virtual methods).

        void accept(DeclVisitor* visitor) override
        {
            visitor->visit(this);
        }
    };

    //======================================================================
    // Part B: ValueDecl => DeclaratorDecl => FieldDecl, FunctionDecl, VarDecl ...
    //======================================================================

    /**
     * ValueDecl: Represents declarations that can be used as a value,
     * such as variables and functions. In Clang, ValueDecl inherits NamedDecl.
     */
    class ValueDecl : public NamedDecl {};

    /**
     * DeclaratorDecl: Inherits from ValueDecl, representing declarations that
     * come from the C/C++ "declarator" syntax. This includes variables, functions,
     * fields, parameters, etc.
     */
    class DeclaratorDecl : public ValueDecl {};

    /**
     * FieldDecl: Represents a non-static data member in a struct/class/union.
     */
    class FieldDecl final : public DeclaratorDecl
    {
    public:
        EVisibility visibility = EVisibility::INVALID; ///< e.g., public, protected, private (if relevant).
        std::size_t offset {}; ///< The field offset in bytes (or bits, depending on usage).

		CXXRecordDecl* parent_class = nullptr; ///< The class where this field is declared.

        void accept(DeclVisitor* visitor) override
        {
            visitor->visit(this);
        }
    };

    /**
     * FunctionDecl: Represents a function in C/C++ (including free functions).
     */
    class FunctionDecl : public DeclaratorDecl
    {
    public:
        ///< The return type of the function.
        Type* result_type = nullptr;  
        ///< The parameter list of the function.
        std::vector<class ParmVarDecl*> params; 
        /// The name used to invoke this function.For member functions, it's spelling, for non member functions, it's namespace::spelling.
		std::string invoke_name; 
        ///< The mangled name, if relevant for linkage.
        std::string mangling; 

		bool is_variadic = false; ///< True if this function accepts variadic arguments.

        void accept(DeclVisitor* visitor) override
        {
            visitor->visit(this);
        }
    };

    /**
     * CXXMethodDecl: Represents a non-constructor/destructor method declared in a C++ class.
     * It can also represent static methods, virtual methods, etc.
     */
    class CXXMethodDecl : public FunctionDecl
    {
    public:
        EVisibility visibility = EVisibility::INVALID; ///< Visibility (public, protected, private).

        bool is_static = false; ///< True if this is a static method.
        bool is_virtual = false; ///< True if this method is declared virtual.
        bool is_const = false; ///< True if this method is declared const.
		bool is_volatile = false; ///< True if this method is declared volatile.

		CXXRecordDecl* parent_class = nullptr; ///< The class where this method is declared.

        void accept(DeclVisitor* visitor) override
        {
            visitor->visit(this);
        }
    };

    /**
     * CXXConstructorDecl: Represents a constructor in a C++ class.
     */
    class CXXConstructorDecl final : public CXXMethodDecl {};

    /**
     * CXXConversionDecl: Represents a user-defined conversion function in C++ (e.g. operator int()).
     */
    class CXXConversionDecl final : public CXXMethodDecl {};

    /**
     * CXXDestructorDecl: Represents a destructor in a C++ class.
     */
    class CXXDestructorDecl final : public CXXMethodDecl {};

    /**
     * VarDecl: Represents a variable declaration (non-parameter) in C/C++,
     * such as global/local variables.
     */
    class VarDecl : public DeclaratorDecl {};

    /**
     * ParmVarDecl: Represents a parameter variable declaration in a function parameter list.
     */
    class ParmVarDecl final : public VarDecl
    {
    public:
        std::size_t arg_index {};  ///< The index of this parameter in the function parameter list.

        void accept(DeclVisitor* visitor) override
        {
            visitor->visit(this);
        }
    };
} // namespace lux::cxx::dref
