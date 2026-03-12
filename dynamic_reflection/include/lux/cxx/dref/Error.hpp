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
#include <lux/cxx/compile_time/expected.hpp>
#include <string>

namespace lux::cxx::dref
{
    /// Error codes for all dynamic-reflection generator and parser operations.
    enum class EErrorCode
    {
        // Configuration / file system
        ConfigFileNotFound,
        ConfigFileParseError,
        TargetFileNotFound,
        SourceFileNotFound,
        CompileCommandsNotFound,
        CompileCommandsParseError,
        TemplateFileNotFound,
        OutputDirCreateFailed,
        OutputFileOpenFailed,
        OutputFileWriteFailed,
        // Parsing
        TranslationUnitFailed,
        UnsupportedDeclarationKind,
        // Template / code-gen look-ups
        DeclarationNotFound,
        TypeNotFound,
        DeclarationTypeMismatch,
        // Data integrity
        CustomFieldParseError,
        TemplateRenderFailed,
    };

    /// Unified error value carrying a machine-readable code and a human-readable message.
    struct DRefError
    {
        EErrorCode  code;
        std::string message;
    };

    /// Alias used throughout the dynamic-reflection subsystem.
    template<typename T>
    using Result = lux::cxx::expected<T, DRefError>;

    /// Convenience factory for building an unexpected<DRefError>.
    inline lux::cxx::unexpected<DRefError> make_error(EErrorCode code, std::string msg)
    {
        return lux::cxx::unexpected<DRefError>{DRefError{code, std::move(msg)}};
    }

} // namespace lux::cxx::dref
