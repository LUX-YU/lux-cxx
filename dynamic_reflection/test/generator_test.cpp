// generator_test
//
// Two layers of validation:
//   1. Compile-time: the generated headers contain static_asserts (see
//      annotation_callbacks_test.template.inja); a compile error here is a
//      generator regression.
//   2. Runtime: the assertions in main() exercise the generated
//      type_meta<T> specialisations and confirm their reported size/align/
//      qualified-name agree with what the C++ language model says about
//      the original types.
//
// If the generator stops emitting a type_meta<TestStruct> specialisation,
// this file fails to compile.

#include "test_header.hpp"

#include <test_headermeta.hpp>
#include <test_annotation_callbacksanno.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <tuple>
#include <type_traits>

using namespace lux::cxx::dref;

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
    // ---------- type_meta<TestStruct> ----------
    using TS = type_meta<TestStruct>;
    static_assert(std::is_same_v<TS::type, TestStruct>,
                  "type_meta::type must be TestStruct");
    static_assert(TS::size  == sizeof(TestStruct),
                  "TestStruct size matches sizeof");
    static_assert(TS::align == alignof(TestStruct),
                  "TestStruct align matches alignof");
    static_assert(std::tuple_size_v<TS::field_types> == 5,
                  "TestStruct has 5 reflected fields (a1, a2, b1, a4, a3)");
    static_assert(TS::is_abstract == false,
                  "TestStruct is not abstract");

    check(TS::name == std::string_view{"TestStruct"},
          "type_meta<TestStruct>::name");
    check(TS::full_qualified_name == std::string_view{"TestStruct"},
          "type_meta<TestStruct>::full_qualified_name");

    // ---------- type_meta<TestClass> (abstract) ----------
    using TC = type_meta<TestClass>;
    static_assert(TC::is_abstract == true,
                  "TestClass is abstract (has pure virtual)");
    static_assert(TC::size  == sizeof(TestClass),
                  "TestClass size matches sizeof");
    static_assert(std::tuple_size_v<TC::field_types> == 3,
                  "TestClass has 3 reflected fields (a1, a2, a3)");

    // ---------- type_meta<test_ns::TestClass2> ----------
    using TC2 = type_meta<test_ns::TestClass2>;
    static_assert(TC2::size  == sizeof(test_ns::TestClass2),
                  "TestClass2 size");
    check(TC2::full_qualified_name == std::string_view{"test_ns::TestClass2"},
          "type_meta<TestClass2>::full_qualified_name preserves namespace");

    // ---------- type_meta<test_ns::test_ns_2::TestClass3> ----------
    using TC3 = type_meta<test_ns::test_ns_2::TestClass3>;
    check(TC3::full_qualified_name == std::string_view{"test_ns::test_ns_2::TestClass3"},
          "type_meta<TestClass3>::full_qualified_name preserves nested namespace");

    // ---------- field_types tuple element types ----------
    static_assert(std::is_same_v<std::tuple_element_t<0, TS::field_types>, int>,
                  "TestStruct::a1 is int");
    static_assert(std::is_same_v<std::tuple_element_t<1, TS::field_types>, int>,
                  "TestStruct::a2 is int");
    // Field order in generated tuple matches declaration order:
    //   0=a1(int) 1=a2(int) 2=b1(std::string) 3=a4(TestStruct*) 4=a3(UnexposedClass*)
    static_assert(std::is_same_v<std::tuple_element_t<3, TS::field_types>, TestStruct*>,
                  "TestStruct::a4 is TestStruct*");

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
