#include <lux/cxx/compile_time/fixed_string.hpp>
#include <iostream>
#include <cassert>
#include <string_view>
#include <sstream>

using namespace lux::cxx;

#define TEST_ASSERT(cond) \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << std::endl; \
        assert(cond); \
    }

// ---- compile-time tests via static_assert -----------------------------------

// Construction from literal
constexpr fixed_string hello = "Hello";
static_assert(hello.size() == 5);
static_assert(hello.capacity == 5);
static_assert(hello[0] == 'H');
static_assert(hello[4] == 'o');
static_assert(!hello.empty());

// Default construction
constexpr fixed_string<10> empty_str;
static_assert(empty_str.size() == 0);
static_assert(empty_str.empty());

// Fill construction
constexpr fixed_string<4> fill_str(3, 'x');
static_assert(fill_str.size() == 3);
static_assert(fill_str[0] == 'x');
static_assert(fill_str[2] == 'x');

// Comparison across capacities
constexpr fixed_string a = "abc";
constexpr fixed_string<10> b(std::string_view("abc"));
static_assert(a == b);

constexpr fixed_string c = "abd";
static_assert(a < c);
static_assert(a != c);

// Comparison with string_view
static_assert(hello == std::string_view("Hello"));
static_assert(hello != std::string_view("World"));

// Concatenation
constexpr auto hello_world = fixed_string("Hello") + fixed_string(" World");
static_assert(hello_world.size() == 11);
static_assert(hello_world == std::string_view("Hello World"));
static_assert(hello_world.capacity == 11); // 5 + 6

// Search
static_assert(hello.starts_with("Hel"));
static_assert(!hello.starts_with("hel"));
static_assert(hello.ends_with("llo"));
static_assert(!hello.ends_with("LL"));
static_assert(hello.contains("ell"));
static_assert(!hello.contains("xyz"));
static_assert(hello.find("ll") == 2);
static_assert(hello.rfind("l") == 3);

// Substr
constexpr auto sub = hello.substr(1, 3);
static_assert(sub == std::string_view("ell"));

// front / back
static_assert(hello.front() == 'H');
static_assert(hello.back() == 'o');

// NTTP usage
template <fixed_string S>
constexpr auto get_size() { return S.size(); }

static_assert(get_size<"test">() == 4);
static_assert(get_size<"">() == 0);

// ---- runtime tests ----------------------------------------------------------

void test_construction()
{
    fixed_string s = "Runtime";
    TEST_ASSERT(s.size() == 7);
    TEST_ASSERT(s.view() == "Runtime");
    TEST_ASSERT(std::string_view(s) == "Runtime");
    TEST_ASSERT(s.c_str()[7] == '\0');

    fixed_string<20> from_sv(std::string_view("from_sv"));
    TEST_ASSERT(from_sv.size() == 7);
    TEST_ASSERT(from_sv == std::string_view("from_sv"));

    fixed_string<5> from_ptr("ptr", 3);
    TEST_ASSERT(from_ptr.size() == 3);
    TEST_ASSERT(from_ptr == std::string_view("ptr"));

    std::cout << "  construction tests passed" << std::endl;
}

void test_iteration()
{
    fixed_string s = "abcd";
    std::string collected;
    for (auto ch : s)
        collected += ch;
    TEST_ASSERT(collected == "abcd");

    TEST_ASSERT(s.begin() + 4 == s.end());
    TEST_ASSERT(s.cbegin() + 4 == s.cend());

    std::cout << "  iteration tests passed" << std::endl;
}

void test_concatenation()
{
    fixed_string a = "foo";
    fixed_string b = "bar";
    auto c = a + b;
    TEST_ASSERT(c.size() == 6);
    TEST_ASSERT(c == std::string_view("foobar"));
    TEST_ASSERT(c.capacity == 6);

    auto d = concat(a, b);
    TEST_ASSERT(d == c);

    std::cout << "  concatenation tests passed" << std::endl;
}

void test_search()
{
    fixed_string s = "Hello, World!";
    TEST_ASSERT(s.find("World") == 7);
    TEST_ASSERT(s.find("xyz") == std::string_view::npos);
    TEST_ASSERT(s.rfind("l") == 10);
    TEST_ASSERT(s.starts_with("Hello"));
    TEST_ASSERT(s.ends_with("World!"));
    TEST_ASSERT(s.contains(","));
    TEST_ASSERT(!s.contains("?"));

    std::cout << "  search tests passed" << std::endl;
}

void test_substr()
{
    fixed_string s = "abcdefgh";
    auto sub1 = s.substr(2, 4);
    TEST_ASSERT(sub1 == std::string_view("cdef"));

    auto sub2 = s.substr(5);
    TEST_ASSERT(sub2 == std::string_view("fgh"));

    std::cout << "  substr tests passed" << std::endl;
}

void test_comparison()
{
    fixed_string a = "abc";
    fixed_string b = "abc";
    fixed_string c = "abd";
    fixed_string<10> d(std::string_view("abc"));

    TEST_ASSERT(a == b);
    TEST_ASSERT(a != c);
    TEST_ASSERT(a < c);
    TEST_ASSERT(c > a);
    TEST_ASSERT(a <= b);
    TEST_ASSERT(a >= b);

    // Cross-capacity comparison
    TEST_ASSERT(a == d);
    TEST_ASSERT(!(a != d));

    std::cout << "  comparison tests passed" << std::endl;
}

void test_stream_output()
{
    fixed_string s = "stream_test";
    std::ostringstream oss;
    oss << s;
    TEST_ASSERT(oss.str() == "stream_test");

    std::cout << "  stream output tests passed" << std::endl;
}

int main()
{
    std::cout << "fixed_string tests:" << std::endl;
    test_construction();
    test_iteration();
    test_concatenation();
    test_search();
    test_substr();
    test_comparison();
    test_stream_output();
    std::cout << "All fixed_string tests passed!" << std::endl;
    return 0;
}
