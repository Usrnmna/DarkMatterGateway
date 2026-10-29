#include "proflame2_controller.hpp"
#include <cassert>
#include <iostream>

using namespace proflame2;
// Synthetic clock bounds are not hardware qualification or production settings.
constexpr Timings timing{100, 10, 200, 2000, 20};
constexpr Capabilities all{true,true,true,true,true,true};
struct Rig {
    Controller controller{timing, all};
    std::uint64_t now = 1000;
    Frame request(Type type, Register reg, std::array<std::uint8_t,3> payload = {}) {
        now += 200;
        Frame frame{};
        assert(controller.next_request(frame, now));
        assert(frame == encode({type,payload,reg}));
        return frame;
    }
    void reply(Type type, Register reg, std::array<std::uint8_t,3> payload = {}) {
        controller.on_reply(encode({type,payload,reg}), ++now);
    }
    void ownership(Type type = Type::Get) {
        request(type, Register::Control, type == Type::Set ? std::array<std::uint8_t,3>{0xc1,0xa0,0} : std::array<std::uint8_t,3>{});
        reply(type == Type::Set ? Type::SetResponse : Type::GetResponse, Register::Control, {0xc1,0xa0,0});
    }
    void baseline(std::uint8_t one = 0x81, std::uint8_t two = 0x8d) {
        ownership(Type::Set); ownership();
        request(Type::Get, Register::State1); reply(Type::GetResponse, Register::State1, {0,one,0});
        request(Type::Get, Register::State2); reply(Type::GetResponse, Register::State2, {0,two,0});
        ownership();
    }
    void idle() { Frame frame{}; assert(!controller.next_request(frame, now + 10000)); }
};

