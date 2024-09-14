#include <chrono>
#include <ratio>
#include <thread>
#include <string>

#include "timer_thread.h"


void repeatTimerThread(const std::string &threadName, const int tickRate_ms, SharedResources &sharedResources, std::atomic<bool> &shutdownFlag) {
    auto nextTick = std::chrono::steady_clock::now() + std::chrono::milliseconds(tickRate_ms);

    while(!shutdownFlag) {
        std::this_thread::sleep_until(nextTick);

        nextTick += std::chrono::milliseconds(tickRate_ms);

        auto event = std::make_unique<TimerFiredEvent>(threadName);
        pushEvent(std::ref(sharedResources), std::move(event));
    }
         
}

void fireOnceTimerThread(const std::string &threadName, const int tickRate_ms, SharedResources &sharedResources) {
    auto nextTick = std::chrono::steady_clock::now() + std::chrono::milliseconds(tickRate_ms);

    std::this_thread::sleep_until(nextTick);

    auto event = std::make_unique<TimerFiredEvent>(threadName);
    pushEvent(std::ref(sharedResources), std::move(event));
}

