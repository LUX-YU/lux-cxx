#pragma once
#include <unordered_map>

namespace lux::cxx
{
    template<typename ... Bases>
    struct HeterogenousLookupBase : Bases ...
    {
        using is_transparent = void;
        using Bases::operator() ...;
    };

    struct char_pointer_hash
    {
        auto operator()(const char* ptr) const noexcept
        {
            return std::hash<std::string_view>{}(ptr);
        }
    };

    using transparent_string_hash = HeterogenousLookupBase<
        std::hash<std::string>,
        std::hash<std::string_view>,
        char_pointer_hash
    >;

    template<typename T> using heterogenouts_map = 
    std::unordered_map <
        std::string,
        T,
        transparent_string_hash,
        std::equal_to<>
    >;
}