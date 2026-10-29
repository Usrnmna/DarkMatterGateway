#pragma once
#include "sdk.hpp"
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fake {
enum class Op {
    GpioConfig, GpioSet, Pullup, Mutex, BinarySemaphore, Queue, QueueSend, Task,
    Allocate, NewRx, NewTx, Callbacks, Enable, Receive, Disable, DeleteChannel,
    Carrier, Encoder, Transmit, WaitTx, DeleteEncoder,
    NvsInit, NvsOpen, NvsGet, NvsSet, NvsCommit, NvsEraseKey, NvsFind, NvsInfo, NvsNext, EraseAll
};
constexpr esp_err_t kInjectedError = 0x7001;
using Bytes = std::vector<std::uint8_t>;
struct Capture {
    std::vector<rmt_symbol_word_t> symbols{{1120, 0, 1120, 1}, {1120, 0, 30000, 1}};
    bool complete = true, idle_high = true, callback = true, wrong_buffer = false;
    std::size_t reported_count = 0; // Zero selects the vector size.
    TickType_t elapsed = 1;
};
struct Transmission {
    int pin;
    unsigned carrier;
    float duty;
    std::vector<rmt_symbol_word_t> symbols;
};
struct State {
    std::map<Op, unsigned> calls;
    struct Failure { unsigned call; esp_err_t error; };
    std::map<Op, Failure> failures;
    std::map<int, int> pins;
    std::map<std::string, Bytes> visible, committed;
    std::deque<Capture> captures;
    std::vector<int> receive_pins;
    std::vector<Transmission> transmissions;
    std::vector<TickType_t> delays;
    std::vector<std::string> logs;
    std::function<void()> on_receive, on_wait_tx, after_commit;
    TaskFunction_t worker = nullptr;
    void *worker_argument = nullptr;
    TickType_t ticks = 0;
    bool storage_open = false, commit_before_error = false;
};
extern State state;
void fail(Op operation, unsigned call = 1, esp_err_t error = kInjectedError);
unsigned calls(Op operation);
// Run the registered real worker until it blocks on an empty fake queue.
void run_worker();
std::size_t allocations();
std::size_t channels();
std::size_t encoders();
std::size_t semaphores();
std::size_t queues();
std::size_t iterators();
// Only the test fixture calls this simulated power-off after assertions.
void shutdown();
bool logged(const std::string &fragment);
} // namespace fake
