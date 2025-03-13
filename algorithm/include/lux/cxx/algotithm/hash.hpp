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
#include <string_view>
#include <cstdint>

namespace lux::cxx::algorithm
{
    static constexpr inline size_t fnv1a(std::string_view text) {
        // 32 bit params
        // uint32_t constexpr fnv_prime = 16777619U;
        // uint32_t constexpr fnv_offset_basis = 2166136261U;
        constexpr uint64_t fnv_prime        = 1099511628211ULL;
        constexpr uint64_t fnv_offset_basis = 14695981039346656037ULL;

        uint64_t hash = fnv_offset_basis;

        for (auto c : text) {
            hash ^= c;
            hash *= fnv_prime;
        }

        return hash;
    }

    template<size_t ArrSize>
    consteval size_t TFnv1a(const char(&char_arry)[ArrSize]) {
        constexpr std::size_t fnv_prime = 1099511628211ULL;
        constexpr std::size_t fnv_offset_basis = 14695981039346656037ULL;
        std::size_t hash = fnv_offset_basis;
        for (auto c : char_arry) {
            hash ^= c;
            hash *= fnv_prime;
        }
        return hash;
    }

    template<size_t ArrSize>
    consteval size_t THash(const char(&char_array)[ArrSize])
    {
        return TFnv1a<ArrSize>(char_array);
    }
}