#pragma once
#include <lux/cxx/dref/runtime/Attribute.hpp>
#include <string>

struct LUX_REFL(static_reflection;dynamic_reflection) TestClass
{
    std::string_view field1;
    int              field2;
    double           field3;

    int func1(int a, double b)
    {
        return field2 + a / b;
    }

    double func2()
    {
        return field3;
    }

    std::string_view func3() const
    {
        return field1;
    }

    static void static_func1()
    {
        // ...
    }

    static std::string static_func2()
    {
        return "";
    }
};

enum class LUX_REFL(static_reflection;dynamic_reflection) TestEnum
{
    FIRST_ENUMERATOR = 100,
    SECOND_ENUMERATOR = 200,
    THIRD_ENUMERATOR = 4,
};
