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
#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <lux/cxx/dref/runtime/MetaBuilder.hpp>
#include <lux/cxx/dref/runtime/MetaRegistry.hpp>
#include <register_all_dynamic_meta.hpp>
#include <iostream>
 
using namespace lux::cxx::dref;

int test_func(int a, int b)
{
	return a + b;
}

class TestClass
{
public:
	int test_method(int a, int b)
	{
		return a + b;
	}
};

int main(int argc, char* argv[])
{
	runtime::RuntimeMetaRegistry registry;
	runtime::register_reflections(registry);

	auto funcMeta = runtime::FunctionMetaBuilder<&test_func>().setName("test_func")
		.setQualifiedName("test_func(int)")
		.setHash(123456789)
		.build();

	auto methodMeta = runtime::MethodMetaBuilder<&TestClass::test_method>().setName("test_method")
		.setQualifiedName("TestClass::test_method(int)")
		.setHash(987654321)
		.build();

	int a = 5; int b = 3; int ret; void* args[] = { &a, &b };
	std::get<0>(funcMeta.invokable_info.invoker)(args, &ret);
	std::cout << "Function result: " << ret << std::endl;

	TestClass obj;
	std::get<1>(methodMeta.invokable_info.invoker)(&obj, args, &ret);
	std::cout << "Method result: " << ret << std::endl;

	for (auto& fundamental : registry.sFundamentals())
	{
		std::cout << "Fundamental: " << fundamental->basic_info.name << std::endl;
	}

    return 0;
}
