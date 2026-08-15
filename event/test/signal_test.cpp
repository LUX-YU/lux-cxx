// Signal<E> unit tests
// Tests: Phase 1 (subscribe, emit, priority, reentry, RAII),
//        Phase 2 (SlotCallback, Connection, connect(), lambda),
//        Phase 3 (ThreadSafeSignal),
//        Phase 4 (InterceptableSignal, FilteredSignal),
//        Phase 5 (Trackable).

#include <lux/cxx/event/Signal.hpp>
#include <lux/cxx/event/Registry.hpp>
#include <lux/cxx/event/Connect.hpp>
#include <lux/cxx/event/ThreadSafeSignal.hpp>
#include <lux/cxx/event/EventResult.hpp>
#include <lux/cxx/event/InterceptableSignal.hpp>
#include <lux/cxx/event/FilteredSignal.hpp>
#include <lux/cxx/event/Trackable.hpp>

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <numeric>

// ── Test Events ──────────────────────────────────────────────

struct IntEvent
{
    int value;
};

struct FloatEvent
{
    float x, y;
};

struct EmptyEvent
{
};

// ── Helpers ──────────────────────────────────────────────────

static int g_call_count = 0;
static int g_last_value = 0;

static void free_callback(void * /*ctx*/, const IntEvent &e)
{
    ++g_call_count;
    g_last_value = e.value;
}

// ── Tests ────────────────────────────────────────────────────

static void test_basic_subscribe_emit()
{
    std::puts("  test_basic_subscribe_emit");

    lux::cxx::event::Signal<IntEvent> sig;

    g_call_count = 0;
    g_last_value = 0;

    auto sub = sig.subscribe(nullptr, &free_callback);
    assert(sig.subscriber_count() == 1);

    sig.emit({42});
    assert(g_call_count == 1);
    assert(g_last_value == 42);

    sig.emit({99});
    assert(g_call_count == 2);
    assert(g_last_value == 99);
}

static void test_unsubscribe_via_reset()
{
    std::puts("  test_unsubscribe_via_reset");

    lux::cxx::event::Signal<IntEvent> sig;
    g_call_count = 0;

    auto sub = sig.subscribe(nullptr, &free_callback);
    sig.emit({1});
    assert(g_call_count == 1);

    sub.reset();
    assert(!sub.active());
    assert(sig.subscriber_count() == 0);

    sig.emit({2});
    assert(g_call_count == 1); // not called again
}

static void test_unsubscribe_via_destructor()
{
    std::puts("  test_unsubscribe_via_destructor");

    lux::cxx::event::Signal<IntEvent> sig;
    g_call_count = 0;

    {
        auto sub = sig.subscribe(nullptr, &free_callback);
        sig.emit({1});
        assert(g_call_count == 1);
    }
    // sub destroyed here

    assert(sig.subscriber_count() == 0);
    sig.emit({2});
    assert(g_call_count == 1);
}

static void test_multiple_subscribers()
{
    std::puts("  test_multiple_subscribers");

    lux::cxx::event::Signal<IntEvent> sig;
    std::vector<int> results;

    auto cb = [](void *ctx, const IntEvent &e)
    {
        static_cast<std::vector<int> *>(ctx)->push_back(e.value);
    };

    auto s1 = sig.subscribe(&results, +cb);
    auto s2 = sig.subscribe(&results, +cb);
    auto s3 = sig.subscribe(&results, +cb);

    sig.emit({7});
    assert(results.size() == 3);
    assert(results[0] == 7 && results[1] == 7 && results[2] == 7);
}

static void test_priority_ordering()
{
    std::puts("  test_priority_ordering");

    lux::cxx::event::Signal<IntEvent> sig;
    std::vector<int> order;

    auto cb = [](void *ctx, const IntEvent &e)
    {
        static_cast<std::vector<int> *>(ctx)->push_back(e.value);
    };

    // Lower-priority subscriber first, higher-priority later
    auto s1 = sig.subscribe(&order, +[](void *ctx, const IntEvent &)
                                    { static_cast<std::vector<int> *>(ctx)->push_back(1); },
                            /*priority=*/0);

    auto s2 = sig.subscribe(&order, +[](void *ctx, const IntEvent &)
                                    { static_cast<std::vector<int> *>(ctx)->push_back(2); },
                            /*priority=*/10);

    auto s3 = sig.subscribe(&order, +[](void *ctx, const IntEvent &)
                                    { static_cast<std::vector<int> *>(ctx)->push_back(3); },
                            /*priority=*/-5);

    sig.emit({0});

    // Expected order: priority 10 (s2=2), priority 0 (s1=1), priority -5 (s3=3)
    assert(order.size() == 3);
    assert(order[0] == 2);
    assert(order[1] == 1);
    assert(order[2] == 3);
}

