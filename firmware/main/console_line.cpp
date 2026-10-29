#include "console_line.hpp"

#include <cstdlib>
#include <cstring>

namespace console_line {
char *read(std::FILE *input, std::FILE *output, const char *prompt)
{
    char buffer[kMaxCharacters + 1]{};
    std::size_t length = 0;
    bool rejected = false;
    std::fputs(prompt, output);
    std::fflush(output);
    for (;;) {
        const int character = std::fgetc(input);
        if (character == EOF) {
            // An incomplete command is never dispatched after disconnect/EOF.
            return nullptr;
        }
        if (character == '\n' || character == '\r') break;
        if (rejected) continue;
        if (character == '\b' || character == 127) {
            if (length != 0) {
                buffer[--length] = '\0';
                std::fputs("\b \b", output);
                std::fflush(output);
            }
        } else if (character < 32 || character > 126 || length == kMaxCharacters) {
            // Drain to the line boundary; a valid prefix must not execute.
            rejected = true;
        } else {
            buffer[length++] = static_cast<char>(character);
            buffer[length] = '\0';
            std::fputc(character, output);
            std::fflush(output);
        }
    }
    std::fputc('\n', output);
    if (rejected) {
        std::fputs("ERROR console line rejected: maximum 255 printable ASCII characters\n", output);
        std::fflush(output);
        return nullptr;
    }
    std::fflush(output);
    if (length == 0) return nullptr;
    auto *line = static_cast<char *>(std::malloc(length + 1));
    if (line) std::memcpy(line, buffer, length + 1);
    return line;
}
}

// ESP-IDF retains transport setup, command dispatch, and result reporting. Its
// pinned linenoise implementation is bypassed only for probing and line input.
extern "C" int __wrap_linenoiseProbe()
{
    return -1;
}

extern "C" char *__wrap_linenoise(const char *prompt)
{
    return console_line::read(stdin, stdout, prompt);
}
