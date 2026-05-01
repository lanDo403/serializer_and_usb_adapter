#pragma once

#include <chrono>
#include <cstdint>
#include <string>

typedef std::chrono::steady_clock::time_point ThroughputTimePoint;

ThroughputTimePoint ThroughputNow();
double ThroughputSeconds(ThroughputTimePoint start, ThroughputTimePoint end);
void PrintThroughput(const std::string& label, uint64_t bytes, double seconds);