static void test_member_function_subscribe()
{
    std::puts("  test_member_function_subscribe");

    struct Listener
    {
        int received = 0;
        void on_int(const IntEvent &e) { received = e.value; }
    };

    lux::cxx::event::Signal<IntEvent> sig;
    Listener listener;

    auto sub = sig.subscribe<&Listener::on_int>(&listener);
    sig.emit({123});
    assert(listener.received == 123);
}

static void test_empty_signal()
{
    std::puts("  test_empty_signal");

    lux::cxx::event::Signal<IntEvent> sig;
    assert(sig.empty());
    assert(sig.subscriber_count() == 0);

    // Emit on empty signal should be a no-op
    sig.emit({42});
}

static void test_scoped_subscription_move()
{
    std::puts("  test_scoped_subscription_move");

    lux::cxx::event::Signal<IntEvent> sig;
    g_call_count = 0;

    lux::cxx::event::ScopedSubscription outer;

    {
        auto inner = sig.subscribe(nullptr, &free_callback);
        outer = std::move(inner);
        assert(!inner.active());
        assert(outer.active());
    }
    // inner destroyed, but ownership has moved to outer
    assert(sig.subscriber_count() == 1);

    sig.emit({1});
    assert(g_call_count == 1);

    outer.reset();
    assert(sig.subscriber_count() == 0);
}

static void test_subscription_group()
{
    std::puts("  test_subscription_group");

    lux::cxx::event::Signal<IntEvent> sig;
    lux::cxx::event::Signal<FloatEvent> sig2;
    g_call_count = 0;

    lux::cxx::event::SubscriptionGroup group;

    group.add(sig.subscribe(nullptr, &free_callback));
    group.add(sig2.subscribe(nullptr, +[](void *, const FloatEvent &)
                                      { ++g_call_count; }));

    sig.emit({1});
    sig2.emit({1.f, 2.f});
    assert(g_call_count == 2);

    group.clear(); // unsubscribes all
    assert(sig.subscriber_count() == 0);
    assert(sig2.subscriber_count() == 0);

    sig.emit({2});
    sig2.emit({3.f, 4.f});
    assert(g_call_count == 2); // no new calls
}

static void test_unsubscribe_during_emit()
{
    std::puts("  test_unsubscribe_during_emit");

    lux::cxx::event::Signal<IntEvent> sig;
    int call_count_a = 0;
    int call_count_b = 0;

    lux::cxx::event::ScopedSubscription sub_b;

    // Subscriber A: unsubscribes B during emit
    struct Ctx
    {
        int *count_a;
        int *count_b;
        lux::cxx::event::ScopedSubscription *sub_b_ptr;
    };

    Ctx ctx{&call_count_a, &call_count_b, &sub_b};

    auto sub_a = sig.subscribe(&ctx, 
        +[](void *raw, const IntEvent &)
        {
            auto *c = static_cast<Ctx *>(raw);
            ++(*c->count_a);
            c->sub_b_ptr->reset(); // unsubscribe B during emit
        },
        /*priority=*/10
    );

    sub_b = sig.subscribe(&call_count_b, +[](void *raw, const IntEvent &)
                                         { ++(*static_cast<int *>(raw)); },
                          /*priority=*/0);

    sig.emit({1});
    assert(call_count_a == 1);
    // B should NOT have been called: it was unsubscribed before its turn
    // (A has higher priority, so it runs first and unsubscribes B)
    assert(call_count_b == 0);
    assert(sig.subscriber_count() == 1);
}

