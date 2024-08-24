#include <lux/cxx/compile_time/type_info.hpp>
#include <iostream>

struct test_type
{
    int    a;
    double b;
};

int main()
{
    using namespace lux::cxx;
    static constexpr auto info = make_basic_type_info<test_type>();
    std::cout << "Type name: " << info.name() << std::endl;
    std::cout << "Type hash: " << info.hash() << std::endl;
    static constexpr auto mem_name = 
        strip_field_name<std::addressof(fake_obj<test_type>.a)>();
    std::cout << "Member name: " << mem_name << std::endl; 
    return 0;
}