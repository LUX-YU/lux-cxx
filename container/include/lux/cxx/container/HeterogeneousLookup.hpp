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
#include <unordered_map>
#include <string_view>

namespace lux::cxx
{
    template<typename ... Bases>
    struct HeterogeneousLookupBase : Bases ...
    {
        using is_transparent = void;
        using Bases::operator() ...;
    };

    struct char_pointer_hash
    {
        auto operator()(const char* ptr) const noexcept
        {
            return std::hash<std::string_view>{}(ptr);
        }
    };

    using transparent_string_hash = HeterogeneousLookupBase<
        std::hash<std::string>,
        std::hash<std::string_view>,
        char_pointer_hash
    >;

    template<typename T> using heterogeneous_map =
    std::unordered_map <
        std::string,
        T,
        transparent_string_hash,
        std::equal_to<>
    >;
}