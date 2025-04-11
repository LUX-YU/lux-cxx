#pragma once
#include <lux/cxx/dref/runtime/Attribute.hpp>
#include <string>

struct LUX_REFL(static_reflection;dynamic_reflection) TestClass
{
    std::string_view field1;
    int              field2;
    double           field3;

	TestClass(int a, double b)
		: field1("Hello, World!")
		, field2(a)
		, field3(b)
	{
	}

	TestClass()
		: field1("Hello, World!")
		, field2(42)
		, field3(3.14)
	{
	}

    double func1(int a, double b, std::string)
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

    static std::string static_func2(int arg1, int arg2)
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

namespace lux::cxx::dref::test
{
    struct LUX_REFL(static_reflection;dynamic_reflection) TestClass
    {
        std::string_view field1;
        int              field2;
        double           field3;

        double func1(int a, double b)
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

    static inline int LUX_REFL(dynamic_reflection) func()
    {
        return 0;
    }

	static inline int LUX_REFL(dynamic_reflection) func(int a)
	{
		return a;
	}
}