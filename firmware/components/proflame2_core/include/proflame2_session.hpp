#pragma once
#include "proflame2_controller.hpp"
#include <cstddef>

namespace proflame2 {
// Calls are serialized. Implementations must use bounded, nonblocking reads;
// negative results include UART overflow/framing/driver errors. write() returns
// only after bounded TX completion, before inhibit is asserted again.
class Transport {
public:
    virtual ~Transport() = default;
    virtual bool qualified() const = 0;
    virtual bool open() = 0;
    virtual bool inhibit(bool disabled) = 0;
    virtual int read(std::uint8_t *bytes, std::size_t capacity) = 0;
    virtual int write(const Frame &frame) = 0;
    virtual bool flush() = 0;
    virtual void close() = 0;
};

// Byte framing, transport lifetime and operation bookkeeping around Controller.
// No background polling or reconnect occurs. IDs are local, never wire fields.
class Session {
public:
    Session(Transport &transport, bool enabled, bool qualified,
            const Timings &timings, const Capabilities &capabilities);
    bool init();
    Outcome refresh(std::uint64_t now_ms);
    Outcome change(Change change, std::uint64_t now_ms);
    void poll(std::uint64_t now_ms);
    void quiesce(std::uint64_t now_ms);
    bool active() const { return active_; }
    bool busy() const { return active_ && controller_.busy(); }
    Snapshot snapshot(std::uint64_t now_ms) const;
    std::uint32_t operation_id() const { return operation_id_; }
private:
    Outcome start(bool write, Change change, std::uint64_t now_ms);
    void fail(std::uint64_t now_ms, bool fatal);
    Transport &transport_;
    Controller controller_;
    Timings timings_;
    Frame receive_{};
    std::size_t received_ = 0;
    std::uint64_t last_byte_ms_ = 0;
    std::uint32_t operation_id_ = 0;
    bool configured_ = false, active_ = false, initialized_ = false;
};
} // namespace proflame2
