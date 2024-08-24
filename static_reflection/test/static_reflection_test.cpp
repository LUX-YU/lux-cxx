#include "lux/cxx/static_reflection/static_type_info.hpp"
#include <iostream>
#include <string>

struct test_type
{
    int     a;
    double  b;
};

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
    MAKE_FIELD_TYPE(int, a),
    MAKE_FIELD_TYPE(double, b)
>;

using test_type_info2 = 
lux::cxx::static_type_info<
    test_type2,
    MAKE_FIELD_TYPE(int, a),
    MAKE_FIELD_TYPE(double, b),
    MAKE_FIELD_TYPE(std::string, str),
    MAKE_FIELD_TYPE_EX(test_type_info, test)
>;

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

    return 0;
}