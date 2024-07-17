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

	static inline std::string_view trim(std::string text) {
		return trim(std::string_view(text));
	}
}
