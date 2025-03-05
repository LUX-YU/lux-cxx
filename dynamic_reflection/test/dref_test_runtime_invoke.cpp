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

#include <lux/cxx/dref/runtime/MetaType.hpp>
#include <lux/cxx/dref/runtime/Method.hpp>
#include <lux/cxx/dref/runtime/Instance.hpp>
#include <iostream>
#include <chrono>
#include <random>

class TestClass
{
public:
	double test_method(int arg1, double arg2)
	{
		return arg1 + a + std::sinf(arg1 + a);
	}
private:
	int a = 10;
};

#define STATIC_NEW_FUNCTION_META_DECLARE(TYPE)\
	static FunctionTypeMeta TYPE##_new_func_meta;\
	static TypeMeta			TYPE##_new_result_type_meta;\
	TYPE##_new_func_meta.func_name		= # TYPE "_new_func_meta";\
	TYPE##_new_func_meta.func_wrap_ptr	= static_cast<Function::InvokerPtr>(new_func<TYPE>);\
	TYPE##_new_func_meta.is_static		= true;\
	TYPE##_new_func_meta.meta_type		= MetaType::FUNCTION;\
	TYPE##_new_func_meta.type_id		= THash( #TYPE "_new_func_meta");\
	TYPE##_new_func_meta.type_name		= "";\
	TYPE##_new_func_meta.type_underlying_name = "";\
	TYPE##_new_func_meta.result_info	= &TYPE##_new_result_type_meta;\
	TYPE##_new_func_meta.result_info->type_id = THash("void*");

#define STATIC_DELETE_FUNCTION_META_DECLARE(TYPE)\
	static FunctionTypeMeta TYPE##_delete_func_meta;\
	static TypeMeta			TYPE##_delete_result_type_meta;\
	static TypeMeta			TYPE##_delete_parameter_type_meta;\
	TYPE##_delete_func_meta.func_name		= # TYPE "_delete_func_meta";\
	TYPE##_delete_func_meta.func_wrap_ptr	= static_cast<Function::InvokerPtr>(delete_func<TYPE>);\
	TYPE##_delete_func_meta.is_static		= true;\
	TYPE##_delete_func_meta.meta_type		= MetaType::FUNCTION;\
	TYPE##_delete_func_meta.type_id			= THash( #TYPE "_delete_func_meta");\
	TYPE##_delete_func_meta.type_name		= "";\
	TYPE##_delete_func_meta.type_underlying_name = "";\
	TYPE##_delete_func_meta.result_info		= &TYPE##_delete_result_type_meta;\
	TYPE##_delete_func_meta.result_info->type_id = 0;\
	TYPE##_delete_parameter_type_meta.type_id = THash("void*");\
	TYPE##_delete_func_meta.parameters_list.push_back(&TYPE##_delete_parameter_type_meta);

namespace lux::cxx::dref
{
	static void TestClass_test_method_wrap(void* _ins, void** _args)
	{
		TestClass* instance = (TestClass*)_ins;

		void* ret = _args[0];

		*(double*)ret = instance->test_method(
			(*reinterpret_cast<std::add_pointer_t<int>>(_args[1])),
			(*reinterpret_cast<std::add_pointer_t<double>>(_args[2]))
		);
	}

	template<typename T>
	static void new_func(void** args)
	{
		void** ret = static_cast<void**>(args[0]);
		*ret = new T;
	}

	template<typename T>
	static void delete_func(void** args)
	{
		void** object = static_cast<void**>(args[1]);

		delete static_cast<T*>(*object);
	}

	static ClassMeta* initialize_TestClass()
	{
		STATIC_NEW_FUNCTION_META_DECLARE(int)
		STATIC_DELETE_FUNCTION_META_DECLARE(int)
		STATIC_NEW_FUNCTION_META_DECLARE(double)
		STATIC_DELETE_FUNCTION_META_DECLARE(double)
		STATIC_NEW_FUNCTION_META_DECLARE(TestClass)
		STATIC_DELETE_FUNCTION_META_DECLARE(TestClass)

			static DataTypeMeta		int_meta;
		int_meta.type_id = TFnv1a("int");
		int_meta.type_name = "int";
		int_meta.meta_type = DataTypeMeta::self_meta_type;
		int_meta.size = sizeof(int_meta);

		int_meta.constructor_meta = &int_new_func_meta;
		int_meta.destructor_meta = &int_delete_func_meta;

		static DataTypeMeta		double_meta;
		double_meta.type_id = TFnv1a("double");
		double_meta.type_name = "double";
		double_meta.meta_type = DataTypeMeta::self_meta_type;
		double_meta.size = sizeof(double);
		double_meta.constructor_meta = &double_new_func_meta;
		double_meta.destructor_meta = &double_delete_func_meta;

		static ClassMeta		class_meta;
		static MethodMeta		test_method_meta;
		static TypeMeta			test_method_result_meta;
		static TypeMeta			int_type_meta;
		static TypeMeta			double_type_meta;

		int_type_meta.type_id = THash("int");
		double_type_meta.type_id = THash("double");

		test_method_result_meta.type_id = THash("double");
		test_method_meta.func_name = "test_method";
		test_method_meta.result_info = &test_method_result_meta;
		test_method_meta.func_wrap_ptr = &TestClass_test_method_wrap;
		test_method_meta.parameters_list.push_back(&int_type_meta);
		test_method_meta.parameters_list.push_back(&double_type_meta);

		class_meta.type_id = lux::cxx::dref::fnv1a("TestClass");
		class_meta.type_name = "TestClass";
		class_meta.type_underlying_name = "TestClass";
		class_meta.meta_type = lux::cxx::dref::MetaType::CLASS;
		class_meta.size = sizeof(TestClass);
		class_meta.constructor_meta = &TestClass_new_func_meta;
		class_meta.destructor_meta = &TestClass_delete_func_meta;
		class_meta.align = { 4 };
		class_meta.field_meta_list = {};
		class_meta.parents_list = {};
		class_meta.method_meta_list = { &test_method_meta };
		return &class_meta;
	}
}

static lux::cxx::dref::ClassMeta* class_meta{lux::cxx::dref::initialize_TestClass()};

#pragma optimize( "", off ) 
static int __main(int argc, char* argv[])
{
	using namespace lux::cxx::dref;

	Class test_class(class_meta);

	ClassInstance instance(test_class); // wrap install
	TestClass* test_class_instance = new TestClass();

	Method test_method = instance.getMethod("test_method");

	double ret;
	srand((uint64_t)(time(nullptr)));
	int arg1 = rand();
	double arg2 = 0.01;

#define TEST_TINVOKE_NO_CHECK
	auto start_time = std::chrono::system_clock::now();
	for (int i = 0; i < 500000000; i++)
	{
#if defined TEST_TINVOKE
		test_method.tInvoke(LUX_ARG(double, ret), LUX_ARG(int, arg1), LUX_ARG(double, arg2));
#elif defined TEST_TINVOKE_NO_CHECK
		test_method.tInvokeNoCheck(LUX_ARG(double, ret), LUX_ARG(int, arg1), LUX_ARG(double, arg2));
#elif defined TEST_RAW
		test_class_instance->test_method(arg1, arg2);
#endif
	}
	auto end_time = std::chrono::system_clock::now();

	std::cout << "Wrap Function Invoke Time:" << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << " ms" << std::endl;

	// std::cout << ret << std::endl;

	delete test_class_instance;
	return 0;
}
#pragma optimize( "", on )

#include <lux/cxx/subprogram/subprogram.hpp>
RegistFuncSubprogramEntry(__main, "runtime_invoke")
