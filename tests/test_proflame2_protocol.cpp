#include "proflame2_protocol.hpp"
#include <cassert>
#include <iostream>

using namespace proflame2;
int main()
{
    const Frame vectors[] = {
        {0x5e,0,0,0,0x16,0x48}, {0x5d,0xc1,0xa0,0,0x16,0x2a},
        {0xad,0xc1,0xa0,0,0x16,0xda}, {0xad,0,0,0,0x16,0xbb},
        {0xae,0,0x61,0,0x17,0xd8}, {0x5d,0,0x80,0,0x18,0xc5}
    };
    for (const auto &frame : vectors) {
        Message message;
        assert(decode(frame, message));
        assert(encode(message) == frame);
        for (unsigned offset = 0; offset < 6; ++offset) {
            Frame corrupted = frame;
            corrupted[offset] ^= 1;
            Message unchanged = message;
            assert(!decode(corrupted, unchanged));
            assert(encode(unchanged) == frame);
        }
    }
    // Correct XOR cannot legitimize an unknown command/register or payload.
    for (Frame frame : {Frame{0xff,0,0,0,0x16,0}, Frame{0xae,0,0,0,0x19,0},
                        Frame{0x5e,0,1,0,0x17,0}, Frame{0xae,1,0,0,0x17,0},
                        Frame{0xae,0xc1,0xa1,0,0x16,0}}) {
        frame[5] = checksum(frame);
        Message message;
        assert(!decode(frame, message));
    }
    std::cout << "Proflame2 protocol tests passed\n";
}
