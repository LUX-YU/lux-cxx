// ============================================================================
// small_vector_test.cpp
// ----------------------------------------------------------------------------
// Focused tests for lux::cxx::SmallVector, in particular the make_gap()
// middle-insert path for non-trivially-copyable element types.
//
// Regression: make_gap() used to destroy [pos, pos+tail) on the success path.
// When the inserted count < tail, the freshly move-constructed tail elements at
// [pos+count, pos+tail) were among those destroyed, leaving destroyed-but-counted
// slots that were then destroyed again at container teardown (double-destroy).
// The Tracked live-counter below goes non-zero (negative) if that regression
// returns, so these asserts pin the fix.
// ============================================================================
#include <lux/cxx/container/SmallVector.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>

namespace
{
    struct Tracked
    {
        int v;
        static inline int live = 0;

        Tracked(int x = 0) : v(x) { ++live; }
        Tracked(const Tracked& o) : v(o.v) { ++live; }
        Tracked(Tracked&& o) noexcept : v(o.v) { ++live; o.v = -1; }
        Tracked& operator=(const Tracked& o) { v = o.v; return *this; }
        Tracked& operator=(Tracked&& o) noexcept { v = o.v; o.v = -1; return *this; }
        ~Tracked() { --live; }
    };

    struct alignas(64) OverAligned final
    {
        int value = 0;
    };

    struct CopyFailure final
    {
    };

    struct ThrowingCopy final
    {
        static inline int copies_before_throw = -1;
        int value = 0;

        explicit ThrowingCopy(int input = 0) noexcept
            : value(input)
        {
        }

        ThrowingCopy(const ThrowingCopy& other)
            : value(other.value)
        {
            if (copies_before_throw == 0) throw CopyFailure{};
            if (copies_before_throw > 0) --copies_before_throw;
        }

        ThrowingCopy(ThrowingCopy&& other) noexcept(false)
            : value(other.value)
        {
        }

        ThrowingCopy& operator=(const ThrowingCopy&) = default;
        ThrowingCopy& operator=(ThrowingCopy&&) = default;
    };
}

int main()
{
    using lux::cxx::SmallVector;

    // 1. Single-element front/middle insert (count == 1, tail >= 2).
    {
        SmallVector<Tracked, 16> v;     // inline cap 16 → no reallocation
        v.push_back(Tracked(10));
        v.push_back(Tracked(20));
        v.push_back(Tracked(30));

        v.insert(v.begin(), Tracked(5));          // idx=0, count=1, tail=3
        assert(v.size() == 4);
        {
            const int expect[] = { 5, 10, 20, 30 };
            for (std::size_t i = 0; i < v.size(); ++i) assert(v[i].v == expect[i]);
        }

        v.insert(v.begin() + 2, Tracked(15));     // middle insert, tail=2
        assert(v.size() == 5);
        {
            const int expect[] = { 5, 10, 15, 20, 30 };
            for (std::size_t i = 0; i < v.size(); ++i) assert(v[i].v == expect[i]);
        }
    }
    assert(Tracked::live == 0);   // balanced: no leak, no double-destroy

    // 2. Range insert in the middle with cnt < tail (the overlap case).
    {
        SmallVector<Tracked, 32> v;
        for (int x : { 10, 20, 30, 40, 50 }) v.push_back(Tracked(x));

        Tracked ins[] = { Tracked(91), Tracked(92) };
        v.insert(v.begin() + 1, ins, ins + 2);    // idx=1, cnt=2, tail=4 → cnt<tail
        assert(v.size() == 7);
        const int expect[] = { 10, 91, 92, 20, 30, 40, 50 };
        for (std::size_t i = 0; i < v.size(); ++i) assert(v[i].v == expect[i]);
    }
    assert(Tracked::live == 0);

    // 3. Insert at end (tail == 0) and trivially-copyable path sanity.
    {
        SmallVector<int, 8> iv;
        iv.push_back(1); iv.push_back(2); iv.push_back(3);
        iv.insert(iv.begin() + 1, 99);            // [1,99,2,3]
        const int expect[] = { 1, 99, 2, 3 };
        assert(iv.size() == 4);
        for (std::size_t i = 0; i < iv.size(); ++i) assert(iv[i] == expect[i]);
    }

    // 4. Inline storage honors over-alignment.
    {
        SmallVector<OverAligned, 2> aligned;
        aligned.emplace_back(OverAligned{42});
        assert(reinterpret_cast<std::uintptr_t>(aligned.data()) % 64 == 0);
        aligned.emplace_back(OverAligned{43});
        aligned.emplace_back(OverAligned{44});
        assert(reinterpret_cast<std::uintptr_t>(aligned.data()) % 64 == 0);
    }

    // 5. Move-only values work on inline and heap paths.
    {
        SmallVector<std::unique_ptr<int>, 2> values;
        values.push_back(std::make_unique<int>(1));
        values.push_back(std::make_unique<int>(2));
        values.push_back(std::make_unique<int>(3));
        assert(*values[0] == 1 && *values[1] == 2 && *values[2] == 3);
    }

    // 6. A failed copy during growth leaves the original inline values intact.
    {
        SmallVector<ThrowingCopy, 2> values;
        values.emplace_back(1);
        values.emplace_back(2);
        ThrowingCopy::copies_before_throw = 0;
        bool threw = false;
        try
        {
            values.emplace_back(3);
        }
        catch (const CopyFailure&)
        {
            threw = true;
        }
        ThrowingCopy::copies_before_throw = -1;
        assert(threw);
        assert(values.size() == 2);
        assert(values[0].value == 1 && values[1].value == 2);
    }

    std::cout << "SmallVector tests passed\n";
    return 0;
}
