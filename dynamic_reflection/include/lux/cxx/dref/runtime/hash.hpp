#pragma once
#include <string_view>

namespace lux::cxx::dref::runtime
{
    static inline size_t fnv1a(std::string_view text) {
        // 32 bit params
        // uint32_t constexpr fnv_prime = 16777619U;
        // uint32_t constexpr fnv_offset_basis = 2166136261U;
        uint64_t constexpr fnv_prime = 1099511628211ULL;
        uint64_t constexpr fnv_offset_basis = 14695981039346656037ULL;

        uint64_t hash = fnv_offset_basis;

        for (auto c : text) {
            hash ^= c;
            hash *= fnv_prime;
        }

        return hash;
    }

    template<size_t ArrSize>
    consteval size_t TFnv1a(const char (&char_arry)[ArrSize]) {
        constexpr std::size_t fnv_prime         = 1099511628211ULL;
        constexpr std::size_t fnv_offset_basis  = 14695981039346656037ULL;
        std::size_t hash = fnv_offset_basis;
        for (auto c : char_arry) {
            hash ^= c;
            hash *= fnv_prime;
        }
        return hash;
    }

    template<size_t ArrSize>
    consteval size_t TFnv1a(std::string_view char_arry) {
        constexpr std::size_t fnv_prime = 1099511628211ULL;
        constexpr std::size_t fnv_offset_basis = 14695981039346656037ULL;
        std::size_t hash = fnv_offset_basis;
        for (auto c : char_arry) {
            hash ^= c;
            hash *= fnv_prime;
        }
        return hash;
    }
}