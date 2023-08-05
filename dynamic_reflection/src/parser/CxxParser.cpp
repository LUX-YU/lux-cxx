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

    ParseResult CxxParser::parse(const std::string& file, std::vector<std::string> commands, std::string name, std::string version)
    {
        return _impl->parse(file, std::move(commands), std::move(name), std::move(version));
    }
} // namespace lux::engine::core
