#pragma once

#include <array>
#include <cstdint>

namespace proflame2 {
using Frame = std::array<std::uint8_t, 6>;
enum class Register : std::uint8_t { Control = 0x16, State1 = 0x17, State2 = 0x18 };
enum class Type : std::uint8_t { Set = 0x5d, Get = 0x5e, SetResponse = 0xad, GetResponse = 0xae };
struct Message {
    Type type = Type::Get;
    std::array<std::uint8_t, 3> payload{};
    Register reg = Register::Control;
};

// Explicit six-byte serialization; no compiler packing or host byte order.
std::uint8_t checksum(const Frame &frame);
Frame encode(const Message &message);
// Reject invalid types, registers, XOR or payload shape; leave output unchanged.
bool decode(const Frame &frame, Message &message);
} // namespace proflame2
