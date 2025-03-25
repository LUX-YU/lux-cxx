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

#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>

namespace lux::cxx::dref
{
    /**
     * @brief Converts Clang's CX_CXXAccessSpecifier enum to the internal EVisibility enum.
     *
     * This function maps the access specifier provided by Clang's API to an internal
     * representation (EVisibility) used by the parser. If the specifier is invalid or
     * unrecognized, it returns EVisibility::INVALID.
     *
     * @param specifier Clang's CX_CXXAccessSpecifier value.
     * @return EVisibility corresponding to the provided specifier.
     */
    static EVisibility visibilityFromClangVisibility(const CX_CXXAccessSpecifier specifier)
    {
        switch (specifier)
        {
        case CX_CXXInvalidAccessSpecifier:
            return EVisibility::INVALID;
        case CX_CXXPublic:
            return EVisibility::PUBLIC;
        case CX_CXXProtected:
            return EVisibility::PROTECTED;
        case CX_CXXPrivate:
            return EVisibility::PRIVATE;
        }
        return EVisibility::INVALID; // Fallback for any unhandled cases
    }

    /**
     * @brief Parses a C++ method declaration.
     *
     * This function extracts method-specific properties from the given cursor,
     * such as visibility, virtual status, static status, and const qualification.
     * It then calls a general function declaration parser to handle the function
     * signature and related details.
     *
     * @param cursor A Cursor object representing the method declaration in the AST.
     * @param decl A reference to a CXXMethodDecl object that will be populated with the parsed information.
     */
    void CxxParserImpl::parseCxxMethodDecl(const Cursor& cursor, CXXMethodDecl& decl)
    {
        // Set the visibility based on the Clang access specifier
        decl.visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
        // Determine if the method is virtual, static or const-qualified
        decl.is_virtual = cursor.isMethodVirtual();
        decl.is_static = cursor.isMethodStatic();
        decl.is_const = cursor.isMethodConst();
        // Delegate to the generic function declaration parser for additional parsing
        parseFunctionDecl(cursor, decl);
    }

    /**
     * @brief Parses a field (data member) declaration.
     *
     * This function extracts details such as visibility and offset for a field,
     * and then delegates further name parsing to the common named declaration parser.
     *
     * @param cursor A Cursor object representing the field declaration in the AST.
     * @param decl A reference to a FieldDecl object that will be populated with the parsed information.
     */
    void CxxParserImpl::parseFieldDecl(const Cursor& cursor, FieldDecl& decl)
    {
        // Set the field's visibility from the cursor's access specifier
        decl.visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
        // Get the memory offset of the field in its parent record
        decl.offset = cursor.offsetOfField();
        // Parse the name and other common attributes of the declaration
        parseNamedDecl(cursor, decl);
    }

