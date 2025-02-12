#include <functional>
#include <lux/cxx/compile_time/function_traits.hpp>
#include <iostream>

void common_func_test()
{
    using namespace ::lux::cxx;
    using return_type   = typename function_traits<void(int, double)>::return_type;
    using argument_type = typename function_traits<void(int, double)>::argument_types;

    std::cout << typeid(return_type).name() << std::endl;
    std::cout << typeid(argument_type).name() << std::endl;
}

void lambda_test()
{
    using namespace ::lux::cxx;
    int a;
    using lambda_type   = decltype([&a](int, double&, std::string***){return float{};});
    using return_type   = typename function_traits<lambda_type>::return_type;
    using argument_type = typename function_traits<lambda_type>::argument_types;
    std::cout << typeid(return_type).name() << std::endl;
    std::cout << typeid(argument_type).name() << std::endl;
}

class TestClass
{
public:
    std::string func(const int*, double&, const std::string&)
    {
        return "";
    }

};

void member_func_test()
{
    using namespace ::lux::cxx;
    using return_type   = typename function_traits<decltype(&TestClass::func)>::return_type;
    using argument_type = typename function_traits<decltype(&TestClass::func)>::argument_types;
    std::cout << typeid(return_type).name() << std::endl;
    std::cout << typeid(argument_type).name() << std::endl;
}

int main() {
    common_func_test();
    lambda_test();
    member_func_test();

    return 0;
}