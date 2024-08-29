#pragma once
#include <string_view>
#include <cstdint>

namespace lux::cxx::algorithm
{
    static inline size_t fnv1a(std::string_view text) {
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