    /**
     * @brief Parses a C++ record declaration (class/struct/union).
     *
     * This function processes a C++ record declaration by visiting each of its child cursors.
     * It distinguishes between fields, methods, constructors, destructors, and base specifiers,
     * parsing each accordingly and registering the parsed declarations.
     *
     * Additionally, it sets the record's tag kind (struct, union, or class) and updates the
     * underlying record type with references to its bases, methods, and other members.
     *
     * @param cursor A Cursor object representing the record declaration.
     * @param decl A reference to a CXXRecordDecl object that will be populated with the parsed record information.
     */
    void CxxParserImpl::parseCXXRecordDecl(const Cursor& cursor, CXXRecordDecl& decl)
    {
        // Iterate over each child element in the record declaration
        cursor.visitChildren(
            [this, &decl](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
            {
                // Determine the kind of the child cursor and handle accordingly
                if (const auto cursor_kind = cursor.cursorKind(); cursor_kind == CXCursor_FieldDecl)
                {
                    // Parse a field declaration
                    auto field_decl = std::make_unique<FieldDecl>();
                    parseFieldDecl(cursor, *field_decl);
                    field_decl->kind = EDeclKind::FIELD_DECL;
                    // Add the parsed field declaration to the record's field list
                    decl.field_decls.push_back(field_decl.get());
                    // Register the declaration in the parser's registry (handles ownership)
                    registerDeclaration(std::move(field_decl));
                }
                else if (cursor_kind == CXCursor_CXXMethod)
                {
                    // Parse a non-constructor/non-destructor method declaration
                    auto method_decl = std::make_unique<CXXMethodDecl>();
                    method_decl->kind = EDeclKind::CXX_METHOD_DECL;
                    parseCxxMethodDecl(cursor, *method_decl);
                    // Distinguish between static and non-static methods
                    if (method_decl->is_static)
                    {
                        decl.static_method_decls.push_back(method_decl.get());
                    }
                    else
                    {
                        decl.method_decls.push_back(method_decl.get());
                    }
                    // Register the parsed method declaration
                    registerDeclaration(std::move(method_decl));
                }
                else if (cursor_kind == CXCursor_Constructor)
                {
                    // Parse a constructor declaration
                    auto method_decl = std::make_unique<CXXConstructorDecl>();
                    method_decl->kind = EDeclKind::CXX_CONSTRUCTOR_DECL;
                    parseCxxMethodDecl(cursor, *method_decl);
                    // Add the constructor to the record's constructor list
                    decl.constructor_decls.push_back(method_decl.get());
                    // Register the declaration
                    registerDeclaration(std::move(method_decl));
                }
                else if (cursor_kind == CXCursor_Destructor)
                {
                    // Parse a destructor declaration
                    auto method_decl = std::make_unique<CXXDestructorDecl>();
                    method_decl->kind = EDeclKind::CXX_DESTRUCTOR_DECL;
                    parseCxxMethodDecl(cursor, *method_decl);
                    // Store the destructor declaration (assuming one destructor per record)
                    decl.destructor_decl = method_decl.get();
                    // Register the destructor declaration
                    registerDeclaration(std::move(method_decl));
                }
                else if (cursor_kind == CXCursor_CXXBaseSpecifier)
                {
                    // Handle a base class specifier (inheritance)
                    const auto base_cursor = cursor.getDefinition();
                    // Check if the base class declaration has already been registered
                    if (const auto cursor_id = base_cursor.USR().to_std(); hasDeclaration(cursor_id))
                    {
                        // If found, retrieve the registered declaration and cast it to a record declaration
                        const auto base_decl = dynamic_cast<CXXRecordDecl*>(getDeclaration(cursor_id));
                        assert(base_decl != nullptr);
                        // Add the existing base class declaration to the current record's bases list
                        decl.bases.push_back(base_decl);
                    }
                    else
                    {
                        // Otherwise, create a new record declaration for the base class
                        auto base_class_decl = std::make_unique<CXXRecordDecl>();
                        parseCXXRecordDecl(base_cursor, *base_class_decl);
                        // Add the newly parsed base class to the list
                        decl.bases.push_back(base_class_decl.get());
                        // Register the new base class declaration
                        registerDeclaration(std::move(base_class_decl));
                    }
                }

                // Continue visiting other child cursors
                return CXChildVisit_Continue;
            }
        );

        // Determine the tag kind (struct, union, or class) of the record based on the cursor kind
        auto cursor_kind_enum = cursor.cursorKind().kindEnum();
        if (cursor_kind_enum == CXCursor_StructDecl)
        {
            decl.tag_kind = TagDecl::ETagKind::Struct;
        }
        else if (cursor_kind_enum == CXCursor_UnionDecl)
        {
            decl.tag_kind = TagDecl::ETagKind::Union;
        }
        else if (cursor_kind_enum == CXCursor_ClassDecl)
        {
            decl.tag_kind = TagDecl::ETagKind::Class;
        }

        // Parse common named declaration attributes (such as name, location, etc.)
        parseNamedDecl(cursor, decl);
        // Set the declaration kind for the record
        decl.kind = EDeclKind::CXX_RECORD_DECL;

        // Update the record type associated with the declaration.
        // It is assumed that decl.type is already a pointer to a RecordType.
        auto record_type = dynamic_cast<RecordType*>(decl.type);
        // Ensure the record type is valid and link it back to the declaration
        assert(record_type != nullptr);
        // Register base classes in the record type
        for (auto base : decl.bases)
        {
            record_type->bases.push_back(dynamic_cast<RecordType*>(base->type));
        }
        // Register non-static methods in the record type
        for (auto method : decl.method_decls)
        {
            record_type->method_decls.push_back(dynamic_cast<FunctionType*>(method->type));
        }
        // Register static methods in the record type
        for (auto method : decl.static_method_decls)
        {
            record_type->static_method_decls.push_back(dynamic_cast<FunctionType*>(method->type));
        }
        // Register constructors in the record type
        for (auto method : decl.constructor_decls)
        {
            record_type->constructor_decls.push_back(dynamic_cast<FunctionType*>(method->type));
        }
        // Register the destructor in the record type
        record_type->destructor_decl = dynamic_cast<FunctionType*>(decl.destructor_decl->type);
        // Register field declarations in the record type
        for (auto field : decl.field_decls)
        {
            record_type->field_decls.push_back(field->type);
        }
        record_type->decl = &decl;
    }
}
