#pragma once

namespace lux::cxx::dref
{
	class Type;
	class Invokable
	{
	public:
		Type* type = nullptr;
		using invokable_func_t = void(*)(void** params, void* ret);
		invokable_func_t invokable_func = nullptr;

		bool isValid() const { return type != nullptr && invokable_func != nullptr; }
	};
}
