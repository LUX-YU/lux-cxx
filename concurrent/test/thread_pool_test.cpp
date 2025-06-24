#include <lux/cxx/concurrent/ThreadPool.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

int main()
{
    using namespace lux::cxx;
    using namespace std::chrono_literals;

    std::cout << "hardware_concurrency = "
        << std::jthread::hardware_concurrency() << '\n';
    std::cout << "sizeof move_only_function<void()> = "
        << sizeof(move_only_function<void()>) << '\n';

    /*------------------------------------------------------------
     * 1. 旧接口：无 stop_token
     *-----------------------------------------------------------*/
    ThreadPool pool(5, 64);

    std::vector<std::future<int>> squares;
    for (int i = 0; i < 5; ++i)
        squares.emplace_back(pool.submit([i] { std::this_thread::sleep_for(50ms); return i * i; }));

    auto f_void = pool.submit([] {
        std::cout << "[VoidTask] runs on " << std::this_thread::get_id() << '\n';
        });

    auto f_throw = pool.submit([]()->int { throw std::runtime_error("Intentional error"); });

    for (int i = 0; i < 5; ++i)  assert(squares[i].get() == i * i);
    f_void.get();
    try { (void)f_throw.get(); assert(false); }
    catch (const std::runtime_error& e) { std::cout << "Caught: " << e.what() << '\n'; }

    /*------------------------------------------------------------
     * 2. 新接口：可取消任务
     *-----------------------------------------------------------*/
    std::atomic<int> ticks{ 0 };
    auto lambda = [&ticks](std::stop_token tok)
    {
        while (!tok.stop_requested()) {
            ++ticks;
            std::this_thread::sleep_for(10ms);
        }
		return ticks.load();
    };

    auto h_token = pool.submit(lambda);

    /* 让任务跑一会，然后**主动取消** */
    std::this_thread::sleep_for(120ms);
    h_token.request_stop();

    /* 现在再关闭线程池 */
    pool.close();

    int loops = h_token.get();
    std::cout << "Token-aware task looped " << loops << " times\n";
    assert(loops > 0);

    /*------------------------------------------------------------
     * 3. 关闭后的 submit 必须抛异常
     *-----------------------------------------------------------*/
    try {
        pool.submit([] {});
        assert(false);
    }
    catch (const std::exception&) {}

    std::cout << "All runtime assertions OK.\n";
    return 0;
}
