// serialization_test
//
// Validates the generated lux::cxx::ser::meta_info<T> specialisations:
//   - compile-time: reflected flag, type alias, field_count_v
//   - runtime: field names (incl. ser name override), options (required/skip/short),
//     and member-pointer value access (read + write).

#include "test_serialize.hpp"
#include "test_serialize.serialize.hpp"   // generated

#include <iostream>
#include <string_view>
#include <type_traits>

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

// ---------- compile-time contract ----------
static_assert(meta_info<Point>::reflected,                       "Point is reflected");
static_assert(std::is_same_v<meta_info<Point>::type, Point>,     "meta_info<Point>::type");
static_assert(field_count_v<Point> == 2,                         "Point has 2 fields");
static_assert(field_count_v<Person> == 4,                        "Person has 4 public fields");
static_assert(meta_info<app::config::Server>::reflected,         "namespaced type reflected");
static_assert(field_count_v<app::config::Server> == 2,           "Server has 2 fields");
static_assert(meta_info<int>::reflected == false,               "unreflected fallback");

int main()
{
    check(meta_info<Person>::name == std::string_view{"Person"},        "Person::name");
    check(meta_info<app::config::Server>::qualified_name
              == std::string_view{"app::config::Server"},               "Server qualified_name");

    // Field metadata of Person.
    bool has_full_name = false, age_required = false, age_short_a = false, cached_skipped = false;
    meta_info<Person>::for_each_field([&](auto fd)
    {
        if (fd.name == std::string_view{"full_name"})                         has_full_name  = true;
        if (fd.name == std::string_view{"age"} && fd.options.required)        age_required   = true;
        if (fd.name == std::string_view{"age"} && fd.options.short_name == std::string_view{"a"}) age_short_a = true;
        if (fd.name == std::string_view{"cached"} && fd.options.skip)         cached_skipped = true;
    });
    check(has_full_name,   "ser name override -> full_name");
    check(age_required,    "ser required flag");
    check(age_short_a,     "ser short option");
    check(cached_skipped,  "ser skip flag");

    // Member-pointer value access: write through the descriptor, read back.
    Point p{};
    meta_info<Point>::for_each_field([&](auto fd) { p.*(fd.pointer) = 7; });
    check(p.x == 7 && p.y == 7, "member-pointer write via descriptor");

    Person person{};
    person.name = "Ada"; person.age = 36;
    std::string_view read_name;
    int read_age = 0;
    meta_info<Person>::for_each_field([&](auto fd)
    {
        using M = typename decltype(fd)::member_type;
        if constexpr (std::is_same_v<M, std::string>)
            { if (fd.name == std::string_view{"full_name"}) read_name = person.*(fd.pointer); }
        else if constexpr (std::is_same_v<M, int>)
            { if (fd.name == std::string_view{"age"}) read_age = person.*(fd.pointer); }
    });
    check(read_name == std::string_view{"Ada"}, "read std::string field via descriptor");
    check(read_age == 36,                        "read int field via descriptor");

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