static void test_subscribe_during_emit()
{
    std::puts("  test_subscribe_during_emit");

    lux::cxx::event::Signal<IntEvent> sig;
    int original_calls = 0;
    int new_calls = 0;

    lux::cxx::event::ScopedSubscription new_sub;

    struct Ctx
    {
        lux::cxx::event::Signal<IntEvent> *sig;
        int *original_calls;
        int *new_calls;
        lux::cxx::event::ScopedSubscription *new_sub;
        bool subscribed;
    };

    Ctx ctx{&sig, &original_calls, &new_calls, &new_sub, false};

    auto sub = sig.subscribe(&ctx, +[](void *raw, const IntEvent &)
                                   {
            auto* c = static_cast<Ctx*>(raw);
            ++(*c->original_calls);

            if (!c->subscribed)
            {
                c->subscribed = true;
                *c->new_sub = c->sig->subscribe(c->new_calls,
                    +[](void* raw2, const IntEvent&) {
                        ++(*static_cast<int*>(raw2));
                    });
            } });

    // First emit: original fires, subscribes new (new doesn't fire this emit)
    sig.emit({1});
    assert(original_calls == 1);
    assert(new_calls == 0); // new subscriber should NOT fire during this emit

    // Second emit: both fire
    sig.emit({2});
    assert(original_calls == 2);
    assert(new_calls == 1);
}

static void test_recursive_emit()
{
    std::puts("  test_recursive_emit");

    lux::cxx::event::Signal<IntEvent> sig;
    std::vector<int> received;

    struct Ctx
    {
        lux::cxx::event::Signal<IntEvent> *sig;
        std::vector<int> *received;
    };

    Ctx ctx{&sig, &received};

    auto sub = sig.subscribe(&ctx, +[](void *raw, const IntEvent &e)
                                   {
            auto* c = static_cast<Ctx*>(raw);
            c->received->push_back(e.value);

            // Recursive emit with decremented value
            if (e.value > 0)
            {
                c->sig->emit({e.value - 1});
            } });

    sig.emit({3});

    // Should see: 3, 2, 1, 0
    assert(received.size() == 4);
    assert(received[0] == 3);
    assert(received[1] == 2);
    assert(received[2] == 1);
    assert(received[3] == 0);
}

// Regression: a callback that unsubscribes another slot (marking the signal
// dirty) and THEN triggers a nested emit on the same signal. Pre-fix, the nested
// emit rebuilt sorted_keys_ (erase_if + sort) while the outer emit was still
// iterating it with a stale size → out-of-bounds read.
static void test_nested_emit_after_unsubscribe()
{
    std::puts("  test_nested_emit_after_unsubscribe");
    using namespace lux::cxx::event;

    Signal<IntEvent> sig;

    struct Ctx
    {
        Signal<IntEvent> *sig;
        ScopedSubscription *b;  // slot to unsubscribe mid-emit
        int a_calls = 0;
        int b_calls = 0;
        int c_calls = 0;
        bool nested = false;
    } ctx;
    ctx.sig = &sig;

    // C: lowest priority, just counts.
    auto subC = sig.subscribe(&ctx, +[](void *raw, const IntEvent &)
                              { static_cast<Ctx *>(raw)->c_calls++; }, /*priority*/ -10);

    // B: middle priority; will be unsubscribed by A during the outer emit.
    ScopedSubscription subB = sig.subscribe(&ctx, +[](void *raw, const IntEvent &)
                                            { static_cast<Ctx *>(raw)->b_calls++; }, /*priority*/ 0);
    ctx.b = &subB;

    // A: highest priority. On the outermost emit, unsubscribe B (sets the signal
    // dirty) then trigger a nested emit on the same signal.
    auto subA = sig.subscribe(&ctx, +[](void *raw, const IntEvent &)
                              {
        auto* c = static_cast<Ctx*>(raw);
        c->a_calls++;
        if (!c->nested)
        {
            c->nested = true;
            c->b->reset();          // unsubscribe B during emit -> sorted_dirty_
            c->sig->emit({999});    // nested emit on the same signal
        } }, /*priority*/ 10);

    sig.emit({1});

    // A: once in the outer emit + once in the nested emit.
    assert(ctx.a_calls == 2);
    // B was unsubscribed before the nested emit and must never fire afterwards.
    assert(ctx.b_calls == 0);
    // C: once per emit (outer + nested).
    assert(ctx.c_calls == 2);

    (void)subA;
    (void)subC;
}

