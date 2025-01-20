#include <lux/cxx/concurrent/ThreadPool.hpp>
#include <iostream>

int main()
{
    using namespace lux::cxx;

    // 1) 创建一个线程池: 2个工作线程, 队列容量64
    ThreadPool pool(5, 64);

    auto start_time = std::chrono::steady_clock::now();
    // 2) 提交多个有返回值的任务
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 5; ++i)
    {
        auto f = pool.submit([i] {
            // 模拟工作
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return i * i;
        });
        futures.push_back(std::move(f));
    }
    std::cout << "sizeof move_only_function:" << sizeof(move_only_function<void (void)>) << std::endl;
    sizeof(std::function<void (void)>);
    // allocate to heap
    char buffer[64];
    // 3) 提交一个无返回值的任务
    auto f_void = pool.submit([buffer] {
        std::cout << "[VoidTask] Hello from thread " 
                  << std::this_thread::get_id() << "\n";
    });

    // 4) 提交一个会抛异常的任务
    auto f_ex = pool.submit([] {
        throw std::runtime_error("Intentional error in task.");
        return 42; // never reached
    });

    // 5) 等待并检查有返回值的任务结果
    for (int i = 0; i < 5; ++i)
    {
        int result = futures[i].get();
        std::cout << "Square of " << i << " is " << result << "\n";
        assert(result == i * i);
    }

    // 等待无返回值任务
    f_void.get(); // 确保无异常
    std::cout << "VoidTask done.\n";

    // 6) 检查带异常的任务
    try
    {
        int val = f_ex.get(); 
        // 不应该到这里
        std::cout << "Unexpected success: " << val << "\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "Caught exception from task: " << e.what() << "\n";
    }

    // 7) 线程池析构时自动关闭, join所有线程
    // (离开 main() scope 时发生)
    std::cout << "Main finished.\n";
    auto end_time = std::chrono::steady_clock::now();
    std::cout << "Time elapsed: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << "us\n";

    return 0;
}
