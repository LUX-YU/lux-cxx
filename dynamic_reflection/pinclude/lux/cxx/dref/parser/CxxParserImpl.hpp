#pragma once
#include "libclang.hpp"
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/runtime/MetaUnitImpl.hpp>
#include <lux/cxx/lan_model/declaration.hpp>
#include <lux/cxx/lan_model/types.hpp>
#include <memory>

namespace lux::cxx::dref
{
    class MetaUnit;

    class CxxParserImpl
    {
    public:
        CxxParserImpl();

        ~CxxParserImpl();

        ParseResult parse(std::string_view file, std::vector<std::string_view> commands, std::string_view name, std::string_view version);

        // static size_t declaration_id(const Cursor& cursor);
        static size_t declaration_id(EDeclarationKind kind, std::string_view name);
        static size_t declaration_id(const std::unique_ptr<Declaration>& decl);
        static size_t type_meta_id(std::string_view name);

        static std::string fullQualifiedName(const Cursor& cursor);
        static std::string fullQualifiedParameterName(const Cursor& cursor, size_t index);

        static ::lux::cxx::lan_model::Visibility visibilityFromClangVisibility(CX_CXXAccessSpecifier);

        static std::string annotationFromClangCursor(const Cursor&);
    private:
        
        [[nodiscard]] TranslationUnit   translate(std::string_view file, std::vector<std::string_view> commands) const;

        std::vector<Cursor>             findMarkedCursors(const Cursor& root_cursor) const;
        Declaration*              parseDeclaration(const Cursor& cursor);

        template<EDeclarationKind E, typename T = typename lux::cxx::lan_model::declaration_kind_map<E>::type>
        void TParseDeclaration(const Cursor&, T*);

        template<EDeclarationKind E>
        TypeMeta* TDeclTypeGen(const Cursor& cursor)
        {
            TypeMeta* type_meta{ nullptr };

            if constexpr (E == EDeclarationKind::CLASS)
            {
                type_meta = TParseTypeMetaDecorator<ETypeMetaKind::CLASS>(cursor.cursorType());
            }
            else if constexpr (E == EDeclarationKind::MEMBER_FUNCTION || E == EDeclarationKind::CONSTRUCTOR
                || E == EDeclarationKind::DESTRUCTOR || E == EDeclarationKind::FUNCTION)
            {
                type_meta = TParseTypeMetaDecorator<ETypeMetaKind::FUNCTION>(cursor.cursorType());
            }
            else if constexpr (E == EDeclarationKind::ENUMERATION)
            {
                type_meta = TParseTypeMetaDecorator<ETypeMetaKind::ENUMERATION>(cursor.cursorType());
            }
            else
            {
                type_meta = parseUncertainTypeMeta(cursor.cursorType());
            }

            return type_meta;
        }

        template<EDeclarationKind E, typename T = typename lux::cxx::lan_model::declaration_kind_map<E>::type>
        T* TParseDeclarationDecorator(const Cursor& cursor, const bool is_marked = false)
        {
            const auto usr = cursor.USR().to_std();
            auto id = declaration_id(E, usr);

            if (hasDeclarationInContextById(id))
            {
                return static_cast<T*>(getDeclarationFromContextById(id));
            }

            auto type_meta = TDeclTypeGen<E>(cursor);
            if (type_meta == nullptr)
            {
                return nullptr;
            }

            T declaration;
            declaration.type                   = type_meta;
            declaration.declaration_kind       = E;
            declaration.name                   = cursor.displayName().to_std();
            declaration.full_qualified_name    = fullQualifiedName(cursor);
            declaration.spelling               = cursor.cursorSpelling().to_std();
            declaration.usr                    = usr;
            declaration.attribute              = annotationFromClangCursor(cursor);

            auto raw_ptr = TRegisterDeclaration<E>(is_marked, id, declaration);

            if constexpr (E == EDeclarationKind::CLASS || E == EDeclarationKind::ENUMERATION)
            {
                type_meta->declaration = raw_ptr;
            }

            TParseDeclaration<E>(cursor, raw_ptr);

            return raw_ptr;
        }

        template<EDeclarationKind E, typename T = typename lux::cxx::lan_model::declaration_kind_map<E>::type>
        void TParseLocalDeclarationDecorator(const Cursor& cursor, T& declaration)
        {
            const auto usr = cursor.USR().to_std();

            auto type_meta = TDeclTypeGen<E>(cursor);

            declaration.type                   = type_meta;
            declaration.declaration_kind       = E;
            declaration.name                   = cursor.displayName().to_std();
            declaration.full_qualified_name    = fullQualifiedName(cursor);
            declaration.spelling               = cursor.cursorSpelling().to_std();
            declaration.usr                    = usr;
            declaration.attribute              = annotationFromClangCursor(cursor);

            TParseDeclaration<E>(cursor, &declaration);
        }

        void parseParameter(const Cursor& cursor, size_t index, lux::cxx::lan_model::ParameterDeclaration& declaration)
        {
            const auto full_qualified_name = fullQualifiedParameterName(cursor, index);
            const auto display_name = cursor.displayName().to_std();

            auto type_meta = TDeclTypeGen<EDeclarationKind::PARAMETER>(cursor);
            declaration.type                   = type_meta;
            declaration.declaration_kind       = EDeclarationKind::PARAMETER;
            declaration.name                   = cursor.displayName().to_std();
            declaration.full_qualified_name    = full_qualified_name;
            declaration.spelling               = cursor.cursorSpelling().to_std();
            declaration.usr                    = ""; // parameter declaration doesn't have usr'
            declaration.attribute              = annotationFromClangCursor(cursor);
        }

        template<ETypeMetaKind E, typename T = typename lux::cxx::lan_model::type_kind_map<E>::type>
        void TParseTypeMeta(const Type&, TypeMeta*);

