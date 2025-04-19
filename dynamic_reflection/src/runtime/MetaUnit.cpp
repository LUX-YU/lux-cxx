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

#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/dref/runtime/MetaUnitImpl.hpp>
#include <lux/cxx/dref/parser/libclang.hpp>

namespace lux::cxx::dref
{
	size_t MetaUnit::calculateTypeMetaId(std::string_view name)
	{
		return lux::cxx::algorithm::fnv1a(name);
	}

	MetaUnit::MetaUnit()
		:_impl(nullptr)
	{
		
	}

	MetaUnit::MetaUnit(MetaUnit&& other) noexcept
	{
		_impl = std::move(other._impl);
	}

	MetaUnit& MetaUnit::operator=(MetaUnit&& other) noexcept
	{
		_impl = std::move(other._impl);
		return *this;
	}

	MetaUnit::MetaUnit(std::unique_ptr<MetaUnitImpl> impl)
	{
		_impl = std::move(impl);
	}

	MetaUnit::~MetaUnit() = default;

	size_t MetaUnit::id() const
	{
		return _impl->_id;
	}

	const std::string& MetaUnit::name() const
	{
		return _impl->_name;
	}

	const std::string& MetaUnit::version() const
	{
		return _impl->_version;
	}

	const std::vector<CXXRecordDecl*>& MetaUnit::markedRecordDecls() const
	{
		return _impl->_data->marked_record_decls;
	}

	const std::vector<FunctionDecl*>& MetaUnit::markedFunctionDecls() const
	{
		return _impl->_data->marked_function_decls;
	}

	const std::vector<EnumDecl*>& MetaUnit::markedEnumDecls() const
	{
		return _impl->_data->marked_enum_decls;
	}

	const Decl* MetaUnit::findDeclById(const std::string& id) const
	{
		return _impl->findDeclById(id);
	}

	const Type* MetaUnit::findTypeById(const std::string& id) const
	{
		return _impl->findTypeById(id);
	}

	nlohmann::json MetaUnit::toJson() const
	{
		return _impl->toJson();
	}

	void MetaUnit::fromJson(const std::string& json, MetaUnit& unit)
	{
		auto impl = std::make_unique<MetaUnitImpl>();
		MetaUnitImpl::fromJson(json, *impl);

		unit = MetaUnit(std::move(impl));
	}
}