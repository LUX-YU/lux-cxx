#pragma once
#include <unordered_map>
#include <string_view>

namespace lux::cxx
{
    template<typename ... Bases>
    struct HeterogeneousLookupBase : Bases ...
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

    using transparent_string_hash = HeterogeneousLookupBase<
        std::hash<std::string>,
        std::hash<std::string_view>,
        char_pointer_hash
    >;

    template<typename T> using heterogeneous_map =
    std::unordered_map <
        std::string,
        T,
        transparent_string_hash,
        std::equal_to<>
    >;
}