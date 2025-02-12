#pragma once
#include <lux/cxx/dref/parser/Attribute.hpp>

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

LUX_REF_MARK(function) void TestFunction( 
    LUX_REF_MARK(parameter) myint z, 
    size_t&& c, 
    const double& d, 
    const std::string& str,
    FuncType* func_ptr
)
{

}

struct LUX_REF_MARK(struct) TestStruct
{
    int a1;
    int a2;
    std::string b1;
    TestStruct* a4;

    UnexposedClass* a3;
};

enum class LUX_REF_MARK(enum) TestEnum
{
    VALUE1,
    VALUE2 = 100,
    VALUE3
};

enum LUX_REF_MARK(enum){
  AnonymousEnum = 100,
};

class LUX_REF_MARK(class) TestClass
{
public:
    TestClass();

    TestClass(TestEnum);

    ~TestClass();

    int LUX_REF_MARK(int) a1;
protected:
    int a2;

    virtual void __virtual_func() = 0;
private:
    const int* a3;
    
    void __function(int**, double*, long&){}
};

namespace test_ns
{
    class LUX_REF_MARK(class) TestClass2
    {
    public:
        int a1;
    protected:
        long LUX_REF_MARK(long) a2;
    private:
        const int* a3;
        FuncType a4;
    };

    namespace test_ns_2
    {
        class LUX_REF_MARK(class) TestClass3
        {
        public:
            enum class InternalEnum
            {
                VALUE_1,
                VALUE_2
            };

            int a1;
        protected:
            long LUX_REF_MARK(long) a2;
        private:
            const int* a3;
            FuncType a4;
        };
    }
}

struct LUX_REF_MARK(struct) TestStruct2 : public test_ns::test_ns_2::TestClass3, test_ns::TestClass2
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

    void __func_const_declaration() const;

    static int __static_func()
    {
        return 1;
    }

    static void __static_func_declaration();
};
