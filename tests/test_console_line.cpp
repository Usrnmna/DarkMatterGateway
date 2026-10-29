#include "console_line.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

extern "C" int linenoiseProbe();

namespace {
// Temporary streams exercise the actual line reader without serial hardware.
struct Streams {
    std::FILE *input = std::tmpfile();
    std::FILE *output = std::tmpfile();
    explicit Streams(const std::string &data)
    {
        assert(input && output);
        assert(std::fwrite(data.data(), 1, data.size(), input) == data.size());
        std::rewind(input);
    }
    ~Streams() { std::fclose(input); std::fclose(output); }
    char *read() { return console_line::read(input, output, "ir> "); }
};

void expect_line(Streams &streams, const std::string &expected)
{
    auto *line = streams.read();
    assert(line && std::string(line) == expected);
    std::free(line);
}
}

int main()
{
    assert(linenoiseProbe() != 0);
    Streams normal("send light_on 0\r\ncodes\n");
    expect_line(normal, "send light_on 0");
    assert(normal.read() == nullptr); // The LF in CRLF is an empty line.
    expect_line(normal, "codes");
    assert(normal.read() == nullptr);

    Streams limit(std::string(console_line::kMaxCharacters, 'x') + "\n");
    expect_line(limit, std::string(console_line::kMaxCharacters, 'x'));
    Streams excess(std::string(console_line::kMaxCharacters + 1, 'x') + "send light_on 0\ncodes\n");
    assert(excess.read() == nullptr);
    expect_line(excess, "codes");

    for (int control : {0, 3, 9, 27, 128, 255}) {
        Streams invalid(std::string("send light_on 0") + static_cast<char>(control) + "\ncodes\n");
        assert(invalid.read() == nullptr);
        expect_line(invalid, "codes");
    }
    Streams editing("\b\177codex\b\bs\n");
    expect_line(editing, "cods");
    Streams cancelled("send light_on 0\003\b\ncodes\n");
    assert(cancelled.read() == nullptr);
    expect_line(cancelled, "codes");
    Streams incomplete("send light_on 0");
    assert(incomplete.read() == nullptr);
    Streams empty("\n");
    assert(empty.read() == nullptr);
    std::cout << "Bounded console line tests passed\n";
}
