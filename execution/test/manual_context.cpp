#include <lux/cxx/execution/ManualContext.hpp>
#include <exec/static_thread_pool.hpp>
#include "TestSupport.hpp"

#include <atomic>
#include <barrier>
#include <cstdint>
#include <memory>
#include <new>
#include <thread>
#include <vector>

namespace ex = stdexec;
using namespace lux::cxx::execution;

struct Results
{
    std::atomic<unsigned> values{0}, stops{0}, errors{0};
};
struct Receiver
{
    using receiver_concept = ex::receiver_t;
    Results* results;
    std::thread::id owner;
    ex::inplace_stop_token stop;

    auto get_env() const noexcept { return ex::env{ex::prop{ex::get_stop_token, stop}}; }
    void set_value() && noexcept
    {
        CHECK(std::this_thread::get_id() == owner);
        results->values.fetch_add(1, std::memory_order_relaxed);
    }
    void set_stopped() && noexcept
    {
        CHECK(std::this_thread::get_id() == owner);
        results->stops.fetch_add(1, std::memory_order_relaxed);
    }
    void set_error(ScheduleError error) && noexcept
    {
        CHECK(error == ScheduleError::context_closed);
        results->errors.fetch_add(1, std::memory_order_relaxed);
    }
    template <class Error> void set_error(Error&&) && noexcept { CHECK(false); }
};
using ScheduleSender = decltype(ex::schedule(std::declval<ManualContext::Scheduler>()));
using Operation = ex::connect_result_t<ScheduleSender, Receiver>;
static_assert(ex::sender<ScheduleSender>);
static_assert(ex::operation_state<Operation>);
static_assert(!std::is_move_constructible_v<Operation>);
static_assert(std::is_nothrow_copy_constructible_v<ManualContext::Scheduler>);

