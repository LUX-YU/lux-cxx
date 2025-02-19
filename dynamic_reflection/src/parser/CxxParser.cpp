#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/CxxParserImpl.hpp>

namespace lux::cxx::dref
{
    CxxParser::CxxParser()
    {
        _impl = std::make_unique<CxxParserImpl>();
    }

    CxxParser::~CxxParser() = default;

    ParseResult CxxParser::parse(const std::string_view file, std::vector<std::string> commands, std::string_view name, std::string_view version) const
    {
        return _impl->parse(file, std::move(commands), name, version);
    }

    void CxxParser::setPCHFile(const std::string& file)
    {

    }
} // namespace lux::engine::core
