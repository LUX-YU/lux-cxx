/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once
#include <memory>
#include <optional>
#include <vector>
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
        parse(std::string_view file, std::vector<std::string> commands, std::string_view name, std::string_view version) const;

        /*
         * @brief Set the PCH file to use
         */
        LUX_CXX_PUBLIC void setPCHFile(const std::string& file);

    private:

        std::unique_ptr<CxxParserImpl> _impl;
    };
} // namespace lux::reflection