static void protocol_and_lazy()
{
    ManualContext context;
    Results results;
    auto scheduler = context.get_scheduler();
    CHECK(scheduler == context.get_scheduler());
    CHECK(scheduler != context.get_scheduler(ScheduleMode::delivery));
    auto sender = ex::schedule(scheduler);
    CHECK(ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(sender)) == scheduler);
    auto operation = ex::connect(sender, Receiver{&results, std::this_thread::get_id(), {}});
    CHECK(context.is_idle());
    ex::start(operation);
    CHECK(context.pending() == 1); CHECK(results.values == 0);
    CHECK(context.run_ready(0) == 0);
    CHECK(context.run_ready(1) == 1);
    CHECK(results.values == 1); CHECK(context.is_idle());
}
static void budget_and_explicit_producer()
{
    ManualContext context({512});
    ManualContext::Producer producer(context);
    Results results;
    std::vector<std::unique_ptr<Operation>> operations;
    for (unsigned i = 0; i < 150; ++i)
    {
        operations.emplace_back(new Operation(ex::connect(ex::schedule(producer.get_scheduler()),
            Receiver{&results, std::this_thread::get_id(), {}})));
        ex::start(*operations.back());
    }
    CHECK(context.run_ready(65) == 65); CHECK(context.pending() == 85);
    CHECK(context.run_ready(200) == 85); CHECK(context.is_idle()); CHECK(results.values == 150);
}
static void stopped_before_start()
{
    ManualContext context;
    Results results;
    ex::inplace_stop_source stop;
    stop.request_stop();
    auto operation = ex::connect(ex::schedule(context.get_scheduler()),
        Receiver{&results, std::this_thread::get_id(), stop.get_token()});
    ex::start(operation);
    CHECK(results.stops == 0);
    CHECK(context.run_ready(1) == 1);
    CHECK(results.stops == 1); CHECK(results.values == 0);
}
static void stopped_after_enqueue()
{
    ManualContext context;
    Results results;
    ex::inplace_stop_source stop;
    auto operation = ex::connect(ex::schedule(context.get_scheduler()),
        Receiver{&results, std::this_thread::get_id(), stop.get_token()});
    ex::start(operation); stop.request_stop();
    CHECK(context.run_ready(1) == 1); CHECK(results.stops == 1);
}
static void delivery_ignores_stop()
{
    ManualContext context;
    Results results;
    ex::inplace_stop_source stop;
    stop.request_stop();
    auto operation = ex::connect(ex::schedule(context.get_scheduler(ScheduleMode::delivery)),
        Receiver{&results, std::this_thread::get_id(), stop.get_token()});
    ex::start(operation);
    CHECK(context.run_ready(1) == 1); CHECK(results.values == 1); CHECK(results.stops == 0);
}
static void close_preserves_admitted_work()
{
    ManualContext context;
    Results results;
    auto first = ex::connect(ex::schedule(context.get_scheduler()),
        Receiver{&results, std::this_thread::get_id(), {}});
    auto rejected = ex::connect(ex::schedule(context.get_scheduler(ScheduleMode::delivery)),
        Receiver{&results, std::this_thread::get_id(), {}});
    ex::start(first);
    CHECK(context.close()); CHECK(!context.close());
    std::jthread publisher([&] { ex::start(rejected); });
    publisher.join();
    CHECK(results.errors == 1); CHECK(results.values == 0); CHECK(context.pending() == 1);
    CHECK(context.run_ready(10) == 1); CHECK(results.values == 1); CHECK(context.is_idle());
}
static void notifier_is_after_publication()
{
    struct Notice { ManualContext* context = nullptr; unsigned notifications = 0; } notice;
    ManualContext context(default_manual_context_config(), {&notice, [](void* p) noexcept {
        auto& n = *static_cast<Notice*>(p);
        CHECK(n.context->pending() != 0 || n.context->is_closed());
        ++n.notifications;
    }});
    notice.context = &context;
    Results results;
    auto op = ex::connect(ex::schedule(context.get_scheduler()), Receiver{&results, std::this_thread::get_id(), {}});
    ex::start(op); CHECK(notice.notifications == 1);
    CHECK(context.run_ready(1) == 1);
    CHECK(context.close()); CHECK(notice.notifications == 2);
}
static void notifier_tail_blocks_idle()
{
    // Deliberately stall the notifier as a fault injection (production notifiers must be fast).
    struct Notice { std::atomic<bool> entered{false}, release{false}; } notice;
    ManualContext context(default_manual_context_config(), {&notice, [](void* p) noexcept {
        auto& n = *static_cast<Notice*>(p);
        n.entered.store(true, std::memory_order_release);
        while (!n.release.load(std::memory_order_acquire)) std::this_thread::yield();
    }});
    Results results;
    auto op = ex::connect(ex::schedule(context.get_scheduler()), Receiver{&results, std::this_thread::get_id(), {}});
    std::jthread publisher([&] { ex::start(op); });
    test::eventually([&] { return notice.entered.load(std::memory_order_acquire); });
    CHECK(context.run_ready(1) == 1); CHECK(results.values == 1); CHECK(context.pending() == 0);
    CHECK(!context.is_idle());
    notice.release.store(true, std::memory_order_release);
    publisher.join(); CHECK(context.is_idle());
}
static void close_notifier_tail_blocks_idle()
{
    struct Notice { std::atomic<bool> entered{false}, release{false}; } notice;
    ManualContext context(default_manual_context_config(), {&notice, [](void* p) noexcept {
        auto& n = *static_cast<Notice*>(p);
        n.entered.store(true, std::memory_order_release);
        while (!n.release.load(std::memory_order_acquire)) std::this_thread::yield();
    }});
    std::jthread closer([&] { CHECK(context.close()); });
    test::eventually([&] { return notice.entered.load(std::memory_order_acquire); });
    CHECK(context.is_closed()); CHECK(!context.is_idle());
    notice.release.store(true, std::memory_order_release);
    closer.join(); CHECK(context.is_idle());
}
static void receiver_may_destroy_operation()
{
    struct SelfDeletingReceiver
    {
        using receiver_concept = ex::receiver_t;
        void** allocation;
        void (*destroy)(void*) noexcept;
        unsigned* completed;
        auto get_env() const noexcept { return ex::env{}; }
        void set_value() && noexcept
        {
            ++*completed;
            auto* p = *allocation;
            auto dispose = destroy;
            *allocation = nullptr;
            dispose(p);
        }
        void set_error(ScheduleError) && noexcept { CHECK(false); }
        void set_stopped() && noexcept { CHECK(false); }
    };
    ManualContext context;
    unsigned completed = 0;
    void* allocation = nullptr;
    using Op = ex::connect_result_t<ScheduleSender, SelfDeletingReceiver>;
    auto* op = new Op(ex::connect(ex::schedule(context.get_scheduler()),
        SelfDeletingReceiver{&allocation, [](void* p) noexcept { delete static_cast<Op*>(p); }, &completed}));
    allocation = op;
    ex::start(*op);
    CHECK(context.run_ready(1) == 1);
    CHECK(allocation == nullptr); CHECK(completed == 1); CHECK(context.is_idle());
}
static void enqueue_from_callback()
{
    ManualContext context;
    Results results;
    auto second = ex::connect(ex::schedule(context.get_scheduler()),
        Receiver{&results, std::this_thread::get_id(), {}});
    auto first = ex::connect(ex::schedule(context.get_scheduler()) | ex::then([&]() noexcept {
        CHECK(!context.is_idle());
        ex::start(second);
    }), Receiver{&results, std::this_thread::get_id(), {}});
    ex::start(first);
    CHECK(context.run_ready(1) == 1); CHECK(context.pending() == 1);
    CHECK(context.run_ready(1) == 1); CHECK(results.values == 2);
}

