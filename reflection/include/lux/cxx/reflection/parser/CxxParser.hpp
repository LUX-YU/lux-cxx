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
#include <string_view>
#include <functional>
#include <lux/cxx/visibility.h>
#include <lux/cxx/reflection/runtime/MetaUnit.hpp>
#include <lux/cxx/reflection/runtime/MetaIr.hpp>

namespace lux::cxx::reflection
{
    class CxxParserImpl;

    enum class EParseResult
    {
        /// Parsing completed without errors; MetaUnit is populated.
        SUCCESS,
        /// One or more marked declarations have unsupported cursor kinds.
        /// The successfully parsed declarations are still available in MetaUnit.
        UNKNOWN_TYPE,
        /// Translation unit could not be created, or a hard libclang error occurred.
        FAILED
    };

    using ParseResult = std::pair<EParseResult, ir::MetaUnit>;
    using LegacyParseResult = std::pair<EParseResult, MetaUnit>;

	struct ParseOptions
	{
        // The name of the library
		std::string              name;
        // version The version of the library
		std::string              version;
		std::string              marker_symbol;
		// Symbol used to exclude individual members from reflection.
		// Members annotated with LUX_META(<exclude_symbol>) will be skipped.
		std::string              exclude_symbol = "no_reflect";
		// Opt-in: also parse marked declarations that live in headers INCLUDED
		// by the target file (default keeps the main-file-only guard). Lets a
		// generator pull in a struct defined next to its own consumers (e.g. a
		// pass-params scalars struct shared with the editor header) without
		// forcing both into one file. Leave false for registration-style
		// templates — they would otherwise re-register every marked type the
		// TU can see, once per including TU.
		bool                     parse_included_marked = false;
        // commands The commands to pass to the parser
		std::vector<std::string> commands;
		std::string              pch_file;
		// Called while the translation unit is alive; the view is borrowed only for this call.
		std::function<void(std::string_view)> on_included_file;
	};

    class LUX_CXX_PUBLIC CxxParser
    {
    public:
        /**
         * @brief Construct a new CxxParser object
         * 
        */
        CxxParser(ParseOptions option);

        /**
		 * @brief Destroy the CxxParser object
		 * 
		 */
        ~CxxParser();

        /**
		 * @brief Parse a file and return the result
		 * @param file The file to parse
         */
        [[nodiscard]] ParseResult parse(std::string_view file) const;

        /// Transitional test/debug face. Production generator transport uses
        /// compact MetaIr through parse().
        [[nodiscard]] LegacyParseResult parseLegacy(std::string_view file) const;

		/**
		 * @brief Set the callback for parse error
         */
        void setOnParseError(std::function<void(const std::string&)> callback);

    private:
        std::unique_ptr<CxxParserImpl> _impl;
    };
} // namespace lux::reflection
