#pragma once
#include "entity.hpp"

namespace lux::cxx::lan_model
{
	struct FunctionDeclaration;

	/*
	* https://en.cppreference.com/w/cpp/language/functions
	* 
	* Each function has a type, which consists of the
	* function's return type, the types of all parameters 
	* (after array-to-pointer and function-to-pointer 
	* transformations, see parameter list),
	* whether the function is noexcept or not (since C++17),
	* and, for non-static member functions, cv-qualification
	* and ref-qualification (since C++11). 
	* 
	* Function types also have language linkage. 
	* There are no cv-qualified function types 
	* (not to be confused with the types of 
	* cv-qualified functions such as int f() const; 
	* or functions returning cv-qualified types, 
	* such as std::string const f();). 
	* Any cv-qualifier is ignored if it is added to an alias for a 
	* function type.
	*/
	struct Function : public Entity
	{
		FunctionDeclaration*	declaration;
		void*					body;
	};
}