struct JoinedReceiver
{
    using receiver_concept = ex::receiver_t;
    std::atomic<bool>* done;
    auto get_env() const noexcept { return ex::env{ex::prop{ex::get_scheduler, ex::inline_scheduler{}}}; }
    void set_value() && noexcept { done->store(true, std::memory_order_release); }
    void set_stopped() && noexcept { CHECK(false); }
    template<class E> void set_error(E&&) && noexcept { CHECK(false); }
};
static void pool_transfer_and_scope_join()
{
    ManualContext context;
    exec::static_thread_pool pool{4};
    ex::counting_scope scope;
    const auto owner = std::this_thread::get_id();
    unsigned completed = 0;
    for (unsigned i = 0; i < 1000; ++i)
    {
        auto pipeline = ex::schedule(pool.get_scheduler())
            | ex::then([i]() noexcept { return std::make_unique<unsigned>(i); })
            | ex::continues_on(context.get_scheduler(ScheduleMode::delivery))
            | ex::then([&, i](std::unique_ptr<unsigned> value) noexcept {
                CHECK(std::this_thread::get_id() == owner); CHECK(*value == i); ++completed;
            })
            | ex::upon_error([](auto&&) noexcept { CHECK(false); });
        ex::spawn(std::move(pipeline), scope.get_token());
    }
    scope.close();
    std::atomic<bool> joined{false};
    auto join = ex::connect(scope.join(), JoinedReceiver{&joined});
    ex::start(join);
    test::eventually([&] {
        (void)context.run_ready(64);
        return joined.load(std::memory_order_acquire) && context.is_idle();
    });
    CHECK(completed == 1000);
    CHECK(context.close());
}
static void starts_on_and_when_all()
{
    ManualContext context;
    Results results;
    const auto scheduler = context.get_scheduler();
    auto sender = ex::when_all(
        ex::starts_on(scheduler, ex::just(10)),
        ex::starts_on(scheduler, ex::just(20)))
        | ex::then([](int a, int b) noexcept { CHECK(a + b == 30); });
    auto op = ex::connect(std::move(sender), Receiver{&results, std::this_thread::get_id(), {}});
    ex::start(op);
    test::eventually([&] { (void)context.run_ready(64); return results.values.load() == 1; });
    CHECK(context.is_idle());
}

