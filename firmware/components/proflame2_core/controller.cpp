#include "proflame2_controller.hpp"
namespace proflame2 {
namespace {
constexpr std::array<std::uint8_t, 3> owner_token{0xc1, 0xa0, 0};
bool valid_levels(std::uint8_t one, std::uint8_t two)
{
    return ((one >> 4) & 7) <= 6 && ((two >> 4) & 7) <= 6 && (two & 7) <= 6;
}
}

bool valid_timings(const Timings &t)
{
    // Structural bounds only; these checks do not qualify physical timings.
    return t.response_timeout_ms > 0 && t.response_timeout_ms <= 60000 &&
        t.inter_request_ms > 0 && t.inter_request_ms <= 60000 &&
        t.inter_byte_ms > 0 && t.inter_byte_ms <= t.response_timeout_ms &&
        t.resync_quiet_ms >= t.response_timeout_ms + t.inter_byte_ms &&
        t.resync_quiet_ms <= 120000 && t.snapshot_max_age_ms > t.inter_request_ms &&
        t.snapshot_max_age_ms <= 120000;
}

const char *outcome_name(Outcome result)
{
    switch (result) {
    case Outcome::None: return "InProgress";
    case Outcome::RefreshComplete: return "RefreshComplete";
    case Outcome::ReadbackMatched: return "ReadbackMatched";
    case Outcome::NoChange: return "NoChange";
    case Outcome::Disabled: return "Disabled";
    case Outcome::Busy: return "Busy";
    case Outcome::InvalidRequest: return "InvalidRequest";
    case Outcome::Unsupported: return "Unsupported";
    case Outcome::ExternalControl: return "ExternalControl";
    case Outcome::Timeout: return "Timeout";
    case Outcome::ProtocolError: return "ProtocolError";
    case Outcome::Indeterminate: return "Indeterminate";
    case Outcome::RecoveryRequired: return "RecoveryRequired";
    }
    return "Unknown";
}

const char *ownership_name(Ownership owner)
{
    switch (owner) {
    case Ownership::Unknown: return "Unknown";
    case Ownership::NotGranted: return "NotGranted";
    case Ownership::Granted: return "Granted";
    }
    return "Unknown";
}

Controller::Controller(const Timings &t, const Capabilities &c)
    : timings_(t), capabilities_(c), enabled_(valid_timings(t)) {}

Outcome Controller::begin(bool change, std::uint64_t now)
{
    if (!enabled_) return Outcome::Disabled;
    if (busy()) return Outcome::Busy;
    if (change && state_.resync_required) return Outcome::RecoveryRequired;
    changing_ = change;
    wrote_state_ = waiting_ = false;
    initial_quiet_ = true;
    activity_ms_ = now;
    baseline_ms_ = 0;
    state_.readback_valid = false;
    state_.ownership = Ownership::Unknown;
    state_.last_outcome = Outcome::None;
    stage_ = change ? Stage::Claim : Stage::Confirm;
    return Outcome::None;
}

Outcome Controller::begin_refresh(std::uint64_t now) { return begin(false, now); }

Outcome Controller::begin_change(Change change, std::uint64_t now)
{
    if (!enabled_) return Outcome::Disabled;
    if (busy()) return Outcome::Busy;
    const auto field = static_cast<unsigned>(change.field);
    if (field >= capabilities_.size()) return Outcome::InvalidRequest;
    const bool level = change.field == Field::Flame || change.field == Field::Fan || change.field == Field::Light;
    if (change.value > (level ? 6 : 1)) return Outcome::InvalidRequest;
    if (!capabilities_[field]) return Outcome::Unsupported;
    const auto result = begin(true, now);
    if (result == Outcome::None) change_ = change;
    return result;
}

bool Controller::baseline_fresh(std::uint64_t now) const
{
    return now >= baseline_ms_ && now - baseline_ms_ < timings_.snapshot_max_age_ms;
}

void Controller::finish(Outcome result, std::uint64_t now, bool recovery)
{
    const bool success = result == Outcome::RefreshComplete || result == Outcome::ReadbackMatched || result == Outcome::NoChange;
    if (wrote_state_ && !success) result = Outcome::Indeterminate;
    state_.last_outcome = result;
    state_.readback_valid = success;
    if (success) {
        state_.state1 = baseline1_;
        state_.state2 = baseline2_;
        state_.observed_ms = baseline_ms_;
        state_.resync_required = false;
    } else {
        if (state_.ownership != Ownership::NotGranted) state_.ownership = Ownership::Unknown;
        state_.resync_required = state_.resync_required || recovery;
    }
    stage_ = Stage::Idle;
    waiting_ = wrote_state_ = false;
    activity_ms_ = now;
}

void Controller::tick(std::uint64_t now)
{
    if (waiting_ && now - sent_ms_ >= timings_.response_timeout_ms)
        finish(Outcome::Timeout, now, true);
    else if (stage_ == Stage::Write && !baseline_fresh(now))
        finish(Outcome::Timeout, now, true);
}

bool Controller::next_request(Frame &frame, std::uint64_t now)
{
    tick(now);
    if (!enabled_ || !busy() || waiting_) return false;
    const auto delay = initial_quiet_ ? timings_.resync_quiet_ms : timings_.inter_request_ms;
    if (now - activity_ms_ < delay) return false;
    pending_ = {Type::Get, {}, Register::Control};
    switch (stage_) {
    case Stage::Claim: pending_ = {Type::Set, owner_token, Register::Control}; break;
    case Stage::State1: case Stage::Read1: pending_.reg = Register::State1; break;
    case Stage::State2: case Stage::Read2: pending_.reg = Register::State2; break;
    case Stage::Write:
        pending_ = {Type::Set, {0, wanted_, 0}, write_register_};
        // Even a short UART write may have reached the receiver.
        wrote_state_ = true;
        break;
    default: break;
    }
    frame = encode(pending_);
    sent_ms_ = now;
    waiting_ = true;
    initial_quiet_ = false;
    return true;
}

void Controller::on_reply(const Frame &frame, std::uint64_t now)
{
    const bool timed_out = waiting_ && now - sent_ms_ >= timings_.response_timeout_ms;
    tick(now);
    if (timed_out) return; // Retain the timeout/indeterminate result of this operation.
    if (!enabled_) return;
    Message reply;
    const Type expected = pending_.type == Type::Get ? Type::GetResponse : Type::SetResponse;
    if (!waiting_ || !decode(frame, reply) || reply.type != expected || reply.reg != pending_.reg) {
        on_transport_error(now);
        return;
    }
    waiting_ = false;
    activity_ms_ = now;
    if (reply.reg == Register::Control) {
        if (reply.payload != owner_token) {
            state_.ownership = Ownership::NotGranted;
            finish(Outcome::ExternalControl, now, wrote_state_);
            return;
        }
        state_.ownership = Ownership::Granted;
    } else if (pending_.type == Type::Set && reply.payload != pending_.payload) {
        finish(Outcome::ProtocolError, now, true);
        return;
    }
    switch (stage_) {
    case Stage::Claim: stage_ = Stage::Confirm; break;
    case Stage::Confirm: stage_ = Stage::State1; break;
    case Stage::State1:
        baseline1_ = reply.payload[1]; baseline_ms_ = now; stage_ = Stage::State2; break;
    case Stage::State2:
        baseline2_ = reply.payload[1]; stage_ = Stage::Before; break;
    case Stage::Before: {
        if (!baseline_fresh(now)) { finish(Outcome::Timeout, now, true); break; }
        if (!valid_levels(baseline1_, baseline2_) || (baseline1_ & 0x0c) || (changing_ && (baseline1_ & 0x02))) {
            finish(Outcome::Unsupported, now, false); break;
        }
        if (!changing_) { finish(Outcome::RefreshComplete, now, false); break; }
        std::uint8_t mask = 0, shift = 0;
        write_register_ = Register::State2;
        switch (change_.field) {
        case Field::Power: write_register_ = Register::State1; mask = 0x01; break;
        case Field::Light: write_register_ = Register::State1; mask = 0x70; shift = 4; break;
        case Field::Flame: mask = 0x07; break;
        case Field::Fan: mask = 0x70; shift = 4; break;
        case Field::Aux: mask = 0x08; shift = 3; break;
        case Field::SplitFlow: mask = 0x80; shift = 7; break;
        }
        const auto original = write_register_ == Register::State1 ? baseline1_ : baseline2_;
        wanted_ = static_cast<std::uint8_t>((original & ~mask) | (change_.value << shift));
        if (wanted_ == original) finish(Outcome::NoChange, now, false);
        else stage_ = Stage::Write;
        break;
    }
    case Stage::Write: stage_ = Stage::After; break;
    case Stage::After: stage_ = Stage::Read1; break;
    case Stage::Read1:
        if (reply.payload[1] != (write_register_ == Register::State1 ? wanted_ : baseline1_)) {
            finish(Outcome::Indeterminate, now, true); break;
        }
        baseline1_ = reply.payload[1]; baseline_ms_ = now; stage_ = Stage::Read2; break;
    case Stage::Read2:
        if (reply.payload[1] != (write_register_ == Register::State2 ? wanted_ : baseline2_)) {
            finish(Outcome::Indeterminate, now, true); break;
        }
        baseline2_ = reply.payload[1]; stage_ = Stage::Final; break;
    case Stage::Final:
        finish(baseline_fresh(now) ? Outcome::ReadbackMatched : Outcome::Indeterminate, now, !baseline_fresh(now));
        break;
    case Stage::Idle: break;
    }
}

void Controller::on_transport_error(std::uint64_t now)
{
    if (!enabled_) return;
    // Late bytes cannot turn an uncertain completed write into a definite failure.
    const auto result = !busy() && state_.last_outcome == Outcome::Indeterminate ? Outcome::Indeterminate : Outcome::ProtocolError;
    finish(result, now, true);
}

Snapshot Controller::snapshot(std::uint64_t now) const
{
    auto result = state_;
    if (!enabled_ || (result.readback_valid && (now < result.observed_ms || now - result.observed_ms >= timings_.snapshot_max_age_ms))) {
        result.readback_valid = false;
        result.ownership = Ownership::Unknown;
    }
    return result;
}
bool Controller::busy() const { return stage_ != Stage::Idle; }

void Controller::quiesce(std::uint64_t now)
{
    const auto result = !busy() && state_.last_outcome == Outcome::Indeterminate ? Outcome::Indeterminate : Outcome::Disabled;
    finish(result, now, wrote_state_);
    enabled_ = false;
}
}
