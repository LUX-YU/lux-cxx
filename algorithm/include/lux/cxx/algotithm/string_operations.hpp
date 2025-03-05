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
#include <string>
#include <string_view>

namespace lux::cxx::algorithm
{
	static inline std::string_view trim(std::string_view text) {
		size_t start = 0;
		size_t end = text.size();

		while (start < end && std::isspace(text[start])) {
			start++;
		}

		while (end > start && std::isspace(text[end - 1])) {
			end--;
		}

		return text.substr(start, end - start);
	}

	static inline std::string replace(std::string_view str, std::string_view old_value, std::string_view new_value) {
		std::string result(str);
		size_t pos = 0;
		while ((pos = result.find(old_value, pos)) != std::string::npos) {
			result.replace(pos, old_value.size(), new_value);
			pos += new_value.size();
		}
		return result;
	}
}
