// ============================================================================
// test_intrusive_ptr.cpp
// -----------------------------------------------------------------------------
// Minimal test‑drive for lux::cxx::intrusive_ptr
// Compile (GCC / Clang):
//     g++ -std=c++17 -pthread -O2 test_intrusive_ptr.cpp -o test_intrusive_ptr
// ----------------------------------------------------------------------------
#include <iostream>
#include <thread>
#include <vector>
#include <lux/cxx/memory/intrusive_ptr.hpp>

// A simple object that prints messages on construction / destruction so we can
// visually trace the reference‑counting behaviour.
struct Foo : lux::cxx::intrusive_ref_counter<Foo> {
    int value;
    explicit Foo(int v) : value(v) {
        std::cout << "Foo(" << value << ") constructed\n";
    }
    ~Foo() {
        std::cout << "Foo(" << value << ") destroyed\n";
    }
    void greet() const {
        std::cout << "Hello from Foo(" << value << ")\n";
    }
};

// Helper to exercise copy‑construction across threads — implicitly bumps the
// reference counter when passed by value.
void thread_func(lux::cxx::intrusive_ptr<Foo> p) {
    p->greet();
}

// ---------------------------------------------------------------------
// 4. Pointer casts (static / dynamic)
// ---------------------------------------------------------------------
struct Base : lux::cxx::intrusive_ref_counter<Base> { virtual ~Base() = default; };
struct Derived : Base { int extra = 99; };

static inline void intrusive_ptr_add_ref(Derived* p) noexcept {
    intrusive_ptr_add_ref(static_cast<Base*>(p));
}

static inline void intrusive_ptr_release(Derived* p) noexcept {
    intrusive_ptr_release(static_cast<Base*>(p));
}

int main() {
    using lux::cxx::intrusive_ptr;

    // ---------------------------------------------------------------------
    // 1. Basic lifetime
    // ---------------------------------------------------------------------
    intrusive_ptr<Foo> a(new Foo(42));     // ref = 1
    {
        intrusive_ptr<Foo> b = a;          // ref = 2
        intrusive_ptr<Foo> c = b;          // ref = 3
        c->greet();
    }                                      // c, b leave scope → ref back to 1

    // ---------------------------------------------------------------------
    // 2. Move semantics
    // ---------------------------------------------------------------------
    intrusive_ptr<Foo> d = std::move(a);   // a becomes nullptr, d owns (ref = 1)

    // ---------------------------------------------------------------------
    // 3. Multi‑threaded copies (atomic counter path)
    // ---------------------------------------------------------------------
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(thread_func, d);   // each copy ++ref
    }
    for (auto& t : threads) t.join();           // all copies --ref at join


    intrusive_ptr<Base> base(new Derived);      // ref = 1 (Base view)
    auto derived = lux::cxx::dynamic_pointer_cast<Derived>(base);
    if (derived) {
        std::cout << "dynamic_pointer_cast succeeded, extra=" << derived->extra << "\n";
    }

    // ---------------------------------------------------------------------
    // 5. Manual reset triggers destruction
    // ---------------------------------------------------------------------
    d.reset();          // ref reaches 0 here → Foo(42) destroyed
    base.reset();       // ref reaches 0 for Derived/Base instance

    std::cout << "All done. Press <Enter> to exit..." << std::endl;
    std::cin.get();
    return 0;
}
// ============================================================================
