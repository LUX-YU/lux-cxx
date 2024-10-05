//
// Created by chenh on 8/28/2024.
//

#include <lux/cxx/concurrent/Timer.hpp>
#include <iostream>

static void repeated_task(const int ms, lux::cxx::Timer& timer) {
    timer.addTimer(ms,
        [ms, &timer]() {
            std::cout << "r 1s" << std::endl;
            repeated_task(ms, timer);
        }
    );
}

int main() {
    using namespace ::lux::cxx;
    ThreadPool pool(2);
    Timer t(pool);

    t.addTimer(5, [](){std::cout << "0.005s" << std::endl;});
    repeated_task(1000, t);

    t.addTimer(2000, [](){std::cout << "2s" << std::endl;});
    t.addTimer(1500, [](){std::cout << "1.5s" << std::endl;});
    t.addTimer(2345, [](){std::cout << "2.345s" << std::endl;});
    t.addTimer(2999, [](){std::cout << "2.999s" << std::endl;});
    t.addTimer(3000, [](){std::cout << "3s" << std::endl;});

    t.start();

    for(int i = 0; i < 3; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "----------------------------------------------" << std::endl;
    }

    return 0;
}