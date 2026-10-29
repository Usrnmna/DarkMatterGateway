#pragma once

#include <cstddef>
#include <cstdio>

namespace console_line {
constexpr std::size_t kMaxCharacters = 255;

// Read one terminated ASCII command. Invalid or oversized lines are consumed
// completely and rejected. Successful buffers use malloc for linenoiseFree().
char *read(std::FILE *input, std::FILE *output, const char *prompt);
}
