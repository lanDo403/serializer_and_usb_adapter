#include "payload_test.h"

#include "ft601_device.h"
#include "service_protocol.h"
#include "throughput.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

std::vector<uint32_t> GenerateDeterministicPayload(size_t word_count) {
    std::vector<uint32_t> payload(word_count);
    uint32_t x = 0x13579BDFu;

    for (size_t i = 0; i < word_count; ++i) {
        x = x * 1664525u + 1013904223u;
        payload[i] = x ^ (static_cast<uint32_t>(i) * 0x9E3779B9u);
    }

    return payload;
}

bool WritePayload(FT_HANDLE h,
                  const std::vector<uint32_t>& payload,
                  std::string& err,
                  FT_STATUS* last_status) {
    return WriteWords(h, payload, err, last_status);
}

bool ComparePayload(const std::vector<uint32_t>& tx,
                    const std::vector<uint32_t>& rx,
                    std::string& err) {
    if (tx.size() != rx.size()) {
        err = "Payload size mismatch: expected " + std::to_string(tx.size()) +
              " words, got " + std::to_string(rx.size()) + " words";
        return false;
    }

    for (size_t i = 0; i < tx.size(); ++i) {
        if (tx[i] != rx[i]) {
            std::ostringstream oss;
            oss << "Payload mismatch at word " << i
                << " byte offset " << (i * sizeof(uint32_t))
                << ": expected 0x" << std::hex << std::setw(8)
                << std::setfill('0') << tx[i]
                << ", got 0x" << std::setw(8) << rx[i];
            err = oss.str();
            return false;
        }
    }

    return true;
}

size_t ReadPayloadWordCount() {
    std::cout << "Payload words [" << DEFAULT_LOOPBACK_WORDS
              << ", max " << MAX_LOOPBACK_WORDS << "]: ";

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string line;
    if (!std::getline(std::cin, line) || line.empty()) {
        return DEFAULT_LOOPBACK_WORDS;
    }

    std::istringstream iss(line);
    size_t word_count = DEFAULT_LOOPBACK_WORDS;
    if (!(iss >> word_count)) {
        return DEFAULT_LOOPBACK_WORDS;
    }

    if (word_count == 0) {
        return DEFAULT_LOOPBACK_WORDS;
    }

    if (word_count > MAX_LOOPBACK_WORDS) {
        std::cout << "Clamping payload to " << MAX_LOOPBACK_WORDS
                  << " words.\n";
        return MAX_LOOPBACK_WORDS;
    }

    return word_count;
}

bool DoWriteTestPayload(FT_HANDLE h, std::string& err, FT_STATUS* last_status) {
    std::vector<uint32_t> payload(WRITE_WORD_COUNT);
    for (uint32_t i = 0; i < payload.size(); ++i) {
        payload[i] = i + 1;
    }

    const ThroughputTimePoint start = ThroughputNow();
    if (!WriteWords(h, payload, err, last_status)) {
        return false;
    }

    const double seconds = ThroughputSeconds(start, ThroughputNow());
    const uint64_t bytes = static_cast<uint64_t>(payload.size()) *
                           sizeof(uint32_t);
    PrintThroughput("Write payload throughput", bytes, seconds);
    return true;
}

bool DoLoopbackIntegrityTest(FT_HANDLE h,
                             size_t word_count,
                             std::string& err,
                             FT_STATUS* last_status) {
    std::cout << "Entering loopback mode...\n";
    if (!SendCommandFrame(h, CMD_SET_LOOPBACK, err, last_status)) {
        return false;
    }

    uint32_t status_word = 0;
    if (!RequestStatus(h, status_word, err, last_status)) {
        return false;
    }
    PrintStatusWord(status_word);

    if ((status_word & 1u) == 0) {
        err = "FPGA did not enter loopback mode";
        return false;
    }

    std::cout << "Generating " << word_count << " payload words...\n";
    const std::vector<uint32_t> tx = GenerateDeterministicPayload(word_count);
    const uint64_t payload_bytes = static_cast<uint64_t>(tx.size()) *
                                   sizeof(uint32_t);

    std::cout << "Writing payload to EP02...\n";
    const ThroughputTimePoint total_start = ThroughputNow();
    const ThroughputTimePoint write_start = total_start;
    if (!WritePayload(h, tx, err, last_status)) {
        return false;
    }
    const ThroughputTimePoint write_end = ThroughputNow();

    std::cout << "Reading exact loopback payload from EP82...\n";
    std::vector<uint32_t> rx;
    const ThroughputTimePoint read_start = ThroughputNow();
    if (!ReadExactWords(h, tx.size(), rx, err, last_status)) {
        return false;
    }
    const ThroughputTimePoint read_end = ThroughputNow();

    if (!ComparePayload(tx, rx, err)) {
        return false;
    }

    PrintThroughput("Loopback write throughput",
                    payload_bytes,
                    ThroughputSeconds(write_start, write_end));
    PrintThroughput("Loopback read throughput",
                    payload_bytes,
                    ThroughputSeconds(read_start, read_end));
    PrintThroughput("Loopback round-trip throughput",
                    payload_bytes * 2u,
                    ThroughputSeconds(total_start, read_end));
    std::cout << "LOOPBACK PASS: " << tx.size() << " words verified.\n";
    return true;
}
