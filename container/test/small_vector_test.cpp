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
#include <iostream>

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

    std::cout << "SmallVector tests passed\n";
    return 0;
}