static void test_event_type_id()
{
    std::puts("  test_event_type_id");

    // Different types should have different IDs
    auto id1 = lux::cxx::event::kEventTypeId<IntEvent>;
    auto id2 = lux::cxx::event::kEventTypeId<FloatEvent>;
    auto id3 = lux::cxx::event::kEventTypeId<EmptyEvent>;

    assert(id1 != id2);
    assert(id1 != id3);
    assert(id2 != id3);

    // Same type should always give the same ID
    assert(id1 == lux::cxx::event::kEventTypeId<IntEvent>);
}

static void test_event_traits()
{
    std::puts("  test_event_traits");

    // Default traits: only allow_cross_thread remains
    static_assert(lux::cxx::event::EventTraits<IntEvent>::allow_cross_thread == true);
    static_assert(lux::cxx::event::EventTraits<std::string>::allow_cross_thread == false);
}

static void test_concepts()
{
    std::puts("  test_concepts");

    static_assert(lux::cxx::event::Event<IntEvent>);
    static_assert(lux::cxx::event::Event<FloatEvent>);
    static_assert(lux::cxx::event::Event<std::string>);

    static_assert(lux::cxx::event::InlineEvent<IntEvent>);
    static_assert(lux::cxx::event::InlineEvent<FloatEvent>);
    static_assert(!lux::cxx::event::InlineEvent<std::string>); // not trivially copyable

    static_assert(lux::cxx::event::CrossThreadEvent<IntEvent>);
    static_assert(!lux::cxx::event::CrossThreadEvent<std::string>);
}

static void test_registry()
{
    std::puts("  test_registry");

    lux::cxx::event::EventTypeRegistry::register_type<IntEvent>("IntEvent");
    lux::cxx::event::EventTypeRegistry::register_type<FloatEvent>("FloatEvent");

    auto *meta = lux::cxx::event::EventTypeRegistry::find(lux::cxx::event::kEventTypeId<IntEvent>);
    assert(meta != nullptr);
    assert(meta->name == "IntEvent");
    assert(meta->size == sizeof(IntEvent));
    assert(meta->trivially_copyable == true);

    auto *meta2 = lux::cxx::event::EventTypeRegistry::find(lux::cxx::event::kEventTypeId<FloatEvent>);
    assert(meta2 != nullptr);
    assert(meta2->name == "FloatEvent");

    // Unknown type
    assert(lux::cxx::event::EventTypeRegistry::find(0) == nullptr);

    // All registered
    auto all = lux::cxx::event::EventTypeRegistry::all();
    assert(all.size() >= 2);

    // Double registration is idempotent
    auto size_before = all.size();
    lux::cxx::event::EventTypeRegistry::register_type<IntEvent>("IntEvent");
    assert(lux::cxx::event::EventTypeRegistry::all().size() == size_before);
}

static void test_empty_event()
{
    std::puts("  test_empty_event");

    lux::cxx::event::Signal<EmptyEvent> sig;
    int count = 0;

    auto sub = sig.subscribe(&count, +[](void *ctx, const EmptyEvent &)
                                     { ++(*static_cast<int *>(ctx)); });

    sig.emit({});
    assert(count == 1);
}

static void test_stable_priority_order()
{
    std::puts("  test_stable_priority_order");

    lux::cxx::event::Signal<IntEvent> sig;
    std::vector<int> order;

    // Subscribe 3 handlers at same priority — should fire in registration order
    auto s1 = sig.subscribe(&order, +[](void *ctx, const IntEvent &)
                                    { static_cast<std::vector<int> *>(ctx)->push_back(1); },
                            /*priority=*/0);

    auto s2 = sig.subscribe(&order, +[](void *ctx, const IntEvent &)
                                    { static_cast<std::vector<int> *>(ctx)->push_back(2); },
                            /*priority=*/0);

    auto s3 = sig.subscribe(&order, +[](void *ctx, const IntEvent &)
                                    { static_cast<std::vector<int> *>(ctx)->push_back(3); },
                            /*priority=*/0);

    sig.emit({0});
    assert(order.size() == 3);
    assert(order[0] == 1);
    assert(order[1] == 2);
    assert(order[2] == 3);
}

