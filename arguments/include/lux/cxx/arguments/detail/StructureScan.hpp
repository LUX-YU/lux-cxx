#pragma once
// Lexical boundary finding only. Never builds a token vector or an AST.
// Shared by structured token sources and custom structured conversion leaves.
#include <lux/cxx/arguments/Value.hpp>
#include <array>

namespace lux::cxx::args::detail
{
    inline constexpr std::size_t structure_depth_limit = 64;

    // On success, pos is one past the closing bracket. Input starts at '[' or '{'.
    inline ValueError skipStructure(
        std::string_view input, std::size_t& pos, std::size_t limit = structure_depth_limit
    ) noexcept
    {
        std::array<char, structure_depth_limit> stack;
        std::size_t depth = 0;
        char quote = 0;
        for (; pos < input.size(); ++pos)
        {
            const char c = input[pos];
            if (quote != 0)
            {
                if (c == '\\')
                {
                    if (++pos == input.size())
                        return {EErrorCode::INVALID_ESCAPE, pos - 1};
                }
                else if (c == quote)
                    quote = 0;
                continue;
            }
            if (c == '"' || c == '\'')
            {
                quote = c;
                continue;
            }
            if (c == '[' || c == '{')
            {
                if (depth == limit || depth == stack.size())
                    return {EErrorCode::DEPTH_LIMIT, pos};
                stack[depth++] = c == '[' ? ']' : '}';
            }
            else if (c == ']' || c == '}')
            {
                if (depth == 0 || stack[depth - 1] != c)
                    return {EErrorCode::INVALID_STRUCTURE, pos};
                if (--depth == 0)
                {
                    ++pos;
                    return {};
                }
            }
        }
        return {quote ? EErrorCode::UNCLOSED_QUOTE : EErrorCode::INVALID_STRUCTURE, pos};
    }
} // namespace lux::cxx::args::detail
