#pragma once
#include <lux/cxx/reflection/runtime/Marker.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

enum class LUX_META(serializable) EYamlColor
{
    RED,
    GREEN,
    BLUE,
};

struct LUX_META(serializable) YamlAddress
{
    std::string city;
    int         zip;
};

struct LUX_META(serializable) YamlRequired
{
    std::string LUX_META(serializable, required) id;
    int value;
};

struct LUX_META(serializable) YamlOptions
{
    std::string LUX_META(serializable, name = "display_name") name;
    int LUX_META(serializable, xml_attribute) yaml_scalar = 17;
    std::vector<int> LUX_META(serializable, omit_empty) omitted_when_empty;
    int LUX_META(serializable, skip) runtime_only = 99;
};

struct YamlCustomPoint
{
    int x = 0;
    int y = 0;
};

struct LUX_META(serializable) YamlProfile
{
    std::string                       name;
    std::string                       ambiguous;
    int                               age = 0;
    double                            ratio = 0.0;
    bool                              enabled = false;
    EYamlColor                        favorite = EYamlColor::RED;
    std::vector<std::string>          tags;
    std::optional<int>                optional_value;
    std::unique_ptr<int>              unique_value;
    std::shared_ptr<std::string>      shared_value;
    YamlAddress                       address;
    std::tuple<int, std::string>      tuple_value;
    std::map<std::string, int>        scores;
    std::map<int, std::string>        numbered;
    std::vector<std::byte>            blob;
    YamlCustomPoint                   custom;
    YamlOptions                       options;
};