// ══════════════════════════════════════════════════════════════
// Phase 2 Tests: SlotCallback, Connection, connect(), lambda
// ══════════════════════════════════════════════════════════════

static void test_slot_callback_raw_fn()
{
    std::puts("  test_slot_callback_raw_fn");

    int result = 0;
    lux::cxx::event::SlotCallback<IntEvent> cb{
        &result,
        +[](void *ctx, const IntEvent &e)
        { *static_cast<int *>(ctx) = e.value; }};

    cb(IntEvent{42});
    assert(result == 42);
}

static void test_slot_callback_lambda()
{
    std::puts("  test_slot_callback_lambda");

    int result = 0;
    lux::cxx::event::SlotCallback<IntEvent> cb{
        [&result](const IntEvent &e)
        { result = e.value; }};

    cb(IntEvent{77});
    assert(result == 77);
}

static void test_slot_callback_move()
{
    std::puts("  test_slot_callback_move");

    int result = 0;
    lux::cxx::event::SlotCallback<IntEvent> cb{
        [&result](const IntEvent &e)
        { result = e.value; }};

    lux::cxx::event::SlotCallback<IntEvent> cb2 = std::move(cb);
    cb2(IntEvent{55});
    assert(result == 55);
}

static void test_signal_subscribe_lambda()
{
    std::puts("  test_signal_subscribe_lambda");

    lux::cxx::event::Signal<IntEvent> sig;
    int received = 0;

    auto sub = sig.subscribe(
        lux::cxx::event::SlotCallback<IntEvent>{[&received](const IntEvent &e)
                                                { received = e.value; }});

    sig.emit({123});
    assert(received == 123);
}

static void test_signal_connect_lambda()
{
    std::puts("  test_signal_connect_lambda");

    lux::cxx::event::Signal<IntEvent> sig;
    int received = 0;

    auto conn = sig.connect(
        lux::cxx::event::SlotCallback<IntEvent>{[&received](const IntEvent &e)
                                                { received = e.value; }});

    assert(conn.connected());
    sig.emit({456});
    assert(received == 456);

    conn.disconnect();
    assert(!conn.connected());

    sig.emit({789});
    assert(received == 456); // not updated
}

static void test_free_connect_lambda()
{
    std::puts("  test_free_connect_lambda");

    lux::cxx::event::Signal<IntEvent> sig;
    int received = 0;

    auto conn = lux::cxx::event::connect(sig, [&received](const IntEvent &e)
                                         { received = e.value; });

    sig.emit({100});
    assert(received == 100);
}

static void test_free_connect_member()
{
    std::puts("  test_free_connect_member");

    struct Listener
    {
        int value = 0;
        void onInt(const IntEvent &e) { value = e.value; }
    };

    lux::cxx::event::Signal<IntEvent> sig;
    Listener listener;

    auto conn = lux::cxx::event::connect<&Listener::onInt>(sig, &listener);

    sig.emit({200});
    assert(listener.value == 200);
}

static void test_connection_group()
{
    std::puts("  test_connection_group");

    lux::cxx::event::Signal<IntEvent> sig;
    lux::cxx::event::Signal<FloatEvent> sig2;
    int count = 0;

    lux::cxx::event::ConnectionGroup group;

    group.add(lux::cxx::event::connect(sig, [&count](const IntEvent &)
                                       { ++count; }));
    group.add(lux::cxx::event::connect(sig2, [&count](const FloatEvent &)
                                       { ++count; }));

    sig.emit({1});
    sig2.emit({1.0f, 2.0f});
    assert(count == 2);

    group.disconnect_all();
    assert(sig.subscriber_count() == 0);
    assert(sig2.subscriber_count() == 0);

    sig.emit({2});
    sig2.emit({3.0f, 4.0f});
    assert(count == 2); // no new calls
}

static void test_scoped_connection_raii()
{
    std::puts("  test_scoped_connection_raii");

    lux::cxx::event::Signal<IntEvent> sig;
    int received = 0;

    {
        auto conn = lux::cxx::event::connect(sig, [&received](const IntEvent &e)
                                             { received = e.value; });
        sig.emit({10});
        assert(received == 10);
    }
    // conn destroyed → disconnected

    assert(sig.subscriber_count() == 0);
    sig.emit({20});
    assert(received == 10); // unchanged
}

