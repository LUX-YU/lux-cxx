#pragma once
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

#include <deque>
#include <lux/cxx/visibility.h>
#include <string_view>
#include <memory>

#include "Declaration.hpp"
#include "Type.hpp"

namespace lux::cxx::dref
{
	class MetaUnitImpl;
	class MetaUnitData;
	class LUX_CXX_PUBLIC MetaUnit final
	{
		friend class CxxParserImpl;
	public:
		MetaUnit();

		MetaUnit(MetaUnit&&) noexcept;
		MetaUnit& operator=(MetaUnit&&) noexcept;
		~MetaUnit();
		static size_t calculateTypeMetaId(std::string_view name);

		[[nodiscard]] bool isValid() const;
		[[nodiscard]] size_t id() const;
		[[nodiscard]] const std::string& name() const;
		[[nodiscard]] const std::string& version() const;
		[[nodiscard]] const std::vector<Decl*>& markedDeclarations() const;
		[[nodiscard]] const std::vector<Type*>& markedType() const;

		std::string toJson();
		static void fromJson(const std::string& json, MetaUnit& unit);

	private:
		std::unique_ptr<MetaUnitImpl> _impl;

		explicit MetaUnit(std::unique_ptr<MetaUnitImpl>);
	};
}
