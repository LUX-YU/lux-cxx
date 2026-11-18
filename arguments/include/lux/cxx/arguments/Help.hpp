#pragma once
// Cold-path diagnostics. Intentionally separate from the parsing hot path.

#include <lux/cxx/arguments/Option.hpp>
#include <array>
#include <string>
#include <string_view>

namespace lux::cxx::args
{

    template<class... Options>
    [[nodiscard]] std::string usage(std::string_view program, const Options&... options)
    {
        std::string result;
        result.reserve(96 + sizeof...(Options) * 48);
        result.append("Usage: ").append(program).append(" [options]\n\nOptions:\n");
        (([&] {
             result.append("  ");
             if (!options.shortName().empty())
             {
                 result.push_back('-');
                 result.append(options.shortName());
                 result.append(", ");
             }
             result.append("--").append(options.name());
             if constexpr (Options::is_flag)
                 result.append(" [=bool]");
             else if constexpr (Options::element_mode)
                 result.append(" <value>...");
             else
                 result.append(" <value>");
             if (options.isRequired())
                 result.append(" (required)");
             if (!options.description().empty())
             {
                 result.append("\n      ");
                 result.append(options.description());
             }
             result.push_back('\n');
         }()),
         ...);
        result.append("  -h, --help\n");
        return result;
    }

    template<class... Options>
    [[nodiscard]] std::string describe(Error error, const Options&... options)
    {
        std::string text{message(error.code)};
        if (error.option != no_option && error.option < sizeof...(Options))
        {
            const std::array<std::string_view, sizeof...(Options)> names{options.name()...};
            text.append(" (--").append(names[error.option]).push_back(')');
        }
        if (error.position != no_position)
        {
            text.append(error.location == ELocationKind::ARGUMENT_INDEX ? " at argv[" : " at offset ");
            text.append(std::to_string(error.position));
            if (error.location == ELocationKind::ARGUMENT_INDEX)
                text.push_back(']');
        }
        if (error.value_offset != no_position)
        {
            text.append(" (decoded value offset ")
                .append(std::to_string(error.value_offset))
                .push_back(')');
        }
        return text;
    }

} // namespace lux::cxx::args
