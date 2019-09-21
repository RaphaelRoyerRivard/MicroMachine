#include "profiler.h"
using Clock = std::chrono::high_resolution_clock;

void Stopwatch::start() {
    startTime = Clock::now();
}

void Stopwatch::stop() {
    endTime = Clock::now();
}

double Stopwatch::millis() {
    std::chrono::duration<double, std::milli> duration = endTime - startTime;
    return duration.count();
}
