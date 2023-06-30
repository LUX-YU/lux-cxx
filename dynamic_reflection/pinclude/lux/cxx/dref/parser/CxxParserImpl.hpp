#pragma once
#include "libclang.hpp"
#include <unordered_map>
#include <lux/cxx/dref/runtime/Class.hpp>
#include <lux/cxx/dref/runtime/Method.hpp>
#include <lux/cxx/dref/runtime/Enumeration.hpp>
#include <memory>
#include <stack>

namespace lux::cxx::dref
{
    class ParserResultImpl;

    namespace runtime
    {
        struct TypeMeta;
    }

    class CxxParserImpl
    {
    public:
        CxxParserImpl();

        ~CxxParserImpl();

        [[nodiscard]] TranslationUnit translate(const std::string& file, std::vector<std::string> commands);

        std::unique_ptr<ParserResultImpl> extractCursor(Cursor& cursor);

    private:
        std::vector<Cursor> findMarkedCursors(Cursor& root_cursor);

        runtime::TypeMeta* cursorTypeDispatch(Cursor& cursor, ParserResultImpl* );

        // shared context for creating translation units
        std::string                                 _pch_file;

        CXIndex                                     _clang_index;
    };
} // namespace lux::reflection
