#include <lux/cxx/core/Format.hpp>

#include <cassert>
#include <iterator>
#include <string>
#include <utility>

namespace
{
    template<typename... Args>
    std::string checkedFormat(
        lux::format_string<Args...> format_text,
        Args&&... args)
    {
        return lux::format(format_text, std::forward<Args>(args)...);
    }
}

int main()
{
    assert(checkedFormat("{} {}", 42, "scene") == "42 scene");

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
    int value = 7;
    const auto arguments = lux::make_format_args(value);
    assert(lux::vformat("value={}", arguments) == "value=7");

    std::string output;
    lux::format_to(std::back_inserter(output), "{}", 9);
    assert(output == "9");
#elif __has_include(<fmt/format.h>)
    int value = 7;
    const auto arguments = lux::make_format_args(value);
    assert(lux::vformat("value={}", arguments) == "value=7");

    std::string output;
    lux::format_to(std::back_inserter(output), "{}", 9);
    assert(output == "9");
#endif
}
