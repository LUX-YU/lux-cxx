#pragma once
#include "libclang.hpp"
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/runtime/MetaUnitImpl.hpp>
#include <lux/cxx/lan_model/declaration.hpp>
#include <lux/cxx/lan_model/type.hpp>
#include <lux/cxx/lan_model/types/class_type.hpp>
#include <lux/cxx/lan_model/types/enumeration.hpp>
#include <unordered_map>
#include <memory>

namespace lux::cxx::dref
{
    class MetaUnit;

    class CxxParserImpl
    {
    public:
        CxxParserImpl();

        ~CxxParserImpl();

        ParseResult parse(const std::string& file, std::vector<std::string> commands, std::string name, std::string version);

        // static size_t declaration_id(const Cursor& cursor);
        static size_t declaration_id(EDeclarationKind kind, const char* name);
        static size_t declaration_id(Declaration* decl);
        static size_t type_meta_id(const char* name);

        static std::string fullQualifiedName(const Cursor& cursor);
        static std::string fullQualifiedParameterName(const Cursor& cursor, size_t index);

        // the return string should be deleted by user
        static char* nameFromClangString(const String&);
        static char* nameFromStdString(const std::string&);
        static ::lux::cxx::lan_model::Visibility visibiltyFromClangVisibility(CX_CXXAccessSpecifier);

        static char* annotationFromClangCursor(const Cursor&);
    private:
        
        [[nodiscard]] TranslationUnit   translate(const std::string& file, std::vector<std::string> commands);

        std::vector<Cursor>             findMarkedCursors(const Cursor& root_cursor);
        Declaration*                    parseDeclaration(const Cursor& cursor);

        template<EDeclarationKind E, typename T = typename lux::cxx::lan_model::declaration_kind_map<E>::type>
        T* TParseDeclaration(const Cursor&, T*);

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
        T* TParseDeclarationDecorator(const Cursor& cursor, bool is_marked = false)
        {
            auto full_qualified_name = fullQualifiedName(cursor);

            auto id = declaration_id(E, full_qualified_name.c_str());
            if (hasDeclarationInContextById(id))
            {
                return static_cast<T*>(getDeclarationFromContextById(id));
            }

            auto declaration = new T{};
            if (is_marked)
            {
                registMarkableDeclaration(id, declaration);
            }
            else
            {
                registUnmarkableDeclaration(id, declaration);
            }

            auto type_meta = TDeclTypeGen<E>(cursor);

            if (type_meta == nullptr)
            {
                delete declaration;
                // TODO remove from 
                return nullptr;
            }

            declaration->type             = type_meta;
            declaration->declaration_kind = E;
            declaration->name             = nameFromStdString(full_qualified_name);
            
            declaration->attribute = annotationFromClangCursor(cursor);
            if constexpr (E == EDeclarationKind::CLASS)
            {
                static_cast<::lux::cxx::lan_model::ClassType*>(type_meta)
                    ->declaration = declaration;
            }
            else if constexpr (E == EDeclarationKind::ENUMERATION)
            {
                static_cast<::lux::cxx::lan_model::EnumerationType*>(type_meta)
                    ->declaration = declaration;
            }

            TParseDeclaration<E>(cursor, declaration);

            return declaration;
        }

        lux::cxx::lan_model::ParameterDeclaration* TParseParameter(const Cursor& cursor, size_t index)
        {
            auto full_qualified_name = fullQualifiedParameterName(cursor, index);
            auto id = declaration_id(EDeclarationKind::PARAMETER, full_qualified_name.c_str());
            auto declaration = new lux::cxx::lan_model::ParameterDeclaration();
            registUnmarkableDeclaration(id, declaration);

            auto type_meta = TDeclTypeGen<EDeclarationKind::PARAMETER>(cursor);

            if (type_meta == nullptr)
            {
                delete declaration;
                // TODO remove from 
                return nullptr;
            }

            declaration->type = type_meta;
            declaration->declaration_kind = EDeclarationKind::PARAMETER;
            declaration->name = nameFromStdString(full_qualified_name);
            declaration->index = index;

            // TParseDeclaration<EDeclarationKind::PARAMETER>(cursor, declaration); no use

            return declaration;
        }

        template<ETypeMetaKind E, typename T = typename lux::cxx::lan_model::type_kind_map<E>::type>
        T* TParseTypeMeta(const Type&, T*);

        template<ETypeMetaKind E, typename T = typename lux::cxx::lan_model::type_kind_map<E>::type>
        T* TParseTypeMetaDecorator(const Type& type)
        {
            auto id = type_meta_id(type.typeSpelling().c_str());
            if (hasTypeMetaInContextById(id))
            {
                return static_cast<T*>(getTypeMetaFromContextById(id));
            }

            return TParseTypeMetaDecoratorNoCheck<E>(type, id);
        }

        template<ETypeMetaKind E, typename T = typename lux::cxx::lan_model::type_kind_map<E>::type>
        T* TParseTypeMetaDecoratorNoCheck(const Type& type, size_t id)
        {
            T* meta_type = new T();
            registTypeMeta(id, meta_type);

            meta_type->type_kind    = E;
            meta_type->is_const     = type.isConstQualifiedType();
            meta_type->is_volatile  = type.isVolatileQualifiedType();
            meta_type->name         = nameFromClangString(type.typeSpelling());

            TParseTypeMeta<E>(type, meta_type);

            return meta_type;
        }

        TypeMeta* parseUncertainTypeMeta(const Type& type);

        
        bool hasDeclarationInContextById(size_t id);
        bool hasDeclarationInContextByName(EDeclarationKind kind, const char* name);
        Declaration* getDeclarationFromContextById(size_t id);
        Declaration* getDeclarationFromContextByName(EDeclarationKind kind, const char* name);

        bool hasTypeMetaInContextById(size_t id);
        bool hasTypeMetaInContextByName(const char* name);
        TypeMeta* getTypeMetaFromContextById(size_t id);
        TypeMeta* getTypeMetaFromContextByName(EDeclarationKind kind, const char* name);

        void registTypeMeta(size_t, TypeMeta*);
        void registUnmarkableDeclaration(Declaration*);
        void registMarkableDeclaration(Declaration*);

        void registUnmarkableDeclaration(size_t, Declaration*);
        void registMarkableDeclaration(size_t, Declaration*);

        template<typename T>
        std::enable_if_t<std::is_base_of_v<::lux::cxx::lan_model::CallableDeclCommon, T>> 
        callableDeclInit(T* declaration, const Cursor& cursor)
        {
            using namespace ::lux::cxx::lan_model;
            declaration->result_type = parseUncertainTypeMeta(cursor.cursorResultType());
            declaration->parameter_number = cursor.numArguments();
            declaration->parameter_decls = declaration->parameter_number > 0 ?
                new ParameterDeclaration* [declaration->parameter_number] : nullptr;
            for (size_t i = 0; i < declaration->parameter_number; i++)
            {
                auto param_decl = TParseParameter(cursor.getArgument(i), i);
                declaration->parameter_decls[i] = param_decl;
            }
        }

        static Type rootAnonicalType(const Type& type);

        template<ETypeMetaKind E1, ETypeMetaKind E2>
        TypeMeta* parseReferenceSemtantic(const Type& type, size_t id)
        {
            auto ref_type = rootAnonicalType(type.getPointeeType());
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
