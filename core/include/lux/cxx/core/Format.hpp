#pragma once

// Prefer the standard C++20 implementation, then header-only libfmt, and keep
// the pre-existing stream fallback for toolchains that provide neither.
#if __has_include(<version>)
#  include <version>
#endif

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#  include <format>

namespace lux
{
    using std::format;
    using std::format_string;
    using std::format_to;
    using std::make_format_args;
    using std::vformat;
}
#elif __has_include(<fmt/format.h>)
#  ifndef FMT_HEADER_ONLY
#    define FMT_HEADER_ONLY
#  endif
#  include <fmt/args.h>
#  include <fmt/format.h>

namespace lux
{
    using fmt::format;
    using fmt::format_to;
    using fmt::make_format_args;
    using fmt::vformat;

    template<typename... Args>
    using format_string = fmt::format_string<Args...>;
}
#else
#  include <sstream>
#  include <string>

namespace lux
{
    template<typename... Args>
    std::string format(const std::string& format_text, Args&&... args)
    {
        std::ostringstream stream;
        stream << format_text;
        ((stream << ' ' << args), ...);
        return stream.str();
    }

    template<typename... Args>
    using format_string = std::string;
}
#endif