int main()
{
    Rig startup; startup.idle();
    assert(startup.controller.begin_change({Field::Light,2},startup.now) == Outcome::None);
    assert(startup.controller.begin_change({Field::Power,0},startup.now) == Outcome::Busy);
    startup.baseline();
    const Frame patch = startup.request(Type::Set,Register::State1,{0,0xa1,0});
    assert((patch == Frame{0x5d,0,0xa1,0,0x17,0xeb}));
    startup.reply(Type::SetResponse,Register::State1,{0,0xa1,0});
    startup.ownership();
    startup.request(Type::Get,Register::State1); startup.reply(Type::GetResponse,Register::State1,{0,0xa1,0});
    startup.request(Type::Get,Register::State2); startup.reply(Type::GetResponse,Register::State2,{0,0x8d,0});
    startup.ownership();
    assert(startup.controller.snapshot(startup.now).last_outcome == Outcome::ReadbackMatched);
    assert(startup.controller.snapshot(startup.now).readback_valid);
    assert(!startup.controller.snapshot(startup.now + 3000).readback_valid);
    startup.idle();

    Rig fan; assert(fan.controller.begin_change({Field::Fan,3},fan.now) == Outcome::None);
    fan.baseline(); fan.request(Type::Set,Register::State2,{0,0xbd,0});
    fan.controller.tick(fan.now + 100);
    assert(fan.controller.snapshot(fan.now + 100).last_outcome == Outcome::Indeterminate);
    assert(fan.controller.begin_change({Field::Power,0},fan.now + 101) == Outcome::RecoveryRequired);
    fan.idle();

    Rig denied; assert(denied.controller.begin_change({Field::Power,1},denied.now) == Outcome::None);
    denied.request(Type::Set,Register::Control,{0xc1,0xa0,0}); denied.reply(Type::SetResponse,Register::Control);
    assert(denied.controller.snapshot(denied.now).last_outcome == Outcome::ExternalControl); denied.idle();

    Rig refresh; assert(refresh.controller.begin_refresh(refresh.now) == Outcome::None);
    refresh.ownership();
    refresh.request(Type::Get,Register::State1); refresh.reply(Type::GetResponse,Register::State1,{0,1,0});
    refresh.request(Type::Get,Register::State2); refresh.reply(Type::GetResponse,Register::State2,{0,6,0});
    refresh.ownership();
    assert(refresh.controller.snapshot(refresh.now).last_outcome == Outcome::RefreshComplete);
    refresh.idle();

    Rig invalid;
    assert(invalid.controller.begin_change({Field::Fan,7},invalid.now) == Outcome::InvalidRequest);
    assert(invalid.controller.begin_change({static_cast<Field>(99),0},invalid.now) == Outcome::InvalidRequest);
    Controller unsupported(timing, {});
    assert(unsupported.begin_change({Field::Power,1},1000) == Outcome::Unsupported);
    Controller unqualified({}, all);
    assert(unqualified.begin_refresh(1000) == Outcome::Disabled);
    for (std::uint8_t mode : {2,4,8}) {
        Rig rig; assert(rig.controller.begin_change({Field::Power,1},rig.now) == Outcome::None);
        rig.baseline(mode,6);
        assert(rig.controller.snapshot(rig.now).last_outcome == Outcome::Unsupported); rig.idle();
    }
    Rig unchanged; unchanged.controller.begin_change({Field::Light,0},unchanged.now); unchanged.baseline();
    assert(unchanged.controller.snapshot(unchanged.now).last_outcome == Outcome::NoChange); unchanged.idle();

    // Response corruption/mismatch and loss of ownership never reach a state write.
    for (int fault = 0; fault < 4; ++fault) {
        Rig rig; rig.controller.begin_change({Field::Power,1},rig.now);
        rig.ownership(Type::Set); rig.request(Type::Get,Register::Control);
        Frame reply = encode({Type::GetResponse,{0xc1,0xa0,0},Register::Control});
        if (fault == 0) reply[5] ^= 1;
        if (fault == 1) reply = encode({Type::SetResponse,{0xc1,0xa0,0},Register::Control});
        if (fault == 2) reply = encode({Type::GetResponse,{0,1,0},Register::State1});
        if (fault == 3) reply = encode({Type::GetResponse,{},Register::Control});
        rig.controller.on_reply(reply,++rig.now);
        assert(!rig.controller.snapshot(rig.now).readback_valid); rig.idle();
    }
    Rig stale; stale.controller.begin_change({Field::Fan,3},stale.now); stale.baseline();
    stale.now += 3000; Frame frame{}; assert(!stale.controller.next_request(frame,stale.now)); stale.idle();

    Rig late; late.controller.begin_change({Field::Power,1},late.now);
    late.request(Type::Set,Register::Control,{0xc1,0xa0,0});
    late.now += 100; late.controller.tick(late.now);
    assert(late.controller.snapshot(late.now).last_outcome == Outcome::Timeout);
    assert(late.controller.begin_refresh(late.now) == Outcome::None);
    late.reply(Type::SetResponse,Register::Control,{0xc1,0xa0,0});
    assert(!late.controller.busy()); late.idle();
    assert(late.controller.begin_refresh(late.now) == Outcome::None);
    assert(!late.controller.next_request(frame,late.now + 199));
    late.ownership();
    late.request(Type::Get,Register::State1); late.reply(Type::GetResponse,Register::State1,{0,0,0});
    late.request(Type::Get,Register::State2); late.reply(Type::GetResponse,Register::State2,{0,0,0}); late.ownership();
    assert(!late.controller.snapshot(late.now).resync_required); late.idle();
    late.controller.quiesce(late.now);
    assert(late.controller.begin_change({Field::Power,1},late.now) == Outcome::Disabled);
    assert(late.controller.begin_refresh(late.now) == Outcome::Disabled);

    // Each writable field changes only its own bits, with all other fields
    // preserved through a complete, ownership-bracketed readback.
    struct PatchCase { Field field; std::uint8_t value, one, two; Register reg; };
    const PatchCase cases[] = {
        {Field::Power,0,0x80,0x8d,Register::State1},
        {Field::Flame,1,0x81,0x89,Register::State2},
        {Field::Fan,3,0x81,0xbd,Register::State2},
        {Field::Light,2,0xa1,0x8d,Register::State1},
        {Field::Aux,0,0x81,0x85,Register::State2},
        {Field::SplitFlow,0,0x81,0x0d,Register::State2}
    };
    for (const auto &item : cases) {
        Rig rig; assert(rig.controller.begin_change({item.field,item.value},rig.now) == Outcome::None);
        rig.baseline();
        const auto wanted = item.reg == Register::State1 ? item.one : item.two;
        rig.request(Type::Set,item.reg,{0,wanted,0}); rig.reply(Type::SetResponse,item.reg,{0,wanted,0});
        rig.ownership();
        rig.request(Type::Get,Register::State1); rig.reply(Type::GetResponse,Register::State1,{0,item.one,0});
        rig.request(Type::Get,Register::State2); rig.reply(Type::GetResponse,Register::State2,{0,item.two,0});
        rig.ownership();
        const auto result = rig.controller.snapshot(rig.now);
        assert(result.last_outcome == Outcome::ReadbackMatched && result.readback_valid);
        assert(result.state1 == item.one && result.state2 == item.two); rig.idle();
    }
    // A fresh ownership grant never revives an abandoned operation. It takes
    // another deliberate request; no pending actuation backlog exists.
    assert(denied.controller.begin_change({Field::Power,0},denied.now) == Outcome::None);
    denied.baseline(); denied.request(Type::Set,Register::State1,{0,0x80,0});
    denied.controller.quiesce(++denied.now);
    assert(denied.controller.snapshot(denied.now).last_outcome == Outcome::Indeterminate); denied.idle();

    // Ownership loss or mismatched readback after TX is uncertain, never a
    // successful OFF, a retry, or a rollback of other fields.
    for (int failure = 0; failure < 4; ++failure) {
        Rig rig; rig.controller.begin_change({Field::Power,0},rig.now); rig.baseline();
        rig.request(Type::Set,Register::State1,{0,0x80,0});
        if (failure == 0) rig.reply(Type::SetResponse,Register::State1,{0,0x81,0});
        else {
            rig.reply(Type::SetResponse,Register::State1,{0,0x80,0});
            if (failure == 1) {
                rig.request(Type::Get,Register::Control); rig.reply(Type::GetResponse,Register::Control);
            } else {
                rig.ownership();
                rig.request(Type::Get,Register::State1);
                rig.reply(Type::GetResponse,Register::State1,{0,static_cast<std::uint8_t>(failure == 2 ? 0x81 : 0x80),0});
                if (failure == 3) {
                    rig.request(Type::Get,Register::State2); rig.reply(Type::GetResponse,Register::State2,{0,0x8d,0});
                    rig.request(Type::Get,Register::Control); rig.reply(Type::GetResponse,Register::Control);
                }
            }
        }
        assert(rig.controller.snapshot(rig.now).last_outcome == Outcome::Indeterminate);
        assert(!rig.controller.snapshot(rig.now).readback_valid);
        assert(rig.controller.begin_change({Field::Power,1},rig.now) == Outcome::RecoveryRequired); rig.idle();
    }
    Rig invalid_level; invalid_level.controller.begin_change({Field::Fan,0},invalid_level.now);
    invalid_level.baseline(0x81,0x7d);
    assert(invalid_level.controller.snapshot(invalid_level.now).last_outcome == Outcome::Unsupported); invalid_level.idle();

    // A late duplicate arriving between requests invalidates the operation.
    Rig duplicate; duplicate.controller.begin_change({Field::Fan,0},duplicate.now); duplicate.ownership(Type::Set);
    duplicate.reply(Type::SetResponse,Register::Control,{0xc1,0xa0,0});
    assert(duplicate.controller.snapshot(duplicate.now).last_outcome == Outcome::ProtocolError); duplicate.idle();
    std::cout << "Proflame2 controller tests passed\n";
}
