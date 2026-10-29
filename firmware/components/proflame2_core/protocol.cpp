#include "proflame2_protocol.hpp"
namespace proflame2 {
std::uint8_t checksum(const Frame &frame)
{
    return frame[0] ^ frame[1] ^ frame[2] ^ frame[3] ^ frame[4];
}

Frame encode(const Message &message)
{
    Frame frame{static_cast<std::uint8_t>(message.type), message.payload[0],
                message.payload[1], message.payload[2], static_cast<std::uint8_t>(message.reg), 0};
    frame[5] = checksum(frame);
    return frame;
}

bool decode(const Frame &frame, Message &message)
{
    if (checksum(frame) != frame[5]) return false;
    const auto type = static_cast<Type>(frame[0]);
    const auto reg = static_cast<Register>(frame[4]);
    if (type != Type::Get && type != Type::Set && type != Type::GetResponse && type != Type::SetResponse)
        return false;
    if (reg != Register::Control && reg != Register::State1 && reg != Register::State2) return false;
    const std::array<std::uint8_t, 3> payload{frame[1], frame[2], frame[3]};
    const bool zero = payload == std::array<std::uint8_t, 3>{0, 0, 0};
    const bool token = payload == std::array<std::uint8_t, 3>{0xc1, 0xa0, 0};
    if (type == Type::Get && !zero) return false;
    if (reg == Register::Control) {
        if (!zero && !token) return false;
        if (type == Type::Set && !token) return false;
    } else if (frame[1] != 0 || frame[3] != 0) {
        return false;
    }
    message = {type, payload, reg};
    return true;
}
}
