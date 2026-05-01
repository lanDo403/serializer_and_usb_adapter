#include "throughput.h"

#include <iomanip>
#include <iostream>
#include <sstream>

ThroughputTimePoint ThroughputNow() {
    return std::chrono::steady_clock::now();
}

double ThroughputSeconds(ThroughputTimePoint start, ThroughputTimePoint end) {
    return std::chrono::duration<double>(end - start).count();
}

void PrintThroughput(const std::string& label, uint64_t bytes, double seconds) {
    if (bytes == 0) {
        std::cout << label << ": 0 bytes, throughput not measured.\n";
        return;
    }

    if (seconds <= 0.0) {
        std::cout << label << ": " << bytes
                  << " bytes, elapsed time is too small to measure.\n";
        return;
    }

    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    const double mib_per_sec = mib / seconds;
    const double mbps = (static_cast<double>(bytes) * 8.0) /
                        (seconds * 1000000.0);

    std::ostringstream oss;
    oss << label << ": " << bytes << " bytes in "
        << std::fixed << std::setprecision(6) << seconds << " s, "
        << std::setprecision(2) << mib_per_sec << " MiB/s, "
        << mbps << " Mbps";

    std::cout << oss.str() << "\n";
}