// ══════════════════════════════════════════════════════════════
// Phase 3 Tests: ThreadSafeSignal
// ══════════════════════════════════════════════════════════════

static void test_thread_safe_signal_basic()
{
    std::puts("  test_thread_safe_signal_basic");

    lux::cxx::event::ThreadSafeSignal<IntEvent> sig;
    std::atomic<int> sum{0};

    auto sub = sig.subscribe(
        lux::cxx::event::SlotCallback<IntEvent>{[&sum](const IntEvent &e)
                                                { sum.fetch_add(e.value, std::memory_order_relaxed); }});

    sig.emit({10});
    assert(sum.load() == 10);
}

static void test_thread_safe_signal_concurrent_emit()
{
    std::puts("  test_thread_safe_signal_concurrent_emit");

    lux::cxx::event::ThreadSafeSignal<IntEvent> sig;
    std::atomic<int> count{0};

    auto sub = sig.subscribe(
        lux::cxx::event::SlotCallback<IntEvent>{[&count](const IntEvent &)
                                                { count.fetch_add(1, std::memory_order_relaxed); }});

    constexpr int kThreads = 4;
    constexpr int kPerThread = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&sig]()
                             {
            for (int i = 0; i < kPerThread; ++i)
                sig.emit({1}); });
    }

    for (auto &th : threads)
        th.join();

    assert(count.load() == kThreads * kPerThread);
}

// Unsubscribing one of several subscribers must remove exactly that one. This
// exercises the full 64-bit id pack/unpack through the SubscriptionHandle.
static void test_thread_safe_unsubscribe_correct()
{
    std::puts("  test_thread_safe_unsubscribe_correct");

    lux::cxx::event::ThreadSafeSignal<IntEvent> sig;
    std::atomic<int> a{0}, b{0}, c{0};

    auto sa = sig.subscribe(lux::cxx::event::SlotCallback<IntEvent>{
        [&a](const IntEvent &) { a.fetch_add(1, std::memory_order_relaxed); }});
    auto sb = sig.subscribe(lux::cxx::event::SlotCallback<IntEvent>{
        [&b](const IntEvent &) { b.fetch_add(1, std::memory_order_relaxed); }});
    auto sc = sig.subscribe(lux::cxx::event::SlotCallback<IntEvent>{
        [&c](const IntEvent &) { c.fetch_add(1, std::memory_order_relaxed); }});

    sig.emit({1});
    assert(a.load() == 1 && b.load() == 1 && c.load() == 1);

    sb.reset();              // unsubscribe only B
    sig.emit({1});
    assert(a.load() == 2 && b.load() == 1 && c.load() == 2);  // B unchanged
}

// Stress: a background thread emits continuously while the main thread keeps
// subscribing and unsubscribing. After unsubscribe() returns, the callback (and
// the captured object) must never be touched again — verified here by destroying
// the captured counter right after reset() and relying on ASan to catch any UAF.
static void test_thread_safe_concurrent_churn()
{
    std::puts("  test_thread_safe_concurrent_churn");

    lux::cxx::event::ThreadSafeSignal<IntEvent> sig;
    std::atomic<bool> stop{false};
    std::atomic<long long> total{0};

    // A permanent subscriber so the emitter always has work.
    auto perm = sig.subscribe(lux::cxx::event::SlotCallback<IntEvent>{
        [&total](const IntEvent &e) { total.fetch_add(e.value, std::memory_order_relaxed); }});

    std::thread emitter([&]
                        {
        while (!stop.load(std::memory_order_relaxed))
            sig.emit({1}); });

    // Do not let a very fast main thread stop the emitter before the scheduler
    // has run it at least once; that would test scheduling luck, not Signal.
    while (total.load(std::memory_order_relaxed) == 0)
    {
        std::this_thread::yield();
    }

    for (int i = 0; i < 2000; ++i)
    {
        // Heap-allocate the captured state so ASan flags any callback that runs
        // after unsubscribe() returned (which frees it).
        auto hits = std::make_unique<std::atomic<int>>(0);
        std::atomic<int> *raw = hits.get();
        auto sub = sig.subscribe(lux::cxx::event::SlotCallback<IntEvent>{
            [raw](const IntEvent &) { raw->fetch_add(1, std::memory_order_relaxed); }});
        sub.reset();          // must quiesce: no in-flight emit may use `raw` after this
        hits.reset();         // free the captured state; a late callback => UAF
    }

    stop.store(true, std::memory_order_relaxed);
    emitter.join();
    assert(total.load() > 0);
}

