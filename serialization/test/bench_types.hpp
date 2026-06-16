#pragma once
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include <cstdint>
#include <string>
#include <vector>

// Small "message" — a typical RPC call / log event.
struct LUX_META(serializable) SmallMsg
{
    std::int64_t id;
    std::string  method;
    std::string  user;
    bool         ok;
    double       latency_ms;
    std::int32_t code;
};

// Medium nested config.
struct LUX_META(serializable) Endpoint
{
    std::string  host;
    std::int32_t port;
    bool         tls;
};

struct LUX_META(serializable) MediumCfg
{
    std::string              name;
    std::int32_t             workers;
    Endpoint                 primary;
    Endpoint                 replica;
    std::vector<std::string> tags;
};

// Large array-heavy payload — bulk records.
struct LUX_META(serializable) Record
{
    std::int64_t ts;
    std::string  key;
    double       value;
};

struct LUX_META(serializable) Batch
{
    std::string         source;
    std::vector<Record> records;
};
