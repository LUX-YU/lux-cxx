// xml_test — round-trip the serialization core through the XML backend.
// This TU includes ONLY <lux/cxx/serialization/xml.hpp> — no tinyxml2.

#include "xml_types.hpp"
#include "xml_types.serialize.hpp"          // generated meta_info<Window/Settings>

#include <lux/cxx/serialization/xml.hpp>

#include <iostream>
#include <string>

using namespace lux::cxx::ser;

namespace
{
    int g_failures = 0;
    void check(bool cond, const char* desc)
    {
        if (cond) { std::cout << "[PASS] " << desc << "\n"; }
        else      { std::cerr << "[FAIL] " << desc << "\n"; ++g_failures; }
    }
    bool has(const std::string& hay, std::string_view needle) { return hay.find(needle) != std::string::npos; }
}

int main()
{
    Settings s;
    s.id         = 7;
    s.name       = "demo";
    s.mode       = Mode::Auto;
    s.fullscreen = true;
    s.recent     = { 10, 20, 30 };
    s.profile    = "default";
    s.window     = { "Main", 800, 600 };
    s.limits     = { { "cpu", 4 }, { "mem", 16 } };
    s.blob       = { std::byte{1}, std::byte{2}, std::byte{0xFF} };

    // ---- serialize ----
    const std::string x = to_xml(s, "settings");
    std::cout << x << "\n";
    check(has(x, "id=\"7\""),                                "xml_attribute -> attribute");
    check(has(x, "<blob>AQL/</blob>"),                      "bytes -> base64 element");
    check(has(x, "<fullscreen>true</fullscreen>"),          "bool -> text");
    check(has(x, "<mode>Auto</mode>"),                     "enum -> name text");
    check(has(x, "<window>") && has(x, "<width>800</width>"), "nested element");
    check(has(x, "<recent>") && has(x, "<item>10</item>"),  "sequence -> <item> elements");
    check(has(x, "<limits>") && has(x, "<cpu>4</cpu>") && has(x, "<mem>16</mem>"), "string-keyed map -> named elements");
    check(has(x, "<profile>default</profile>"),             "optional engaged -> element");

    // ---- deserialize (round-trip) ----
    const auto r = from_xml<Settings>(x);
    check(r.has_value(), "from_xml succeeded");
    if (r)
    {
        const Settings& q = r.value();
        check(q.id == 7,                                     "xml_attribute round-trip");
        check(q.blob == s.blob,                              "base64 bytes round-trip");
        check(q.name == "demo",                              "name round-trip");
        check(q.mode == Mode::Auto,                          "enum round-trip");
        check(q.fullscreen == true,                          "bool round-trip");
        check(q.recent == s.recent,                          "vector round-trip");
        check(q.profile && *q.profile == "default",          "optional round-trip");
        check(q.window.title == "Main" && q.window.width == 800 && q.window.height == 600, "nested round-trip");
        check(q.limits.size() == 2 && q.limits.at("cpu") == 4 && q.limits.at("mem") == 16, "map round-trip");
    }

    // ---- disengaged optional -> omitted element ----
    Settings s2 = s;
    s2.profile = std::nullopt;
    s2.recent.clear();
    const std::string x2 = to_xml(s2, "settings");
    check(!has(x2, "<profile>"), "disengaged optional -> element omitted");
    const auto r2 = from_xml<Settings>(x2);
    check(r2 && !r2.value().profile,        "optional null round-trip");
    check(r2 && r2.value().recent.empty(),  "empty vector round-trip");

    // ---- malformed XML -> parse_error ----
    const auto r3 = from_xml<Settings>("<settings><unclosed>");
    check(!r3.has_value() && r3.error().code == error_code::parse_error, "malformed XML -> parse_error");

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
