#include "proflame2_session.hpp"
#include <algorithm>
namespace proflame2 {
Session::Session(Transport &t, bool e, bool q, const Timings &timings, const Capabilities &caps)
    : transport_(t), controller_(timings, caps), timings_(timings), configured_(e && q && valid_timings(timings)) {}

bool Session::init()
{
    if (initialized_) return false;
    initialized_ = true;
    if (!configured_) return true;
    if (!transport_.qualified()) return false;
    // The adapter establishes its hardware inhibit before assigning UART pins.
    if (!transport_.open()) return false;
    if (!transport_.inhibit(true) || !transport_.flush()) {
        transport_.close();
        return false;
    }
    active_ = true;
    return true;
}

Outcome Session::start(bool write, Change change, std::uint64_t now)
{
    if (!active_) return Outcome::Disabled;
    const auto result = write ? controller_.begin_change(change, now) : controller_.begin_refresh(now);
    if (result != Outcome::None) return result;
    received_ = 0;
    if (!transport_.flush()) {
        fail(now, true);
        return snapshot(now).last_outcome;
    }
    if (++operation_id_ == 0) ++operation_id_;
    return Outcome::None;
}

Outcome Session::refresh(std::uint64_t now) { return start(false, {Field::Power,0}, now); }
Outcome Session::change(Change change, std::uint64_t now) { return start(true, change, now); }

void Session::fail(std::uint64_t now, bool fatal)
{
    controller_.on_transport_error(now);
    received_ = 0;
    const bool inhibited = transport_.inhibit(true);
    const bool flushed = transport_.flush();
    if (fatal || !inhibited || !flushed) quiesce(now);
}

void Session::poll(std::uint64_t now)
{
    if (!active_) return;
    const bool was_busy = controller_.busy();
    controller_.tick(now);
    if (was_busy && !controller_.busy()) {
        received_ = 0;
        if (!transport_.flush()) quiesce(now);
        return;
    }
    if (received_ && now - last_byte_ms_ >= timings_.inter_byte_ms) {
        fail(now, false);
        return;
    }
    // One byte beyond a frame detects coalesced/unsolicited data. No sliding
    // checksum search or automatic retry is allowed after framing uncertainty.
    std::uint8_t bytes[7]{};
    const int count = transport_.read(bytes, sizeof(bytes));
    if (count < 0 || count > static_cast<int>(sizeof(bytes)) || received_ + static_cast<std::size_t>(count) > receive_.size()) {
        fail(now, false);
        return;
    }
    if (count > 0) {
        std::copy_n(bytes, count, receive_.begin() + received_);
        received_ += static_cast<std::size_t>(count);
        last_byte_ms_ = now;
        if (received_ == receive_.size()) {
            received_ = 0;
            controller_.on_reply(receive_, now);
        }
    }
    Frame request{};
    if (received_ || !controller_.next_request(request, now)) return;
    if (!transport_.inhibit(false)) { fail(now, true); return; }
    const int written = transport_.write(request);
    const bool inhibited = transport_.inhibit(true);
    // A failed TX may leave bytes in a hardware FIFO. Latch the service closed;
    // a later request must never release those bytes by reopening the gate.
    if (written != static_cast<int>(request.size()) || !inhibited) fail(now, true);
}

void Session::quiesce(std::uint64_t now)
{
    controller_.quiesce(now);
    received_ = 0;
    if (active_) {
        transport_.inhibit(true);
        transport_.close();
    }
    active_ = false;
    initialized_ = true; // Reinitialization requires a fresh process/session.
}

Snapshot Session::snapshot(std::uint64_t now) const
{
    auto result = controller_.snapshot(now);
    if (!active_) {
        result.readback_valid = false;
        result.ownership = Ownership::Unknown;
        if (result.last_outcome != Outcome::Indeterminate) result.last_outcome = Outcome::Disabled;
    }
    return result;
}
}
