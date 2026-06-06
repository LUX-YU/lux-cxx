// arguments_test — bind a reflected struct from CLI options via the Arguments backend.

#include "args_types.hpp"
#include "args_types.serialize.hpp"          // generated meta_info<DbConfig/AppConfig>

#include <lux/cxx/serialization/arguments.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace lux::cxx::ser;

namespace
{
    int g_failures = 0;
    void check(bool cond, const char* desc)
    {
        if (cond) { std::cout << "[PASS] " << desc << "\n"; }
        else      { std::cerr << "[FAIL] " << desc << "\n"; ++g_failures; }
    }
}

int main()
{
    // ---- full command line, incl. nested (--db.*), enum, flag, multi, optional ----
    const auto r = from_args<AppConfig>(
        std::string("--name srv --threads 8 --verbose --level Info --limit 100 "
                    "--db.host localhost --db.port 5432 --inputs a b"),
        "app");
    check(r.has_value(), "from_args succeeded");
    if (r)
    {
        const AppConfig& c = r.value();
        check(c.name == "srv",                                  "string field");
        check(c.threads == 8,                                   "int field");
        check(c.verbose == true,                                "bool flag");
        check(c.level == LogLevel::Info,                        "enum field (2 -> Info)");
        check((c.inputs == std::vector<std::string>{"a", "b"}), "multi-value sequence");
        check(c.limit && *c.limit == 100,                       "optional present");
        check(c.db.host == "localhost" && c.db.port == 5432,    "nested struct (--db.*)");
    }

    // ---- defaults: omit most; struct/parser defaults apply ----
    const auto r2 = from_args<AppConfig>(std::string("--name only"), "app");
    check(r2.has_value(), "from_args (defaults) succeeded");
    if (r2)
    {
        const AppConfig& c = r2.value();
        check(c.name == "only",       "provided field");
        check(c.threads == 0,         "default int");
        check(c.verbose == false,     "default bool");
        check(!c.limit.has_value(),   "absent optional stays empty");
        check(c.inputs.empty(),       "absent sequence stays empty");
        check(c.db.port == 0,         "default nested");
    }

    // ---- short option (-j) for threads ----
    const auto r3 = from_args<AppConfig>(std::string("--name s -j 16"), "app");
    check(r3 && r3.value().threads == 16, "short option -j");

    // ---- usage text reflects nested + short options ----
    const std::string u = usage<AppConfig>("app");
    std::cout << u << "\n";
    check(u.find("--db.port") != std::string::npos,   "usage lists nested option");
    check(u.find("--threads") != std::string::npos,   "usage lists field");
    check(u.find("-j") != std::string::npos,          "usage shows short option");

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
