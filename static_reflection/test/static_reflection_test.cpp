#include "lux/cxx/static_reflection/static_type_info.hpp"
#include <iostream>
#include <string>
#include <functional>

struct test_type
{
    int     a;
    double  b;

    double func(double a, std::string b)
    {
        return a + b.size();
    }
};

std::ostream& operator<<(std::ostream& os, const test_type& td)
{
    os << td.a << ' ' << td.b;
    return os;
}

struct test_type2
{
    int         a;
    double      b;
    std::string str;
    test_type   test;
};

using test_type_info = 
lux::cxx::static_type_info<
    test_type,
    MAKE_FIELD_TYPE(test_type, int, a),
    MAKE_FIELD_TYPE(test_type, double, b)
>;

using test_type_info2 = 
lux::cxx::static_type_info<
    test_type2,
    MAKE_FIELD_TYPE(test_type2, int, a),
    MAKE_FIELD_TYPE(test_type2, double, b),
    MAKE_FIELD_TYPE(test_type2, std::string, str),
    MAKE_FIELD_TYPE_EX(test_type2, test_type_info, test)
>;

using test_type_info3 = 
START_STATIC_TYPE_INFO_DECLARATION(test_type2)
    FIELD_TYPE(a),
    FIELD_TYPE(b),
    FIELD_TYPE(str),
    FIELD_TYPE_EX(test_type_info, test)
END_STATIC_TYPE_INFO_DECLARATION();

template<typename T, size_t I, size_t Depth = 0>
void print_field() requires lux::cxx::field_info_type<T>
{
    using field_type_info = typename T::type_info;
    for(size_t i = 0; i < Depth; ++i)
    {
        std::cout << "\t";
    }
    std::cout << "Field " << I << " type: " << field_type_info::name() << " name:" << T::name::view() << std::endl;

    field_type_info::for_each_field(
        []<typename U, size_t J>
        {
            print_field<U, J, Depth + 1>();
        }
    );
}

int main(int argc, char* argv[])
{
    test_type_info2::for_each_field(
        []<typename T, size_t I>
        {
            print_field<T, I>();
        }
    );

    test_type2 obj;
    obj.a       = 10;
    obj.b       = 20.0;
    obj.str     = "Hello, World!";
    obj.test.a  = 30;
    obj.test.b  = 40.0;

    test_type_info2::for_each_field(
        []<typename T, size_t I>(test_type2& info)
        {
            std::cout << T::get(info) << std::endl;      
        },
        obj
    );

    return 0;
}