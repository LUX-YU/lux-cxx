#pragma once
#include <memory>
#include <lux/cxx/visibility.h>

#include "ParserResult.hpp"

namespace lux::cxx::dref
{
    class CxxParserImpl;

    class CxxParser
    {
    public:
        LUX_CXX_PUBLIC CxxParser();

        LUX_CXX_PUBLIC ~CxxParser();

        [[nodiscard]] LUX_CXX_PUBLIC ParserResult parse(const std::string& file, std::vector<std::string> commands);

        [[maybe_nouse]] LUX_CXX_PUBLIC void setPCHFile(const std::string& file);

        // for debug use
        LUX_CXX_PUBLIC void parseAll(const std::string& file, std::vector<std::string> commands);

    private:

        std::unique_ptr<CxxParserImpl> _impl;
    };
} // namespace lux::reflection
