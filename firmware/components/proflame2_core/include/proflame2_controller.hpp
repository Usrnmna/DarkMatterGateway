#pragma once
#include "proflame2_protocol.hpp"

namespace proflame2 {
enum class Field { Power, Flame, Fan, Light, Aux, SplitFlow };
using Capabilities = std::array<bool, 6>;
struct Change { Field field; std::uint8_t value; };
enum class Ownership { Unknown, NotGranted, Granted };
enum class Outcome {
    None, RefreshComplete, ReadbackMatched, NoChange, Disabled, Busy, InvalidRequest,
    Unsupported, ExternalControl, Timeout, ProtocolError, Indeterminate, RecoveryRequired
};
struct Timings {
    std::uint32_t response_timeout_ms = 0, inter_request_ms = 0;
    std::uint32_t resync_quiet_ms = 0, snapshot_max_age_ms = 0, inter_byte_ms = 0;
};
struct Snapshot {
    Ownership ownership = Ownership::Unknown;
    std::uint8_t state1 = 0, state2 = 0;
    std::uint64_t observed_ms = 0;
    bool readback_valid = false, resync_required = false;
    Outcome last_outcome = Outcome::None;
};
bool valid_timings(const Timings &timings);
const char *outcome_name(Outcome outcome);
const char *ownership_name(Ownership ownership);

// A single bounded operation. Calls are serialized by the service, with a
// monotonic millisecond clock. This policy does not prove physical arbitration.
class Controller {
public:
    Controller(const Timings &timings, const Capabilities &capabilities);
    Outcome begin_refresh(std::uint64_t now_ms);
    Outcome begin_change(Change change, std::uint64_t now_ms);
    bool next_request(Frame &frame, std::uint64_t now_ms);
    void on_reply(const Frame &frame, std::uint64_t now_ms);
    void on_transport_error(std::uint64_t now_ms);
    void tick(std::uint64_t now_ms);
    Snapshot snapshot(std::uint64_t now_ms) const;
    bool busy() const;
    void quiesce(std::uint64_t now_ms);
private:
    enum class Stage { Idle, Claim, Confirm, State1, State2, Before, Write, After, Read1, Read2, Final };
    Outcome begin(bool change, std::uint64_t now_ms);
    void finish(Outcome result, std::uint64_t now_ms, bool recovery);
    bool baseline_fresh(std::uint64_t now_ms) const;
    Timings timings_;
    Capabilities capabilities_;
    Snapshot state_;
    Stage stage_ = Stage::Idle;
    Change change_{Field::Power, 0};
    Message pending_;
    std::uint64_t activity_ms_ = 0, sent_ms_ = 0, baseline_ms_ = 0;
    std::uint8_t baseline1_ = 0, baseline2_ = 0, wanted_ = 0;
    Register write_register_ = Register::State1;
    bool enabled_ = false, changing_ = false, waiting_ = false;
    bool wrote_state_ = false, initial_quiet_ = true;
};
} // namespace proflame2
