#include <clang-c/Index.h>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/parser/ParserResultImpl.hpp>

#include <lux/cxx/dref/parser/Attribute.hpp>

#include <iostream>
#include <queue>

namespace lux::cxx::dref
{
    CxxParser::CxxParser()
    {
        _impl = std::make_unique<CxxParserImpl>();
    }

    CxxParser::~CxxParser() = default;

    ParserResult CxxParser::parse(const std::string& file, std::vector<std::string> commands)
    {
        ParserResult result;
        auto translate_unit = _impl->translate(file, std::move(commands));

        Cursor cursor(translate_unit);

        result._impl = _impl->extractCursor(cursor);

        return result;
    }

    void CxxParser::parseAll(const std::string& file, std::vector<std::string> commands)
    {
        auto translate_unit = _impl->translate(file, std::move(commands));
        Cursor cursor(translate_unit);

        cursor.visitChildren(
            [this](Cursor cursor, Cursor parent_cursor) -> CXChildVisitResult
            {
                std::cout << "clang_getCursorSpelling: " << cursor.cursorSpelling() << std::endl;



                return CXChildVisitResult::CXChildVisit_Recurse;
            }
        );
    }
} // namespace lux::engine::core
