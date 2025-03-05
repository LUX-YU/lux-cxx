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

#pragma once
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/algotithm/hash.hpp>

#include <unordered_map>
#include <memory>

namespace lux::cxx::dref
{
	struct MetaUnitData
	{
		// All declaration and types
		std::vector<std::unique_ptr<Decl>> declarations;
		std::vector<std::unique_ptr<Type>> types;

		std::unordered_map<std::string, Decl*>  declaration_map;
		std::unordered_map<std::string, Type*>  type_map;

		// Marked declarations
		std::vector<Decl*>				   marked_declarations;
	};

	class MetaUnitImpl
	{
	public:
		friend class MetaUnit; // parse time 

		MetaUnitImpl(std::unique_ptr<MetaUnitData> data, std::string unit_name, std::string unit_version)
			: _data(std::move(data)), _name(std::move(unit_name)), _version(std::move(unit_version))
		{
			const std::string id_str = _name + _version;
			_id = algorithm::fnv1a(id_str);
		}

		~MetaUnitImpl() = default;

	private:
		size_t		 _id;
		std::unique_ptr<MetaUnitData> _data;
		std::string	 _name;
		std::string	 _version;
	};
}
