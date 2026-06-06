#pragma once
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

enum class LUX_META(serializable) Color { Red, Green, Blue };

struct LUX_META(serializable) Address
{
    std::string city;
    int         zip;
};

// Exercises structured load-error reporting (required field + nested path).
struct LUX_META(serializable) Required
{
    std::string LUX_META(serializable, required) id;
    int         value;
};

// Exercises: scalar / enum / sequence / optional / nested reflected / string-keyed map.
struct LUX_META(serializable) Profile
{
    std::string                name;
    int                        age;
    Color                      favorite;        // enum -> integer
    std::vector<std::string>   tags;            // sequence -> array
    std::optional<int>         nickname_len;    // optional -> value | null
    Address                    address;         // nested reflected -> object
    std::map<std::string, int> scores;          // string-keyed map -> object
    std::vector<std::byte>     blob;            // bytes -> base64 string
};
