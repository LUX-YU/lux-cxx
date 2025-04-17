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
#include <lux/cxx/algorithm/hash.hpp>

#include <unordered_map>
#include <memory>

namespace lux::cxx::dref
{
	struct MetaUnitData
	{
		// All declaration and types
		std::vector<std::unique_ptr<Decl>>		declarations;
		std::vector<std::unique_ptr<Type>>		types;

		std::unordered_map<std::string, Decl*>  declaration_map;
		std::unordered_map<std::string, Type*>  type_map;
		std::unordered_map<std::string, Type*>  type_alias_map;

		// Marked declarations
		std::vector<CXXRecordDecl*>				marked_record_decls;
		std::vector<FunctionDecl*>				marked_function_decls;
		std::vector<EnumDecl*>					marked_enum_decls;
	};

	class MetaUnitImpl
	{
	public:
		friend class MetaUnit; // parse time 

		MetaUnitImpl(const MetaUnitImpl&) = delete;
		MetaUnitImpl& operator=(const MetaUnitImpl&) = delete;

		MetaUnitImpl(MetaUnitImpl&&) = default;
		MetaUnitImpl& operator=(MetaUnitImpl&&) = default;

		MetaUnitImpl() {
			_id   = 0;
			_data = std::make_unique<MetaUnitData>();
		}

		/**
		 * @brief Construct a new MetaUnitImpl object
		 * @param data The data to be used
		 * @param unit_name The name of the unit, usually the md5 of file
		 */
		MetaUnitImpl(std::unique_ptr<MetaUnitData> data, std::string unit_name, std::string unit_version)
			: _data(std::move(data)), _name(std::move(unit_name)), _version(std::move(unit_version))
		{
			const std::string id_str = _name + _version;
			_id = algorithm::fnv1a(id_str);
		}

		~MetaUnitImpl() = default;

		Decl* findDeclById(const std::string& idStr) const
		{
			auto it = _data->declaration_map.find(idStr);
			if (it != _data->declaration_map.end()) {
				return it->second;
			}
			return nullptr;
		}

		Type* findTypeById(const std::string& idStr) const
		{
			auto it = _data->type_map.find(idStr);
			if (it != _data->type_map.end()) {
				return it->second;
			}
			return nullptr;
		}

		nlohmann::json toJson() const;
		static void fromJson(const std::string& json, MetaUnitImpl& unit);

	private:
		size_t						  _id;
		std::unique_ptr<MetaUnitData> _data;
		std::string					  _name;
		std::string					  _version;
	};
}
