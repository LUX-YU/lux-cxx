/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "test_header2.hpp"
#include "test_header2.meta.static.hpp"
#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include "register_all_dynamic_meta.hpp"

#include <iostream>

using namespace lux::cxx::dref;

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
    runtime::RuntimeRegistry registry;
    runtime::registerAllMeta(registry);


    TestClass obj;
    displayClassInfo<TestClass>(obj);
    return 0;
}