#include "ft601_device.h"
#include "payload_test.h"
#include "service_protocol.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

int ReadMenuChoice() {
    std::cout << "\nSelect action:\n";
    std::cout << "1) Write test payload (" << WRITE_WORD_COUNT << " words)\n";
    std::cout << "2) Read payload to file\n";
    std::cout << "3) Loopback integrity test\n";
    std::cout << "4) Get FPGA status\n";
    std::cout << "5) Set loopback mode\n";
    std::cout << "6) Set normal mode\n";
    std::cout << "7) Clear TX error\n";
    std::cout << "8) Clear RX error\n";
    std::cout << "9) Clear all errors\n";
    std::cout << "10) Exit\n";
    std::cout << "Select: ";

    int choice = -1;
    if (!(std::cin >> choice)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return -1;
    }

    return choice;
}

bool TryReopenAndRetry(FT_HANDLE& h,
                       FT_STATUS op_status,
                       std::string& err,
                       const char* retry_label,
                       bool (*operation)(FT_HANDLE, std::string&, FT_STATUS*)) {
    if (!IsDisconnectStatus(op_status)) {
        return false;
    }

    if (!ReopenDevice(h, err)) {
        std::cerr << "REOPEN ERROR: " << err << "\n";
        return true;
    }

    std::cout << "Retrying " << retry_label << "...\n";
    FT_STATUS retry_status = FT_OK;
    if (!operation(h, err, &retry_status)) {
        std::cerr << retry_label << " ERROR after reopen: " << err << "\n";
    }

    return true;
}

void HandleWritePayload(FT_HANDLE& h) {
    std::string err;
    FT_STATUS op_status = FT_OK;

    std::cout << "Writing test payload...\n";
    if (!DoWriteTestPayload(h, err, &op_status)) {
        std::cerr << "WRITE ERROR: " << err << "\n";
        TryReopenAndRetry(h, op_status, err, "WRITE", DoWriteTestPayload);
    } else {
        std::cout << "WRITE OK.\n";
    }
}

void HandleReadToFile(FT_HANDLE& h) {
    const std::string out_file = "rx_dump.bin";
    std::string err;
    uint64_t bytes = 0;
    FT_STATUS op_status = FT_OK;

    std::cout << "Reading raw payload until timeout (" << TIMEOUT_MS
              << " ms) -> " << out_file << "\n";

    if (!DoReadToFile(h, out_file, err, bytes, &op_status)) {
        std::cerr << "READ ERROR: " << err << "\n";
        if (IsDisconnectStatus(op_status) && ReopenDevice(h, err)) {
            std::cout << "Retrying read...\n";
            if (!DoReadToFile(h, out_file, err, bytes, &op_status)) {
                std::cerr << "READ ERROR after reopen: " << err << "\n";
            } else {
                std::cout << "READ OK after reopen: saved " << bytes
                          << " bytes to " << out_file << "\n";
            }
        } else if (IsDisconnectStatus(op_status)) {
            std::cerr << "REOPEN ERROR: " << err << "\n";
        }
    } else {
        std::cout << "READ OK: saved " << bytes << " bytes to "
                  << out_file << "\n";
    }
}

void HandleLoopbackTest(FT_HANDLE& h) {
    std::string err;
    FT_STATUS op_status = FT_OK;
    const size_t word_count = ReadPayloadWordCount();

    if (!DoLoopbackIntegrityTest(h, word_count, err, &op_status)) {
        std::cerr << "LOOPBACK ERROR: " << err << "\n";
        if (IsDisconnectStatus(op_status) && ReopenDevice(h, err)) {
            std::cout << "Retrying loopback integrity test...\n";
            if (!DoLoopbackIntegrityTest(h, word_count, err, &op_status)) {
                std::cerr << "LOOPBACK ERROR after reopen: " << err << "\n";
            }
        } else if (IsDisconnectStatus(op_status)) {
            std::cerr << "REOPEN ERROR: " << err << "\n";
        }
    }
}

void HandleGetStatus(FT_HANDLE& h) {
    std::string err;
    FT_STATUS op_status = FT_OK;

    if (!DoGetStatus(h, err, &op_status)) {
        std::cerr << "STATUS ERROR: " << err << "\n";
        TryReopenAndRetry(h, op_status, err, "STATUS", DoGetStatus);
    }
}

void HandleServiceCommand(FT_HANDLE& h, uint32_t opcode, const char* label) {
    std::string err;
    FT_STATUS op_status = FT_OK;

    std::cout << "Sending " << label << " and requesting status...\n";
    if (!DoCommandAndGetStatus(h, opcode, err, &op_status)) {
        std::cerr << "COMMAND ERROR: " << err << "\n";
        if (IsDisconnectStatus(op_status) && ReopenDevice(h, err)) {
            std::cout << "Retrying command...\n";
            if (!DoCommandAndGetStatus(h, opcode, err, &op_status)) {
                std::cerr << "COMMAND ERROR after reopen: " << err << "\n";
            }
        } else if (IsDisconnectStatus(op_status)) {
            std::cerr << "REOPEN ERROR: " << err << "\n";
        }
    }
}

}  // namespace

int main() {
    FT_HANDLE h = nullptr;
    std::string err;

    if (!OpenDevice(h, err)) {
        std::cerr << "ERROR: " << err << "\n";
        return 1;
    }

    std::cout << "Device opened. IN pipe=0x" << std::hex
              << static_cast<int>(IN_PIPE) << " OUT pipe=0x"
              << static_cast<int>(OUT_PIPE) << std::dec << "\n";

    while (true) {
        const int choice = ReadMenuChoice();
        if (choice == 10) {
            break;
        }

        switch (choice) {
            case 1:
                HandleWritePayload(h);
                break;
            case 2:
                HandleReadToFile(h);
                break;
            case 3:
                HandleLoopbackTest(h);
                break;
            case 4:
                HandleGetStatus(h);
                break;
            case 5:
                HandleServiceCommand(h, CMD_SET_LOOPBACK, "SET_LOOPBACK");
                break;
            case 6:
                HandleServiceCommand(h, CMD_SET_NORMAL, "SET_NORMAL");
                break;
            case 7:
                HandleServiceCommand(h, CMD_CLR_TX_ERROR, "CLR_TX_ERROR");
                break;
            case 8:
                HandleServiceCommand(h, CMD_CLR_RX_ERROR, "CLR_RX_ERROR");
                break;
            case 9:
                HandleServiceCommand(h, CMD_CLR_ALL_ERROR, "CLR_ALL_ERROR");
                break;
            default:
                std::cout << "Unknown option.\n";
                break;
        }
    }

    if (h != nullptr) {
        FT_Close(h);
    }

    std::cout << "Bye.\n";
    return 0;
}