static void mpsc_exactly_once(bool seal_early, bool explicit_producers)
{
    constexpr unsigned lanes = 8, per_lane = 125000, total = lanes * per_lane;
    struct SeenReceiver
    {
        using receiver_concept = ex::receiver_t;
        std::atomic<unsigned char>* seen;
        std::atomic<unsigned>* completed;
        std::thread::id owner;
        void finish() noexcept
        {
            CHECK(seen->fetch_add(1, std::memory_order_relaxed) == 0);
            completed->fetch_add(1, std::memory_order_release);
        }
        auto get_env() const noexcept { return ex::env{}; }
        void set_value() && noexcept { CHECK(std::this_thread::get_id() == owner); finish(); }
        void set_stopped() && noexcept { CHECK(false); }
        void set_error(ScheduleError e) && noexcept { CHECK(e == ScheduleError::context_closed); finish(); }
    };
    using Op = ex::connect_result_t<ScheduleSender, SeenReceiver>;
    struct Slot { alignas(Op) std::byte bytes[sizeof(Op)]; };
    ManualContext context({4096});
    std::vector<std::unique_ptr<ManualContext::Producer>> producers;
    for (unsigned n = 0; n < lanes; ++n) producers.emplace_back(new ManualContext::Producer(context));
    auto slots = std::make_unique<Slot[]>(total);
    auto seen = std::make_unique<std::atomic<unsigned char>[]>(total);
    std::atomic<unsigned> completed{0};
    const auto owner = std::this_thread::get_id();
    std::barrier start(lanes + 1);
    std::vector<std::jthread> threads;
    for (unsigned lane = 0; lane < lanes; ++lane)
        threads.emplace_back([&, lane] {
            const auto scheduler = explicit_producers ? producers[lane]->get_scheduler() : context.get_scheduler();
            start.arrive_and_wait();
            for (unsigned n = 0; n < per_lane; ++n)
            {
                const auto i = lane * per_lane + n;
                auto* op = ::new (static_cast<void*>(slots[i].bytes)) Op(ex::connect(ex::schedule(scheduler),
                    SeenReceiver{&seen[i], &completed, owner}));
                ex::start(*op);
            }
        });
    start.arrive_and_wait();
    if (seal_early) CHECK(context.close());
    test::eventually([&] {
        (void)context.run_ready(256);
        return completed.load(std::memory_order_acquire) == total;
    });
    threads.clear();
    CHECK(context.is_idle());
    for (unsigned i = 0; i < total; ++i)
    {
        CHECK(seen[i].load() == 1);
        std::launder(reinterpret_cast<Op*>(slots[i].bytes))->~Op();
    }
}
static void million_implicit() { mpsc_exactly_once(false, false); }
static void million_explicit() { mpsc_exactly_once(false, true); }
static void million_close_race() { mpsc_exactly_once(true, true); }

int main()
{
    const test::Case cases[] = {
        {"protocol_and_lazy", protocol_and_lazy},
        {"budget_and_explicit_producer", budget_and_explicit_producer},
        {"stopped_before_start", stopped_before_start},
        {"stopped_after_enqueue", stopped_after_enqueue},
        {"delivery_ignores_stop", delivery_ignores_stop},
        {"close_preserves_admitted_work", close_preserves_admitted_work},
        {"notifier_is_after_publication", notifier_is_after_publication},
        {"notifier_tail_blocks_idle", notifier_tail_blocks_idle},
        {"close_notifier_tail_blocks_idle", close_notifier_tail_blocks_idle},
        {"receiver_may_destroy_operation", receiver_may_destroy_operation},
        {"enqueue_from_callback", enqueue_from_callback},
        {"pool_transfer_and_scope_join", pool_transfer_and_scope_join},
        {"starts_on_and_when_all", starts_on_and_when_all},
        {"million_implicit", million_implicit},
        {"million_explicit", million_explicit},
        {"million_close_race", million_close_race}
    };
    return test::run(cases);
}
