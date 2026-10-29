#include "service_fixture.hpp"

namespace {
using namespace service_test;

void uninitialized_and_invalid_options()
{
    Fixture fixture(false);
    assert(ir_service::busy());
    assert(ir_service::learn(Receiver::Envelope38) == ESP_ERR_INVALID_STATE);
    assert(ir_service::send("command", 0) == ESP_ERR_INVALID_STATE);
    assert(ir_service::init() == ESP_OK);
    assert(ir_service::init() == ESP_ERR_INVALID_STATE);
    seed();
    for (unsigned zone : {3U, 99U}) assert(ir_service::send("command", zone) == ESP_ERR_INVALID_ARG);
    for (unsigned count : {0U, 6U}) assert(ir_service::send("command", 0, count) == ESP_ERR_INVALID_ARG);
    for (unsigned gap : {19U, 501U}) assert(ir_service::send("command", 0, 1, gap) == ESP_ERR_INVALID_ARG);
    assert(ir_service::send(nullptr, 0) == ESP_ERR_INVALID_ARG);
    assert(ir_service::send("INVALID", 0) == ESP_ERR_INVALID_ARG);
    assert(ir_service::send_pending(0) == ESP_ERR_INVALID_STATE);
    assert(ir_service::learn(static_cast<Receiver>(99)) == ESP_ERR_INVALID_ARG);
    assert(fake::calls(Op::QueueSend) == 0 && fake::calls(Op::NewRx) == 0);
    assert_idle();
}

void capture_and_explicit_save()
{
    for (auto receiver : {Receiver::Envelope38, Receiver::Envelope56}) {
        Fixture fixture;
        assert(ir_service::learn(receiver) == ESP_OK);
        assert(fake::state.receive_pins.back() == (receiver == Receiver::Envelope38 ? 8 : 9));
        assert(fake::calls(Op::NvsSet) == 0 && fake::calls(Op::NvsCommit) == 0);
        assert(ir_service::save_pending("learned", false) == ESP_OK);
        ir::Waveform loaded;
        assert(ir_service::load("learned", loaded) == ESP_OK);
        assert(loaded.carrier_hz == (receiver == Receiver::Envelope38 ? 38000U : 56000U));
        assert((loaded.durations_us == std::vector<std::uint32_t>{560, 560, 560}));
        if (receiver == Receiver::Envelope38) assert(fake::state.committed.at("learned") == kRecord);
        assert_idle();
    }
    Fixture fixture;
    fake::Capture carrier;
    carrier.symbols.assign(20, {17, 0, 33, 1});
    carrier.symbols.push_back({17, 0, 30000, 1});
    fake::state.captures.push_back(carrier);
    assert(ir_service::learn(Receiver::Carrier) == ESP_OK);
    assert(fake::state.receive_pins.back() == 10);
    assert(ir_service::save_pending("carrier", false) == ESP_OK);
    ir::Waveform loaded;
    assert(ir_service::load("carrier", loaded) == ESP_OK && loaded.carrier_hz == 40000);
    assert((loaded.durations_us == std::vector<std::uint32_t>{525}));
    // An envelope-shaped input has too few carrier cycles for estimation.
    assert(ir_service::learn(Receiver::Carrier) == ESP_ERR_INVALID_ARG);
    assert(ir_service::print_pending() == ESP_ERR_INVALID_STATE);
    assert_idle();
}

// Idle callbacks rearm within one deadline; rejected captures clear pending RAM
// without changing previously saved data.
void capture_completion_and_deadline()
{
    {
        Fixture fixture;
        fake::Capture idle;
        idle.symbols = {{30000, 1, 0, 1}};
        fake::state.captures = {idle, fake::Capture{}};
        assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
        assert(fake::calls(Op::Receive) == 2);
        assert_idle();
    }
    for (int variant = 0; variant < 6; ++variant) {
        Fixture fixture;
        seed();
        assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
        fake::Capture capture;
        esp_err_t expected = ESP_ERR_INVALID_SIZE;
        switch (variant) {
        case 0: capture.complete = false; break;
        case 1: capture.wrong_buffer = true; break;
        case 2: capture.reported_count = 16384; break;
        case 3: capture.idle_high = false; break;
        case 4: capture.callback = false; expected = ESP_ERR_TIMEOUT; break;
        case 5:
            capture.symbols = {{30000, 1, 0, 1}};
            capture.elapsed = 10000;
            expected = ESP_ERR_TIMEOUT;
            break;
        }
        fake::state.captures.push_back(capture);
        assert(ir_service::learn(Receiver::Envelope38) == expected);
        assert(ir_service::print_pending() == ESP_ERR_INVALID_STATE);
        assert(ir_service::save_pending("lost", false) == ESP_ERR_INVALID_STATE);
        assert(fake::state.visible.at("command") == kRecord && fake::calls(Op::NvsSet) == 0);
        assert_idle();
    }
}

void capture_failure_cleanup()
{
    for (auto operation : {Op::Allocate, Op::BinarySemaphore, Op::NewRx, Op::Pullup,
                           Op::Callbacks, Op::Enable, Op::Receive}) {
        for (unsigned call = 1; call <= (operation == Op::Allocate ? 2U : 1U); ++call) {
            Fixture fixture;
            fake::fail(operation, call);
            const auto expected = operation == Op::Allocate || operation == Op::BinarySemaphore
                                    ? ESP_ERR_NO_MEM : fake::kInjectedError;
            assert(ir_service::learn(Receiver::Envelope38) == expected);
            assert(ir_service::print_pending() == ESP_ERR_INVALID_STATE);
            assert_idle();
            assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
            assert_idle();
        }
    }
}

void queue_limit_and_failed_insert()
{
    Fixture fixture;
    seed();
    fake::fail(Op::QueueSend);
    assert(ir_service::send("command", 0) == ESP_ERR_TIMEOUT);
    assert_idle();
    for (int i = 0; i < 4; ++i) assert(ir_service::send("command", 0) == ESP_OK);
    assert(ir_service::busy());
    const auto calls = fake::calls(Op::QueueSend);
    assert(ir_service::send("command", 0) == ESP_ERR_INVALID_STATE);
    assert(fake::calls(Op::QueueSend) == calls);
    assert(ir_service::learn(Receiver::Envelope38) == ESP_ERR_INVALID_STATE);
    fake::run_worker();
    assert(fake::state.transmissions.size() == 4);
    assert_idle();
    // Capacity recovers after the worker releases all four jobs.
    assert(ir_service::send("command", 0) == ESP_OK);
    fake::run_worker();
    assert_idle();
}

void queued_waveform_ownership()
{
    Fixture fixture;
    seed();
    assert(ir_service::send("command", 1) == ESP_OK);
    assert(ir_service::remove("command") == ESP_OK);
    seed("command", kOtherRecord);
    assert(ir_service::send("command", 2) == ESP_OK);
    fake::run_worker();
    assert(fake::state.transmissions.size() == 2);
    const auto first = fake::state.transmissions[0];
    const auto &second = fake::state.transmissions[1];
    assert(first.pin == 5 && first.carrier == 38000 && std::fabs(first.duty - 0.33f) < 0.001f);
    assert(second.pin == 6 && second.carrier == 40000);
    assert_symbols(first.symbols, {{560, 1, 560, 0}, {560, 1, 1, 0}});
    assert_symbols(second.symbols, {{1000, 1, 500, 0}, {1000, 1, 1, 0}});
    assert_idle();
    assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
    assert(ir_service::send_pending(0) == ESP_OK);
    assert(ir_service::send_pending(0) == ESP_OK);
    fake::run_worker();
    assert(fake::state.transmissions.size() == 4);
    assert_symbols(fake::state.transmissions[3].symbols, first.symbols);
    assert(ir_service::print_pending() == ESP_OK);
    assert_idle();
}

void capture_transmit_exclusion()
{
    Fixture fixture;
    seed();
    unsigned attempts = 0;
    const auto contend = [&] {
        ++attempts;
        assert(ir_service::busy());
        assert(ir_service::learn(Receiver::Envelope38) == ESP_ERR_INVALID_STATE);
        assert(ir_service::send("command", 0) == ESP_ERR_INVALID_STATE);
        assert(ir_service::save_pending("blocked", false) == ESP_ERR_INVALID_STATE);
    };
    fake::state.on_receive = contend;
    assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
    fake::state.on_wait_tx = contend;
    assert(ir_service::send_pending(0) == ESP_OK);
    fake::run_worker();
    assert(attempts == 2 && fake::calls(Op::NewRx) == 1 && fake::calls(Op::NewTx) == 1);
    assert_idle();
}

void repeats_and_long_spans()
{
    Fixture fixture;
    // Capture two adjacent low halves to obtain a mark exceeding 15 bits.
    fake::Capture capture;
    capture.symbols = {{32767, 0, 32767, 0}, {14466, 0, 30000, 1}};
    fake::state.captures.push_back(capture);
    assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
    assert(ir_service::send_pending(2, 5, 500) == ESP_OK);
    fake::run_worker();
    assert(fake::state.transmissions.size() == 5);
    assert((fake::state.delays == std::vector<TickType_t>{500, 500, 500, 500}));
    for (const auto &sent : fake::state.transmissions) {
        assert(sent.pin == 6);
        assert_symbols(sent.symbols, {{32767, 1, 7233, 1}});
    }
    assert_idle();
}

void transmit_failure_cleanup()
{
    for (auto operation : {Op::Allocate, Op::NewTx, Op::Carrier, Op::Encoder,
                           Op::Enable, Op::GpioSet, Op::Transmit, Op::WaitTx}) {
        Fixture fixture;
        seed();
        // Fail output enabling after the initial inhibit, not initialization.
        fake::fail(operation, fake::calls(operation) + (operation == Op::GpioSet ? 2U : 1U));
        assert(ir_service::send("command", 0, 3) == ESP_OK);
        fake::run_worker();
        assert(fake::logged("Transmission command failed:") && fake::logged("no automatic retry"));
        assert(!fake::logged("Transmitted command") && fake::state.transmissions.empty());
        assert(fake::calls(Op::Transmit) <= 1 && fake::state.delays.empty());
        assert_idle();
        assert(ir_service::send("command", 0) == ESP_OK);
        fake::run_worker();
        assert(fake::state.transmissions.size() == 1);
        assert_idle();
    }
    Fixture fixture;
    seed();
    fake::fail(Op::WaitTx, 2);
    assert(ir_service::send("command", 0, 3, 20) == ESP_OK);
    fake::run_worker();
    assert(fake::state.transmissions.size() == 1 && fake::calls(Op::Transmit) == 2);
    assert((fake::state.delays == std::vector<TickType_t>{20}));
    assert(fake::logged("Transmission command failed:") && !fake::logged("Transmitted command"));
    assert_idle();
}

// Teardown failures retain driver-owned buffers and reject subsequent work.
void teardown_fault_latching()
{
    for (auto operation : {Op::Disable, Op::DeleteChannel}) {
        for (bool receive : {false, true}) {
            Fixture fixture;
            seed();
            fake::fail(operation);
            if (receive) {
                assert(ir_service::learn(Receiver::Envelope38) == ESP_FAIL);
            } else {
                assert(ir_service::send("command", 0) == ESP_OK);
                assert(ir_service::send("command", 0) == ESP_OK);
                fake::run_worker();
                assert(fake::calls(Op::NewTx) == 1);
                assert(fake::logged("Transmission command failed:"));
            }
            assert(ir_service::busy() && fake::state.pins.at(18) == 1);
            assert(fake::channels() == 1 && fake::allocations() == (receive ? 2U : 1U));
            assert(fake::semaphores() == (receive ? 2U : 1U));
            assert(fake::encoders() == (receive ? 0U : 1U));
            assert(fake::logged("service disabled until restart"));
            assert(ir_service::learn(Receiver::Envelope38) == ESP_ERR_INVALID_STATE);
            assert(ir_service::send("command", 0) == ESP_ERR_INVALID_STATE);
            assert(ir_service::send_pending(0) == ESP_ERR_INVALID_STATE);
        }
    }
}
} // namespace

int main(int argc, char **argv)
{
    return service_test::run_cases(argc, argv, {
        {"uninitialized_and_invalid_options", uninitialized_and_invalid_options},
        {"capture_and_explicit_save", capture_and_explicit_save},
        {"capture_completion_and_deadline", capture_completion_and_deadline},
        {"capture_failure_cleanup", capture_failure_cleanup},
        {"queue_limit_and_failed_insert", queue_limit_and_failed_insert},
        {"queued_waveform_ownership", queued_waveform_ownership},
        {"capture_transmit_exclusion", capture_transmit_exclusion},
        {"repeats_and_long_spans", repeats_and_long_spans},
        {"transmit_failure_cleanup", transmit_failure_cleanup},
        {"teardown_fault_latching", teardown_fault_latching},
    });
}
