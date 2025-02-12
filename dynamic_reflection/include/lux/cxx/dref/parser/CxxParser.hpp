#pragma once
#include <memory>
#include <optional>
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
        /**
         * @brief Construct a new CxxParser object
         * 
        */
        LUX_CXX_PUBLIC CxxParser();

        /**
		 * @brief Destroy the CxxParser object
		 * 
		 */
        LUX_CXX_PUBLIC ~CxxParser();

        /**
		 * @brief Parse a file and return the result
		 * 
		 * @param file The file to parse
		 * @param commands The commands to pass to the parser
		 * @param name The name of the library
		 * @param version The version of the library
		 * @return ParseResult The result of the parsing
		 */
        [[nodiscard]] LUX_CXX_PUBLIC ParseResult 
        parse(std::string_view file, std::vector<std::string_view> commands, std::string_view name, std::string_view version);

        /*
         * @brief Set the PCH file to use
         */
        LUX_CXX_PUBLIC void setPCHFile(const std::string& file);

    private:

        std::unique_ptr<CxxParserImpl> _impl;
    };
} // namespace lux::reflection
