#include "proflame2_session.hpp"
#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <vector>

using namespace proflame2;
constexpr Timings timing{100,10,200,2000,20}; // Host-only synthetic bounds.
constexpr Capabilities all{true,true,true,true,true,true};
struct FakeTransport : Transport {
    bool qualification = true, open_ok = true, inhibit_ok = true, flush_ok = true;
    bool disabled = true, opened = false, error = false;
    int opens = 0, calls = 0, chunk = 6, write_size = 6;
    std::deque<std::uint8_t> incoming;
    std::vector<Frame> sent;
    bool qualified() const override { return qualification; }
    bool open() override { ++calls; ++opens; opened = open_ok; return opened; }
    bool inhibit(bool value) override { ++calls; disabled = value; return inhibit_ok; }
    int read(std::uint8_t *data, std::size_t length) override {
        ++calls;
        if (error) { error = false; return -1; }
        const auto count = std::min({length, incoming.size(), static_cast<std::size_t>(chunk)});
        for (std::size_t i = 0; i < count; ++i) { data[i] = incoming.front(); incoming.pop_front(); }
        return static_cast<int>(count);
    }
    int write(const Frame &frame) override {
        ++calls; assert(opened && !disabled); sent.push_back(frame); return write_size;
    }
    bool flush() override { ++calls; incoming.clear(); return flush_ok; }
    void close() override { ++calls; opened = false; }
    void reply(Type type, Register reg, std::array<std::uint8_t,3> payload = {}) {
        const auto frame = encode({type,payload,reg});
        incoming.insert(incoming.end(),frame.begin(),frame.end());
    }
};

int main()
{
    for (int blocked = 0; blocked < 3; ++blocked) {
        FakeTransport uart;
        uart.qualification = blocked != 2;
        Session session(uart, blocked != 0, blocked != 1, timing, all);
        session.init(); session.poll(1000);
        assert(session.refresh(1000) == Outcome::Disabled);
        assert(session.change({Field::Power,1},1000) == Outcome::Disabled);
        session.quiesce(1000); session.poll(2000);
        assert(uart.sent.empty() && uart.calls == 0);
    }
    FakeTransport broken; broken.open_ok = false;
    Session failed(broken,true,true,timing,all);
    assert(!failed.init()); assert(!failed.active()); assert(broken.sent.empty());

    FakeTransport uart;
    Session session(uart,true,true,timing,all);
    assert(session.init()); session.poll(1000); assert(uart.sent.empty());
    assert(session.refresh(1000) == Outcome::None);
    assert(session.change({Field::Power,1},1001) == Outcome::Busy);
    session.poll(1199); assert(uart.sent.empty()); session.poll(1200);
    assert(uart.sent.size() == 1 && uart.sent.back()[0] == 0x5e && uart.disabled);
    // Every possible split in a six-byte reply is assembled without sliding.
    uart.chunk = 1; uart.reply(Type::GetResponse,Register::Control,{0xc1,0xa0,0});
    for (unsigned i = 0; i < 6; ++i) session.poll(1201 + i);
    session.poll(1216); assert(uart.sent.size() == 2);
    uart.chunk = 6; uart.reply(Type::GetResponse,Register::State1,{0,1,0}); session.poll(1217);
    session.poll(1227); uart.reply(Type::GetResponse,Register::State2,{0,6,0}); session.poll(1228);
    session.poll(1238); uart.reply(Type::GetResponse,Register::Control,{0xc1,0xa0,0}); session.poll(1239);
    assert(!session.busy() && session.snapshot(1239).readback_valid);
    assert(session.operation_id() == 1);
    for (auto &frame : uart.sent) assert(frame[0] == 0x5e);
    session.poll(9000); assert(uart.sent.size() == 4);
    session.quiesce(9001); assert(uart.disabled && !uart.opened);
    assert(session.refresh(9002) == Outcome::Disabled);
    assert(!session.init()); session.poll(10000); assert(uart.sent.size() == 4);

    for (int fault = 0; fault < 5; ++fault) {
        FakeTransport bus;
        Session test(bus,true,true,timing,all); assert(test.init());
        test.refresh(1000); test.poll(1200);
        if (fault == 0) { bus.incoming.push_back(0xae); test.poll(1201); test.poll(1221); }
        if (fault == 1) { bus.error = true; test.poll(1201); }
        if (fault == 2) { test.poll(1300); bus.reply(Type::GetResponse,Register::Control,{0xc1,0xa0,0}); test.poll(1301); }
        if (fault == 3 || fault == 4) {
            bus.chunk = fault == 3 ? 7 : 6;
            bus.reply(Type::GetResponse,Register::Control,{0xc1,0xa0,0}); bus.incoming.push_back(0xff);
            test.poll(1201); test.poll(1202); test.poll(1222);
        }
        assert(!test.busy() && !test.snapshot(1400).readback_valid);
        assert(test.snapshot(1400).resync_required);
        assert(test.change({Field::Power,1},1400) == Outcome::RecoveryRequired);
        test.poll(5000); assert(bus.sent.size() == 1 && bus.disabled);
    }
    // A short state write is never retried and is reported as indeterminate.
    FakeTransport short_bus;
    Session short_write(short_bus,true,true,timing,all); assert(short_write.init());
    short_write.change({Field::Power,1},1000); short_write.poll(1200);
    short_bus.reply(Type::SetResponse,Register::Control,{0xc1,0xa0,0}); short_write.poll(1201);
    short_write.poll(1211); short_bus.reply(Type::GetResponse,Register::Control,{0xc1,0xa0,0}); short_write.poll(1212);
    short_write.poll(1222); short_bus.reply(Type::GetResponse,Register::State1,{0,0,0}); short_write.poll(1223);
    short_write.poll(1233); short_bus.reply(Type::GetResponse,Register::State2,{0,0,0}); short_write.poll(1234);
    short_write.poll(1244); short_bus.reply(Type::GetResponse,Register::Control,{0xc1,0xa0,0}); short_write.poll(1245);
    short_bus.write_size = 3; short_write.poll(1255);
    assert(short_write.snapshot(1255).last_outcome == Outcome::Indeterminate);
    short_write.poll(9000); assert(short_bus.sent.size() == 6 && short_bus.disabled);
    std::cout << "Proflame2 session/transport tests passed\n";
}
