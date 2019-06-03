#pragma once
#include <chrono>

struct Stopwatch {
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
	std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
public:
	Stopwatch() : startTime(), endTime() {
		start();
	}
	Stopwatch(bool started) : startTime(), endTime() {
		if (started) start();
	}
	double millis();
	void stop();
	void start();
};
