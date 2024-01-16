#include <clang-c/Index.h>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/CxxParserImpl.hpp>

namespace lux::cxx::dref
{
    CxxParser::CxxParser()
    {
        _impl = std::make_unique<CxxParserImpl>();
    }

    CxxParser::~CxxParser() = default;

    ParseResult CxxParser::parse(std::string_view file, std::vector<std::string_view> commands, std::string_view name, std::string_view version)
    {
        return _impl->parse(file, std::move(commands), std::move(name), std::move(version));
    }

    void CxxParser::setPCHFile(const std::string& file)
    {

    }
} // namespace lux::engine::core
