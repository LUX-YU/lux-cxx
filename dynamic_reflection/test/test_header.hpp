#pragma once
#include <lux/cxx/dref/runtime/Attribute.hpp>

/*
#include <vector>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <utility>
*/
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstdio>

using FuncType = void(int, double);

class UnexposedClass;
typedef int myint;

void LUX_REFL(static_reflection) TestFunction(
    LUX_REFL(parameter) myint z,
    size_t&& c,
    const double& d,
    const std::string& str,
    FuncType* func_ptr
)
{

}

struct LUX_REFL(static_reflection) TestStruct
{
    int a1;
    int a2;
    std::string b1;
    TestStruct* a4;

    UnexposedClass* a3;
};

enum class LUX_REFL(static_reflection) TestEnum
{
    VALUE1,
    VALUE2 = 100,
    VALUE3
};

enum LUX_REFL(static_reflection){
  AnonymousEnum = 100,
};

class LUX_REFL(static_reflection) TestClass
{
public:
    TestClass();

    TestClass(TestEnum);

    ~TestClass() {}

    int LUX_REFL(int) a1;
protected:
    int a2;

    virtual void __virtual_func() = 0;
private:
    const int* a3;

    void __function(int**, double*, long&){}
};

namespace test_ns
{
    class LUX_REFL(dynamic_reflection) TestClass2
    {
    public:
        int a1;
    protected:
        long LUX_REFL(long) a2;
    private:
        const int* a3;
        FuncType a4;
    };

    namespace test_ns_2
    {
        class LUX_REFL(static_reflection) TestClass3
        {
        public:
            enum class InternalEnum
            {
                VALUE_1,
                VALUE_2
            };

            int a1;
        protected:
            long LUX_REFL(long) a2;
        private:
            const int* a3;
            FuncType a4;
        };
    }
}

struct LUX_REFL(static_reflection) TestStruct2 : public test_ns::test_ns_2::TestClass3, test_ns::TestClass2
{
public:
    using AliasType = test_ns::TestClass2;

    enum class InternalEnum
    {
        VALUE_1,
        VALUE_2
    };

    void __func(test_ns::TestClass2&)
    {

    }

    void __func_const_declaration() const {

    }

    static int __static_func()
    {
        return 1;
    }

    static void __static_func_declaration() {

    }
};