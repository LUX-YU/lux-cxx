#include <lux/cxx/compile_time/ct_string.hpp>
#include <iostream>
#include <string_view>
#include <cassert>
#include <type_traits>
#include <utility>

using namespace lux::cxx;

// Helper to verify test results
#define TEST_ASSERT(condition) \
    if (!(condition)) { \
        std::cerr << "Assertion failed: " << #condition << " at line " << __LINE__ << std::endl; \
        assert(condition); \
    }

// Test for literal_ct_string
void test_literal_ct_string() {
    // Default constructor test (empty string)
    constexpr literal_ct_string<char, 0> empty_str{};
    static_assert(empty_str.size == 0);
    static_assert(empty_str.data[0] == 0);
    
    // Single character constructor
    constexpr literal_ct_string<char, 1> char_str('A');
    static_assert(char_str.size == 1);
    static_assert(char_str.data[0] == 'A');
    static_assert(char_str.data[1] == 0);
    
    // String view constructor
    constexpr std::string_view hello = "Hello";
    constexpr literal_ct_string<char, 5> view_str(hello);
    static_assert(view_str.size == 5);
    static_assert(view_str.data[0] == 'H');
    static_assert(view_str.data[5] == 0);
    
    // Character array constructor
    constexpr literal_ct_string<char, 5> array_str("Hello");
    static_assert(array_str.size == 5);
    static_assert(array_str.data[0] == 'H');
    static_assert(array_str.data[5] == 0);
    
    // In-place constructor
    constexpr literal_ct_string<char, 3> in_place_str(std::in_place, 'A', 'B', 'C');
    static_assert(in_place_str.size == 3);
    static_assert(in_place_str.data[0] == 'A');
    static_assert(in_place_str.data[3] == 0);
    
    std::cout << "literal_ct_string tests passed" << std::endl;
}

// Test for ct_string
void test_ct_string() {
    // Basic creation and properties
    using hello_string = ct_string<literal_ct_string<char, 5>("Hello")>;
    static_assert(hello_string::size() == 5);
    static_assert(hello_string::view() == "Hello");
    
    // Test with MAKE_CT_STRING macro
    auto world_string = MAKE_CT_STRING("World");
    using world_string_type = decltype(world_string);
    static_assert(world_string_type::size() == 5);
    static_assert(world_string_type::view() == "World");
    
    // Test with MAKE_CT_STRING_TYPE macro
    using test_string_type = MAKE_CT_STRING_TYPE("Test");
    static_assert(test_string_type::size() == 4);
    static_assert(test_string_type::view() == "Test");
    
    // Test ct_string_s (character pack)
    using abc_string = ct_string_s<'A', 'B', 'C'>;
    static_assert(abc_string::size() == 3);
    static_assert(abc_string::view() == "ABC");
    
    // Test ct_string_c (single character)
    using x_string = ct_string_c<'X'>;
    static_assert(x_string::size() == 1);
    static_assert(x_string::view() == "X");
    
    std::cout << "ct_string tests passed" << std::endl;
}

// Test for type traits and concepts
void test_type_traits() {
    // Test is_ct_string trait
    using hello_string = ct_string<literal_ct_string<char, 5>("Hello")>;
    static_assert(is_ct_string_v<hello_string>);
    static_assert(!is_ct_string_v<int>);
    static_assert(!is_ct_string_v<std::string_view>);
    
    // Test ct_string_type concept
    static_assert(ct_string_type<hello_string>);
    
    std::cout << "Type traits tests passed" << std::endl;
}

// Test for string concatenation
void test_string_concatenation() {
    using hello_string = MAKE_CT_STRING_TYPE("Hello");
    using world_string = MAKE_CT_STRING_TYPE("World");
    
    // Test ct_string_cat_helper
    using hello_world_string = ct_string_cat_helper_t<hello_string, world_string>;
    static_assert(hello_world_string::size() == 10);
    static_assert(hello_world_string::view() == "HelloWorld");
    
    // Test ct_string_cat function
    auto hello_world = ct_string_cat(hello_string{}, world_string{});
    using concat_result_type = decltype(hello_world);
    static_assert(concat_result_type::size() == 10);
    static_assert(concat_result_type::view() == "HelloWorld");
    
    std::cout << "String concatenation tests passed" << std::endl;
}

// Test for runtime string operations
void test_runtime_operations() {
    auto hello_string = MAKE_CT_STRING("Hello");
    
    // Test conversion to string_view
    std::string_view view = hello_string;
    TEST_ASSERT(view == "Hello");
    
    // Test data() access
    const char* data = hello_string.data();
    TEST_ASSERT(std::string_view(data) == "Hello");
    
    std::cout << "Runtime operations tests passed" << std::endl;
}

// Test various edge cases
void test_edge_cases() {
    // Empty string
    auto empty_string = MAKE_CT_STRING("");
    TEST_ASSERT(empty_string.size() == 0);
    TEST_ASSERT(empty_string.view().empty());
    
    // String with special characters
    auto special_string = MAKE_CT_STRING("!@#$%^&*()");
    TEST_ASSERT(special_string.view() == "!@#$%^&*()");
    
    // Unicode characters (note: this is a simple test with basic Latin-1 chars)
    auto unicode_string = MAKE_CT_STRING("áéíóú");
    TEST_ASSERT(unicode_string.size() == 10); // UTF-8 encoded chars use 2 bytes each
    
    std::cout << "Edge cases tests passed" << std::endl;
}

int main() {
    test_literal_ct_string();
    test_ct_string();
    test_type_traits();
    test_string_concatenation();
    test_runtime_operations();
    test_edge_cases();
    
    std::cout << "All compile-time string tests passed!" << std::endl;
    return 0;
}