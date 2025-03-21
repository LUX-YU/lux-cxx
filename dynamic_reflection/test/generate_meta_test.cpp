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
requires (Meta::meta_type == EMetaType::CLASS || Meta::meta_type == EMetaType::STRUCT)
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

	const auto& enum_list   = registry.enumMetaList();
	const auto& record_list = registry.recordMetaList();
	const auto& func_list   = registry.functionMetaList();

	for (const auto& _enum : enum_list)
	{
		std::cout << "Enum: " << _enum->name << std::endl;
		for (const auto& enumerator : _enum->enumerators)
		{
			std::cout << "\tEnumerator: " << enumerator.name << " Value:" << enumerator.value << std::endl;
		}
	}

	for (const auto& record : record_list)
	{
		std::cout << "Record: " << record->name << std::endl;
		for (const auto& field : record->fields)
		{
			std::cout << "\tField: " << field.name << " Offset:" << field.offset << std::endl;
		}
		for (const auto& method : record->methods)
		{
			std::cout << "\tMethod: " << method.name << std::endl;
		}
		for (const auto& static_method : record->static_methods)
		{
			std::cout << "\tStatic Method: " << static_method.name << std::endl;
		}
	}

	// create object test
	auto meta_info = registry.findMeta("lux::cxx::dref::test::TestClass");
	if (!meta_info)
	{
		std::cerr << "Error: WTF! Could not find metadata for class 'lux::cxx::dref::test::TestClass'" << std::endl;
		return 1;
	}

	if (meta_info->type != lux::cxx::dref::runtime::ERuntimeMetaType::RECORD)
	{
		std::cerr << "Error: WTF! Metadata for class 'lux::cxx::dref::test::TestClass' is not a record" << std::endl;
	}

	auto record_meta = registry.findRecord(meta_info->index);
	if (!record_meta)
	{
		std::cerr << "Error: WTF! Could not find record metadata for class 'lux::cxx::dref::test::TestClass'" << std::endl;
		return 1;
	}

	for (const auto& func : func_list)
	{
		std::cout << "Function: " << func->name << std::endl;
	}

	// create object test
	auto test_class_obj = record_meta->ctor[0].invoker(nullptr);
	if (!test_class_obj)
	{
		std::cerr << "Error: WTF! Could not create object for class 'lux::cxx::dref::test::TestClass'" << std::endl;
	}
	// invoke method
	int a = 1; double b = 2.0;
	void* args[] = { &a, &b }; double ret;
	record_meta->methods[0].invoker(test_class_obj, args, &ret);
	std::cout << "Method return value: " << ret << std::endl;
	// destruct object
	record_meta->dtor(test_class_obj);


    TestClass obj;
    displayClassInfo<TestClass>(obj);
    return 0;
}