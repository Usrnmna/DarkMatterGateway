#include "service_fixture.hpp"

namespace {
using namespace service_test;

void initialization_failure_preserves_records()
{
    for (auto operation : {Op::GpioConfig, Op::Mutex, Op::Queue, Op::NvsInit, Op::NvsOpen, Op::Task}) {
        Fixture fixture(false);
        seed();
        fake::fail(operation, 1, ESP_ERR_NVS_NO_FREE_PAGES);
        const auto expected = operation == Op::Mutex || operation == Op::Queue || operation == Op::Task
                                ? ESP_ERR_NO_MEM : ESP_ERR_NVS_NO_FREE_PAGES;
        assert(ir_service::init() == expected);
        assert(fake::calls(Op::EraseAll) == 0 && fake::calls(Op::NvsEraseKey) == 0);
        assert(fake::state.visible.at("command") == kRecord);
        assert(fake::state.committed.at("command") == kRecord);
        assert(fake::calls(Op::EraseAll) == 0 && fake::calls(Op::NvsEraseKey) == 0);
        assert(!fake::state.storage_open && !fake::state.worker);
        assert(fake::queues() == 0 && fake::semaphores() == 0);
        assert(ir_service::busy());
        // A later explicit initialization may succeed without resetting NVS.
        assert(ir_service::init() == ESP_OK);
        fake::run_worker();
        assert(fake::state.transmissions.empty());
        ir::Waveform loaded;
        assert(ir_service::load("command", loaded) == ESP_OK);
        assert_waveform(loaded);
        assert_idle();
    }
}

void save_requires_pending_and_explicit_overwrite()
{
    Fixture fixture;
    seed("untouched", kOtherRecord);
    assert(ir_service::save_pending("new", false) == ESP_ERR_INVALID_STATE);
    assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
    assert(ir_service::save_pending(nullptr, false) == ESP_ERR_INVALID_ARG);
    assert(ir_service::save_pending("not-valid", false) == ESP_ERR_INVALID_ARG);
    assert(ir_service::save_pending("new", false) == ESP_OK);
    assert(fake::state.committed.at("new") == kRecord);
    assert(fake::calls(Op::NvsSet) == 1 && fake::calls(Op::NvsCommit) == 1);
    assert(fake::logged("Saved new to ir_store; commit and waveform read-back succeeded"));
    // A different pending waveform must not replace an existing name implicitly.
    assert(ir_service::learn(Receiver::Envelope56) == ESP_OK);
    fake::state.logs.clear();
    assert(ir_service::save_pending("new", false) == ESP_ERR_INVALID_STATE);
    assert(fake::state.committed.at("new") == kRecord);
    assert(fake::calls(Op::NvsSet) == 1 && fake::calls(Op::NvsCommit) == 1);
    assert(!fake::logged("Saved new"));
    assert(ir_service::save_pending("new", true) == ESP_OK);
    ir::Waveform loaded;
    assert(ir_service::load("new", loaded) == ESP_OK && loaded.carrier_hz == 56000);
    assert(fake::state.committed.at("untouched") == kOtherRecord);
    assert(fake::calls(Op::EraseAll) == 0);
    assert_idle();
}

void save_query_and_write_failures()
{
    for (auto operation : {Op::NvsGet, Op::NvsSet}) {
        Fixture fixture;
        seed();
        assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
        fake::fail(operation);
        assert(ir_service::save_pending("new", false) == fake::kInjectedError);
        assert(fake::calls(Op::NvsCommit) == 0 && !fake::logged("Saved new"));
        assert(fake::state.visible.count("new") == 0);
        assert(fake::state.committed.at("command") == kRecord);
        assert(fake::calls(Op::NvsEraseKey) == 0 && fake::calls(Op::EraseAll) == 0);
        assert_idle();
    }
}

// A failed commit can leave the new value visible or durable. Either modeled
// outcome must report failure; the service does not promise rollback.
void commit_failure_is_visible()
{
    for (bool durable_before_error : {false, true}) {
        Fixture fixture;
        seed("command", kOtherRecord);
        seed("untouched", kOtherRecord);
        assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
        fake::state.commit_before_error = durable_before_error;
        fake::fail(Op::NvsCommit);
        assert(ir_service::save_pending("command", true) == fake::kInjectedError);
        assert(!fake::logged("Saved command"));
        assert(fake::state.visible.at("command") == kRecord);
        assert(fake::state.committed.at("command") == (durable_before_error ? kRecord : kOtherRecord));
        assert(fake::state.committed.at("untouched") == kOtherRecord);
        assert(fake::calls(Op::NvsGet) == 1); // No false read-back success after commit failure.
        assert(fake::calls(Op::NvsEraseKey) == 0 && fake::calls(Op::EraseAll) == 0);
        assert_idle();
    }
}

void readback_api_failures()
{
    for (unsigned call : {2U, 3U}) {
        Fixture fixture;
        seed("untouched", kOtherRecord);
        assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
        // The first get checks overwrite; the next two read size and content.
        fake::fail(Op::NvsGet, call);
        assert(ir_service::save_pending("new", false) == fake::kInjectedError);
        assert(fake::calls(Op::NvsCommit) == 1 && fake::state.committed.at("new") == kRecord);
        assert(!fake::logged("Saved new"));
        assert(fake::state.committed.at("untouched") == kOtherRecord);
        assert(fake::calls(Op::NvsEraseKey) == 0 && fake::calls(Op::EraseAll) == 0);
        assert_idle();
    }
}

void readback_corruption_and_mismatch()
{
    for (unsigned variant = 0; variant < 4; ++variant) {
        Fixture fixture;
        seed("untouched", kOtherRecord);
        assert(ir_service::learn(Receiver::Envelope38) == ESP_OK);
        fake::state.after_commit = [=] {
            if (variant == 0) {
                fake::state.visible.at("new").back() ^= 1;
            } else {
                // Valid CRCs with one changed field test content comparison
                // independently from record-integrity validation.
                ir::Waveform changed{38000, 33, {560, 560, 560}};
                if (variant == 1) changed.carrier_hz = 40000;
                if (variant == 2) changed.duty_percent = 25;
                if (variant == 3) changed.durations_us[0] = 1000;
                fake::state.visible["new"] = ir::serialize(changed);
            }
        };
        assert(ir_service::save_pending("new", false) == (variant == 0 ? ESP_ERR_INVALID_ARG : ESP_FAIL));
        assert(!fake::logged("Saved new"));
        assert(fake::state.committed.at("untouched") == kOtherRecord);
        assert(fake::calls(Op::NvsEraseKey) == 0 && fake::calls(Op::EraseAll) == 0);
        assert_idle();
    }
}

void corrupt_records_are_rejected_without_erasure()
{
    std::vector<fake::Bytes> records{{}, fake::Bytes(16417, 0), fake::Bytes{0x49, 0x52}, kRecord, kRecord};
    records[3].back() ^= 1;
    records[4][3] = 0x32; // Unknown IRW version.
    for (std::size_t i = 0; i < records.size(); ++i) {
        Fixture fixture;
        seed("bad", records[i]);
        seed("untouched", kOtherRecord);
        ir::Waveform loaded{40000, 25, {1000}};
        const auto expected = i < 2 ? ESP_ERR_INVALID_SIZE : ESP_ERR_INVALID_ARG;
        assert(ir_service::load("bad", loaded) == expected);
        assert(loaded.carrier_hz == 40000 && loaded.duty_percent == 25);
        assert((loaded.durations_us == std::vector<std::uint32_t>{1000}));
        assert(ir_service::send("bad", 0) == expected);
        assert(fake::calls(Op::QueueSend) == 0);
        assert(fake::state.visible.at("bad") == records[i]);
        assert(fake::state.committed.at("untouched") == kOtherRecord);
        assert(fake::calls(Op::NvsEraseKey) == 0 && fake::calls(Op::EraseAll) == 0);
        assert_idle();
    }
}

void missing_and_unreadable_records()
{
    Fixture fixture;
    seed();
    ir::Waveform loaded;
    assert(ir_service::load(nullptr, loaded) == ESP_ERR_INVALID_ARG);
    assert(ir_service::load("missing", loaded) == ESP_ERR_NVS_NOT_FOUND);
    assert(ir_service::send("missing", 0) == ESP_ERR_NVS_NOT_FOUND);
    for (unsigned offset : {1U, 2U}) {
        fake::fail(Op::NvsGet, fake::calls(Op::NvsGet) + offset);
        assert(ir_service::load("command", loaded) == fake::kInjectedError);
    }
    assert(fake::state.visible.at("command") == kRecord);
    assert(fake::calls(Op::EraseAll) == 0);
    assert_idle();
}

void removal_failure_and_success()
{
    for (auto operation : {Op::NvsEraseKey, Op::NvsCommit}) {
        Fixture fixture;
        seed();
        seed("untouched", kOtherRecord);
        fake::fail(operation);
        assert(ir_service::remove("command") == fake::kInjectedError);
        assert(fake::state.committed.at("command") == kRecord);
        assert(fake::state.committed.at("untouched") == kOtherRecord);
        if (operation == Op::NvsEraseKey) assert(fake::calls(Op::NvsCommit) == 0);
        assert(fake::calls(Op::EraseAll) == 0);
        assert_idle();
    }
    Fixture fixture;
    seed();
    seed("untouched", kOtherRecord);
    assert(ir_service::remove("command") == ESP_OK);
    assert(fake::state.committed.count("command") == 0);
    assert(fake::state.committed.at("untouched") == kOtherRecord);
    assert(ir_service::remove("missing") == ESP_ERR_NVS_NOT_FOUND);
    assert(ir_service::remove("invalid name") == ESP_ERR_INVALID_ARG);
    assert(fake::calls(Op::NvsCommit) == 1 && fake::calls(Op::EraseAll) == 0);
    assert_idle();
}

void iterator_failures()
{
    for (auto operation : {Op::NvsFind, Op::NvsInfo, Op::NvsNext}) {
        Fixture fixture;
        assert(ir_service::list() == ESP_OK);
        seed();
        fake::fail(operation, fake::calls(operation) + 1);
        assert(ir_service::list() == fake::kInjectedError);
        assert(fake::iterators() == 0);
        assert(ir_service::list() == ESP_OK);
        assert(fake::iterators() == 0);
        assert(fake::state.committed.at("command") == kRecord && fake::calls(Op::EraseAll) == 0);
        assert_idle();
    }
}
} // namespace

int main(int argc, char **argv)
{
    return service_test::run_cases(argc, argv, {
        {"initialization_failure_preserves_records", initialization_failure_preserves_records},
        {"save_requires_pending_and_explicit_overwrite", save_requires_pending_and_explicit_overwrite},
        {"save_query_and_write_failures", save_query_and_write_failures},
        {"commit_failure_is_visible", commit_failure_is_visible},
        {"readback_api_failures", readback_api_failures},
        {"readback_corruption_and_mismatch", readback_corruption_and_mismatch},
        {"corrupt_records_are_rejected_without_erasure", corrupt_records_are_rejected_without_erasure},
        {"missing_and_unreadable_records", missing_and_unreadable_records},
        {"removal_failure_and_success", removal_failure_and_success},
        {"iterator_failures", iterator_failures},
    });
}