        template<ETypeMetaKind E>
        TypeMeta* TParseTypeMetaDecorator(const Type& clang_type)
        {
            const auto canonical_type = clang_type.canonicalType();
            const auto canonical_type_name = canonical_type.typeSpelling().to_std();
            const auto id = type_meta_id(canonical_type_name);
            if (hasTypeMetaInContextById(id))
            {
                return getTypeMetaFromContextById(id);
            }

            return TParseTypeMetaDecoratorNoCheck<E>(canonical_type, id);
        }

        template<ETypeMetaKind E>
        TypeMeta* TParseTypeMetaDecoratorNoCheck(const Type& type, size_t id)
        {
            TypeMeta meta_type;
            meta_type.type_kind    = E;
            meta_type.is_const     = type.isConstQualifiedType();
            meta_type.is_volatile  = type.isVolatileQualifiedType();
            meta_type.name         = type.typeSpelling().to_std();
            meta_type.size         = type.typeSizeof();
            meta_type.align        = type.typeAlignof();

            TypeMeta* raw_ptr      = registerTypeMeta(id, meta_type);
            TParseTypeMeta<E>(type, raw_ptr);
            return raw_ptr;
        }

        TypeMeta* parseUncertainTypeMeta(const Type& type);
        
        [[nodiscard]] bool hasDeclarationInContextById(size_t id) const;
        [[nodiscard]] bool hasDeclarationInContextByName(EDeclarationKind kind, const char* name) const;
        Declaration* getDeclarationFromContextById(size_t id);
        Declaration* getDeclarationFromContextByName(EDeclarationKind kind, const char* name);

        [[nodiscard]] bool hasTypeMetaInContextById(size_t id) const;
        [[nodiscard]] bool hasTypeMetaInContextByName(const char* name) const;
        TypeMeta* getTypeMetaFromContextById(size_t id) const;
        TypeMeta* getTypeMetaFromContextByName(EDeclarationKind kind, std::string_view name);

        TypeMeta* registerTypeMeta(size_t, const TypeMeta&);

        lan_model::ClassDeclaration*        registerMarkedClassDeclaration(size_t, const lan_model::ClassDeclaration&);
        lan_model::FunctionDeclaration*     registerMarkedFunctionDeclaration(size_t, const lan_model::FunctionDeclaration&);
        lan_model::EnumerationDeclaration*  registerMarkedEnumerationDeclaration(size_t, const lan_model::EnumerationDeclaration&);

        lan_model::ClassDeclaration*        registerUnmarkedClassDeclaration(size_t, const lan_model::ClassDeclaration&);
        lan_model::FunctionDeclaration*     registerUnmarkedFunctionDeclaration(size_t, const lan_model::FunctionDeclaration&);
        lan_model::EnumerationDeclaration*  registerUnmarkedEnumerationDeclaration(size_t, const lan_model::EnumerationDeclaration&);

        template<EDeclarationKind E, typename T = lan_model::declaration_kind_map<E>::type>
        T* TRegisterDeclaration(bool is_marked, size_t id, const typename lan_model::declaration_kind_map<E>::type& decl)
        {
            static_assert(
                E == EDeclarationKind::CLASS ||
                E == EDeclarationKind::FUNCTION ||
                E == EDeclarationKind::ENUMERATION
            );

            if(is_marked)
            {
                if constexpr (E == EDeclarationKind::CLASS)
                {
                    return registerMarkedClassDeclaration(id, decl);
                }
                else if constexpr(E == EDeclarationKind::FUNCTION)
                {
                    return registerMarkedFunctionDeclaration(id, decl);
                }
                else if constexpr(E == EDeclarationKind::ENUMERATION)
                {
                    return registerMarkedEnumerationDeclaration(id, decl);
                }
            }
            else
            {
                if constexpr (E == EDeclarationKind::CLASS)
                {
                    return registerUnmarkedClassDeclaration(id, decl);
                }
                else if constexpr(E == EDeclarationKind::FUNCTION)
                {
                    return registerUnmarkedFunctionDeclaration(id, decl);
                }
                else if constexpr(E == EDeclarationKind::ENUMERATION)
                {
                    return registerUnmarkedEnumerationDeclaration(id, decl);
                }
            }
            return nullptr; // unreachable
        }

        template<typename T>
        std::enable_if_t<std::is_base_of_v<::lux::cxx::lan_model::CallableDeclCommon, T>> 
        callableDeclInit(T* declaration, const Cursor& cursor)
        {
            using namespace ::lux::cxx::lan_model;
            declaration->result_type = parseUncertainTypeMeta(cursor.cursorResultType());
            declaration->mangling    = cursor.mangling().to_std();
            for(size_t i = 0; i < cursor.numArguments(); i++)
            {
                declaration->parameter_decls.push_back({});
                parseParameter(
                    cursor.getArgument(i), i,
                    declaration->parameter_decls.back()
                );
            }
        }

        static Type rootAnonicalType(const Type& type);

        template<ETypeMetaKind E1, ETypeMetaKind E2>
        TypeMeta* parseReferenceSemtantic(const Type& type, size_t id)
        {
            auto ref_type  = rootAnonicalType(type.getPointeeType());
            auto type_kind = ref_type.typeKind();
            if (type_kind != CXType_FunctionProto)
            {
                return TParseTypeMetaDecoratorNoCheck<E1>(type, id);
            }
            return TParseTypeMetaDecoratorNoCheck<E2>(type, id);
        }

        // shared context for creating translation units
        std::string                     _pch_file;
        std::unique_ptr<MetaUnitData>   _meta_unit_data;
        CXIndex                         _clang_index;
    };
} // namespace lux::reflection
