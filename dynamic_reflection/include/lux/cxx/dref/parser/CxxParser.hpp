#pragma once
#include <memory>
#include <optional>
#include <string_view>
#include <lux/cxx/visibility.h>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

namespace lux::cxx::dref
{
    class CxxParserImpl;

    enum class EParseResult
    {
        SUCCESS,
        UNKNOWN_TYPE,
        FAILED
    };

    using ParseResult = std::pair<EParseResult, MetaUnit>;

    class CxxParser
    {
    public:
        LUX_CXX_PUBLIC CxxParser();

        LUX_CXX_PUBLIC ~CxxParser();

        [[nodiscard]] LUX_CXX_PUBLIC ParseResult 
        parse(std::string_view file, std::vector<std::string_view> commands, std::string_view name, std::string_view version);

        LUX_CXX_PUBLIC void setPCHFile(const std::string& file);

    private:

        std::unique_ptr<CxxParserImpl> _impl;
    };
} // namespace lux::reflection
