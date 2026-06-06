#pragma once
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include <optional>
#include <string>
#include <vector>

enum class LUX_META(serializable) LogLevel { Error, Warn, Info, Debug };

struct LUX_META(serializable) DbConfig
{
    std::string host;
    int         port;
};

struct LUX_META(serializable) AppConfig
{
    std::string              name;
    int                      LUX_META(serializable, short = "j") threads;
    bool                     verbose;
    LogLevel                 level;          // enum -> underlying integer
    std::vector<std::string> inputs;         // multi-value
    std::optional<int>       limit;          // present only if provided
    DbConfig                 db;             // nested -> --db.host / --db.port
};
