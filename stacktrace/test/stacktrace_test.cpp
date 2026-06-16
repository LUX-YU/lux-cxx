// Smoke test for lux::cxx::stacktrace (std::stacktrace on C++23, cpptrace-backed
// otherwise). Verifies the shim captures a non-empty, symbolized stack.
#include <lux/cxx/stacktrace/stacktrace.hpp>

#include <iostream>
#include <string>

namespace
{
    int g_fail = 0;
    void check(bool cond, const char* desc)
    {
        if (cond) { std::cout << "[PASS] " << desc << "\n"; }
        else      { std::cerr << "[FAIL] " << desc << "\n"; ++g_fail; }
    }
}

// Nested calls so the captured trace has real depth.
static lux::cxx::stacktrace level3() { return lux::cxx::stacktrace::current(); }
static lux::cxx::stacktrace level2() { return level3(); }
static lux::cxx::stacktrace level1() { return level2(); }

int main()
{
#if LUX_HAS_STD_STACKTRACE
    std::cout << "[info] backend: std::stacktrace (C++23)\n";
#else
    std::cout << "[info] backend: cpptrace shim (pre-C++23)\n";
#endif

    const lux::cxx::stacktrace st = level1();
    std::cout << "captured " << st.size() << " frames:\n" << lux::cxx::to_string(st) << "\n";

    check(!st.empty(),       "stacktrace is non-empty");
    check(st.size() >= 3,    "captured at least the nested call frames");

    bool any_symbol = false, any_file = false;
    for (const auto& f : st)
    {
        if (!f.description().empty()) any_symbol = true;
        if (!f.source_file().empty()) any_file  = true;
    }
    check(any_symbol, "at least one frame carries a symbol/description");
    // Source file/line needs debug info — report but don't hard-require it.
    std::cout << (any_file ? "[info] source locations resolved\n"
                           : "[info] no source locations (stripped / no debug info)\n");

    check(!lux::cxx::to_string(st[0]).empty(), "entry to_string is non-empty");

    std::cout << "\n=== Results ===\nFailures: " << g_fail << "\n";
    return g_fail;
}