// ══════════════════════════════════════════════════════════════
// Phase 4 Tests: InterceptableSignal, FilteredSignal
// ══════════════════════════════════════════════════════════════

static void test_interceptable_signal_continue()
{
    std::puts("  test_interceptable_signal_continue");

    lux::cxx::event::InterceptableSignal<IntEvent> sig;
    std::vector<int> order;

    auto s1 = sig.subscribe(
        lux::cxx::event::InterceptableSlotCallback<IntEvent>{
            [&order](const IntEvent &e) -> lux::cxx::event::EventResult
            {
                order.push_back(1);
                return lux::cxx::event::EventResult::Continue;
            }},
        /*priority=*/10);

    auto s2 = sig.subscribe(
        lux::cxx::event::InterceptableSlotCallback<IntEvent>{
            [&order](const IntEvent &e) -> lux::cxx::event::EventResult
            {
                order.push_back(2);
                return lux::cxx::event::EventResult::Continue;
            }},
        /*priority=*/0);

    auto result = sig.emit({42});
    assert(result == lux::cxx::event::EventResult::Continue);
    assert(order.size() == 2);
    assert(order[0] == 1); // higher priority first
    assert(order[1] == 2);
}

static void test_interceptable_signal_handled()
{
    std::puts("  test_interceptable_signal_handled");

    lux::cxx::event::InterceptableSignal<IntEvent> sig;
    int low_priority_calls = 0;

    auto s1 = sig.subscribe(
        lux::cxx::event::InterceptableSlotCallback<IntEvent>{
            [](const IntEvent &) -> lux::cxx::event::EventResult
            {
                return lux::cxx::event::EventResult::Handled; // consume
            }},
        /*priority=*/10);

    auto s2 = sig.subscribe(
        lux::cxx::event::InterceptableSlotCallback<IntEvent>{
            [&low_priority_calls](const IntEvent &) -> lux::cxx::event::EventResult
            {
                ++low_priority_calls;
                return lux::cxx::event::EventResult::Continue;
            }},
        /*priority=*/0);

    auto result = sig.emit({1});
    assert(result == lux::cxx::event::EventResult::Handled);
    assert(low_priority_calls == 0); // never reached
}

static void test_interceptable_signal_member_fn()
{
    std::puts("  test_interceptable_signal_member_fn");

    struct Handler
    {
        int count = 0;
        lux::cxx::event::EventResult on_event(const IntEvent &e)
        {
            ++count;
            return lux::cxx::event::EventResult::Continue;
        }
    };

    lux::cxx::event::InterceptableSignal<IntEvent> sig;
    Handler handler;

    auto sub = sig.subscribe<&Handler::on_event>(&handler);
    sig.emit({1});
    assert(handler.count == 1);
}

static void test_filtered_signal_pass()
{
    std::puts("  test_filtered_signal_pass");

    lux::cxx::event::FilteredSignal<IntEvent> sig;
    int received = 0;

    auto sub = sig.subscribe(
        lux::cxx::event::SlotCallback<IntEvent>{[&received](const IntEvent &e)
                                                { received = e.value; }});

    sig.add_filter(+[](const IntEvent &e) -> bool
                   { return e.value > 0; });

    bool dispatched = sig.emit({42});
    assert(dispatched);
    assert(received == 42);
}

static void test_filtered_signal_reject()
{
    std::puts("  test_filtered_signal_reject");

    lux::cxx::event::FilteredSignal<IntEvent> sig;
    int received = 0;

    auto sub = sig.subscribe(
        lux::cxx::event::SlotCallback<IntEvent>{[&received](const IntEvent &e)
                                                { received = e.value; }});

    sig.add_filter(+[](const IntEvent &e) -> bool
                   { return e.value > 0; });

    bool dispatched = sig.emit({-1});
    assert(!dispatched);
    assert(received == 0); // filter rejected, callback not called
}

