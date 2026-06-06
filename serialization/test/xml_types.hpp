#pragma once
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

enum class LUX_META(serializable) Mode { Off, On, Auto };

struct LUX_META(serializable) Window
{
    std::string title;
    int         width;
    int         height;
};

struct LUX_META(serializable) Settings
{
    int                        LUX_META(serializable, xml_attribute) id;  // -> <settings id="..">
    std::string                name;
    Mode                       mode;          // enum -> name text
    bool                       fullscreen;    // bool -> "true"/"false"
    std::vector<int>           recent;        // sequence -> <item> elements
    std::optional<std::string> profile;       // optional -> element present | omitted
    Window                     window;        // nested reflected -> child element
    std::map<std::string, int> limits;        // string-keyed map -> named child elements
    std::vector<std::byte>     blob;          // bytes -> base64 text
};
