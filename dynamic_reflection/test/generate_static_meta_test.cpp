#include "test_header2.hpp"
#include "test_header2.meta.hpp"

#include <iostream>

using namespace lux::cxx::dref;

static const char* visibility2Str(EVisibility visibility)
{
    if (visibility == EVisibility::INVALID)
        return "INVALID";
    if (visibility == EVisibility::PUBLIC)
        return "PUBLIC";
    if (visibility == EVisibility::PRIVATE)
        return "PRIVATE";

    return "PROTECTED";
}

template<size_t N>
static constexpr void displayField()
{
    std::cout << N << '\n';
}

template<typename T, typename Meta = type_meta<T>>
requires (Meta::meta_type == EMetaType::ClASS || Meta::meta_type == EMetaType::STRUCT)
static constexpr void displayClassInfo(T& obj)
{
    std::cout << "Class name: " << Meta::name << std::endl;
    std::cout << "size:       " << Meta::size << std::endl;
    std::cout << "align:      " << Meta::align << std::endl;
    // list field
    Meta::foreach_field(
        [](auto&& field)
        {
            std::cout << field << std::endl;
        },
        obj
    );
}

int main(int argc, char* argv[])
{
    TestClass test{
        .field1 = "123",
        .field2 = 456,
        .field3 = 7.89
    };
    displayClassInfo<TestClass>(test);
    return 0;
}