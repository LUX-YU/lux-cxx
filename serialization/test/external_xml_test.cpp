// external_xml_test — XML round-trip of a NON-INTRUSIVELY registered external
// type (extlib::Widget / extlib::Status). The meta_info / enum_meta come from the
// generated ext_xml_register.serialize.hpp, produced by reflecting the foreign
// type through a LUX_REFLECT_EXTERNAL proxy — no annotation on the type itself.

#include "ext_serialize_types.hpp"
#include "ext_xml_register.serialize.hpp"    // generated meta_info<Widget> + enum_meta<Status>

#include <lux/cxx/serialization/xml.hpp>

#include <iostream>
#include <string>
#include <string_view>

using namespace lux::cxx::ser;
using namespace extlib;

namespace
{
    int g_failures = 0;
    void check(bool cond, const char* desc)
    {
        if (cond) { std::cout << "[PASS] " << desc << "\n"; }
        else      { std::cerr << "[FAIL] " << desc << "\n"; ++g_failures; }
    }
    bool has(const std::string& hay, std::string_view needle)
    {
        return hay.find(needle) != std::string::npos;
    }
}

int main()
{
    static_assert(meta_info<Widget>::reflected, "Widget reflected non-intrusively");
    static_assert(enum_meta<Status>::reflected, "Status reflected non-intrusively");

    Widget w;
    w.name    = "gizmo";
    w.count   = 42;
    w.enabled = true;
    w.status  = Status::Running;
    w.values  = { 1, 2, 3 };

    const std::string x = to_xml(w, "widget");
    std::cout << x << "\n";
    check(has(x, "<name>gizmo</name>"),       "string field serialized");
    check(has(x, "<count>42</count>"),        "int field serialized");
    check(has(x, "<enabled>true</enabled>"),  "bool field serialized");
    check(has(x, "<status>Running</status>"), "external enum -> name text");
    check(has(x, "<values>") && has(x, "<item>1</item>"), "sequence -> <item> elements");

    const auto r = from_xml<Widget>(x);
    check(r.has_value(), "from_xml succeeded");
    if (r)
    {
        const Widget& q = r.value();
        check(q.name    == w.name,          "name round-trip");
        check(q.count   == w.count,         "count round-trip");
        check(q.enabled == w.enabled,       "enabled round-trip");
        check(q.status  == Status::Running, "external enum round-trip");
        check(q.values  == w.values,        "vector round-trip");
    }

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
