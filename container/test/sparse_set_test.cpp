#include <iostream>
#include <unordered_map>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>

// Include your OffsetSparseSet header here:
#include <lux/cxx/container/SparseSet.hpp>

/**
 * @brief Alias for convenience.
 *        Adjust Key, Value, and Offset as needed.
 */
using MyKey = std::size_t;
using MyValue = std::size_t;
static constexpr MyKey kOffset = 1000;

// Concrete type aliases
using MyOffsetSparseSet = lux::cxx::OffsetSparseSet<MyKey, MyValue, kOffset>;
using MyOffsetAutoSparseSet = lux::cxx::OffsetAutoSparseSet<MyKey, MyValue, kOffset>;

/**
 * @brief Generate random keys in the range [min_val, max_val].
 * @param N         Number of keys to generate
 * @param min_val   Minimum possible key
 * @param max_val   Maximum possible key
 * @return          A vector of randomly generated keys
 */
static std::vector<MyKey> generateRandomKeys(std::size_t N, MyKey min_val, MyKey max_val) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<MyKey> dist(min_val, max_val);

    std::vector<MyKey> keys(N);
    for (std::size_t i = 0; i < N; ++i) {
        keys[i] = dist(gen);
    }
    return keys;
}

/**
 * @brief Measure average time (in nanoseconds) for inserting N items into OffsetAutoSparseSet.
 */
double measureInsert_OffsetAutoSparseSet(std::size_t N, int num_repeats) {
    // We'll just have some dummy values to insert (value = i).
    // The container automatically assigns keys.
    std::vector<MyValue> values(N);
    for (std::size_t i = 0; i < N; ++i) {
        values[i] = i;
    }

    double total_time_ns = 0.0;
    for (int r = 0; r < num_repeats; ++r) {
        MyOffsetAutoSparseSet autoSet;
        autoSet.reserve(N);

        auto start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            autoSet.insert(values[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_time_ns += static_cast<double>(duration);
    }
    return total_time_ns / static_cast<double>(num_repeats);
}

/**
 * @brief Measure average time (in nanoseconds) for inserting N items into std::unordered_map.
 */
double measureInsert_UnorderedMap(std::size_t N, int num_repeats) {
    // Generate the same random keys as for OffsetSparseSet
    auto keys = generateRandomKeys(N, kOffset, 2 * kOffset + 1000000);

    double total_time_ns = 0.0;
    for (int r = 0; r < num_repeats; ++r) {
        std::unordered_map<MyKey, MyValue> umap;
        umap.reserve(N);

        auto start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            umap.emplace(keys[i], i);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_time_ns += static_cast<double>(duration);
    }
    return total_time_ns / static_cast<double>(num_repeats);
}

/**
 * @brief Measure average time for finding N keys in OffsetAutoSparseSet.
 */
double measureFind_OffsetAutoSparseSet(std::size_t N, int num_repeats) {
    MyOffsetAutoSparseSet autoSet;
    autoSet.reserve(N);

    // Insert N values, keep track of the assigned keys
    std::vector<MyKey> assigned_keys(N);
    for (std::size_t i = 0; i < N; ++i) {
        assigned_keys[i] = autoSet.insert(i);
    }
    // Shuffle the assigned keys for random search order
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::shuffle(assigned_keys.begin(), assigned_keys.end(), gen);

    double total_time_ns = 0.0;
    for (int r = 0; r < num_repeats; ++r) {
        auto start = std::chrono::high_resolution_clock::now();
        std::size_t found_count = 0;
        for (auto k : assigned_keys) {
            if (autoSet.contains(k)) {
                ++found_count;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        (void)found_count;

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_time_ns += static_cast<double>(duration);
    }
    return total_time_ns / static_cast<double>(num_repeats);
}

/**
 * @brief Measure average time for finding N random keys in std::unordered_map.
 */
double measureFind_UnorderedMap(std::size_t N, int num_repeats) {
    auto keys = generateRandomKeys(N, kOffset, 2 * kOffset + 1000000);
    std::unordered_map<MyKey, MyValue> umap;
    umap.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        umap.emplace(keys[i], i);
    }
    // Generate keys to search
    auto search_keys = generateRandomKeys(N, kOffset, 2 * kOffset + 1000000);

    double total_time_ns = 0.0;
    for (int r = 0; r < num_repeats; ++r) {
        auto start = std::chrono::high_resolution_clock::now();
        std::size_t found_count = 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (umap.find(search_keys[i]) != umap.end()) {
                ++found_count;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        (void)found_count;

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_time_ns += static_cast<double>(duration);
    }
    return total_time_ns / static_cast<double>(num_repeats);
}

/**
 * @brief Measure average time for erasing N items from OffsetAutoSparseSet.
 */
double measureErase_OffsetAutoSparseSet(std::size_t N, int num_repeats) {
    double total_time_ns = 0.0;
    for (int r = 0; r < num_repeats; ++r) {
        MyOffsetAutoSparseSet autoSet;
        autoSet.reserve(N);

        std::vector<MyKey> assigned_keys(N);
        // Insert
        for (std::size_t i = 0; i < N; ++i) {
            assigned_keys[i] = autoSet.insert(i);
        }

        // Erase
        auto start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            autoSet.erase(assigned_keys[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_time_ns += static_cast<double>(duration);
    }
    return total_time_ns / static_cast<double>(num_repeats);
}

/**
 * @brief Measure average time for erasing N items from std::unordered_map.
 */
double measureErase_UnorderedMap(std::size_t N, int num_repeats) {
    auto keys = generateRandomKeys(N, kOffset, 2 * kOffset + 1000000);

    double total_time_ns = 0.0;
    for (int r = 0; r < num_repeats; ++r) {
        std::unordered_map<MyKey, MyValue> umap;
        umap.reserve(N);
        for (std::size_t i = 0; i < N; ++i) {
            umap.emplace(keys[i], i);
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            umap.erase(keys[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_time_ns += static_cast<double>(duration);
    }
    return total_time_ns / static_cast<double>(num_repeats);
}


int main() {
    // We will test for N in [2^10, 2^20]
    // And repeat each measurement multiple times to get an average
    const int num_repeats = 5;
    std::cout << "Performance test for OffsetSparseSet, OffsetAutoSparseSet, and std::unordered_map.\n";
    std::cout << "All times shown are average nanoseconds for the entire operation batch (not per element).\n";
    std::cout << "(Repeats per test: " << num_repeats << ")\n\n";

    std::cout << "N, Container, Insert(ns), Find(ns), Erase(ns)\n";

    for (std::size_t N = (1 << 10); N <= (1 << 20); N <<= 1) {
        // OffsetAutoSparseSet
        double t_insert_aset = measureInsert_OffsetAutoSparseSet(N, num_repeats);
        double t_find_aset = measureFind_OffsetAutoSparseSet(N, num_repeats);
        double t_erase_aset = measureErase_OffsetAutoSparseSet(N, num_repeats);
        std::cout << N << ", OffsetAutoSparseSet, "
            << t_insert_aset << ", "
            << t_find_aset << ", "
            << t_erase_aset << "\n";

        // std::unordered_map
        double t_insert_map = measureInsert_UnorderedMap(N, num_repeats);
        double t_find_map = measureFind_UnorderedMap(N, num_repeats);
        double t_erase_map = measureErase_UnorderedMap(N, num_repeats);
        std::cout << N << ", std::unordered_map, "
            << t_insert_map << ", "
            << t_find_map << ", "
            << t_erase_map << "\n";
    }

    return 0;
}