static void test_filtered_signal_chain()
{
    std::puts("  test_filtered_signal_chain");

    lux::cxx::event::FilteredSignal<IntEvent> sig;
    int received = 0;

    auto sub = sig.subscribe(
        lux::cxx::event::SlotCallback<IntEvent>{[&received](const IntEvent &e)
                                                { received = e.value; }});

    // Two filters: value > 0 AND value < 100
    sig.add_filter(+[](const IntEvent &e) -> bool
                   { return e.value > 0; });
    sig.add_filter(+[](const IntEvent &e) -> bool
                   { return e.value < 100; });

    assert(sig.emit({50}));
    assert(received == 50);

    assert(!sig.emit({-1}));  // fails first filter
    assert(received == 50);

    assert(!sig.emit({200})); // fails second filter
    assert(received == 50);
}

static void test_filtered_signal_remove_filter()
{
    std::puts("  test_filtered_signal_remove_filter");

    lux::cxx::event::FilteredSignal<IntEvent> sig;
    int received = 0;

    auto sub = sig.subscribe(
        lux::cxx::event::SlotCallback<IntEvent>{[&received](const IntEvent &e)
                                                { received = e.value; }});

    auto filter = +[](const IntEvent &e) -> bool
    { return e.value > 0; };
    sig.add_filter(filter);

    assert(!sig.emit({-1}));
    assert(received == 0);

    sig.remove_filter(filter);
    assert(sig.emit({-1}));
    assert(received == -1);
}

// ══════════════════════════════════════════════════════════════
// Phase 5 Tests: Trackable
// ══════════════════════════════════════════════════════════════

static void test_trackable_auto_disconnect()
{
    std::puts("  test_trackable_auto_disconnect");

    lux::cxx::event::Signal<IntEvent> sig;
    int received = 0;

    struct Widget : lux::cxx::event::Trackable
    {
        int *result;
        Widget(int *r) : result(r) {}

        void bind(lux::cxx::event::Signal<IntEvent> &s)
        {
            track(lux::cxx::event::connect(s, [this](const IntEvent &e)
                                           { *result = e.value; }));
        }
    };

    {
        Widget w(&received);
        w.bind(sig);
        sig.emit({42});
        assert(received == 42);
    }
    // Widget destroyed → all connections auto-disconnected

    assert(sig.subscriber_count() == 0);
    sig.emit({99});
    assert(received == 42); // unchanged
}

// ── Main ─────────────────────────────────────────────────────

int main()
{
    std::puts("=== Signal Phase-1 Tests ===");

    test_basic_subscribe_emit();
    test_unsubscribe_via_reset();
    test_unsubscribe_via_destructor();
    test_multiple_subscribers();
    test_priority_ordering();
    test_member_function_subscribe();
    test_empty_signal();
    test_scoped_subscription_move();
    test_subscription_group();
    test_unsubscribe_during_emit();
    test_subscribe_during_emit();
    test_recursive_emit();
    test_nested_emit_after_unsubscribe();
    test_event_type_id();
    test_event_traits();
    test_concepts();
    test_registry();
    test_empty_event();
    test_stable_priority_order();

    std::puts("\n=== Phase 2 Tests: SlotCallback + Connection + connect() ===");

    test_slot_callback_raw_fn();
    test_slot_callback_lambda();
    test_slot_callback_move();
    test_signal_subscribe_lambda();
    test_signal_connect_lambda();
    test_free_connect_lambda();
    test_free_connect_member();
    test_connection_group();
    test_scoped_connection_raii();

    std::puts("\n=== Phase 3 Tests: ThreadSafeSignal ===");

    test_thread_safe_signal_basic();
    test_thread_safe_signal_concurrent_emit();
    test_thread_safe_unsubscribe_correct();
    test_thread_safe_concurrent_churn();

    std::puts("\n=== Phase 4 Tests: InterceptableSignal + FilteredSignal ===");

    test_interceptable_signal_continue();
    test_interceptable_signal_handled();
    test_interceptable_signal_member_fn();
    test_filtered_signal_pass();
    test_filtered_signal_reject();
    test_filtered_signal_chain();
    test_filtered_signal_remove_filter();

    std::puts("\n=== Phase 5 Tests: Trackable ===");

    test_trackable_auto_disconnect();

    std::puts("\n=== All tests passed! ===");
    return 0;
}
