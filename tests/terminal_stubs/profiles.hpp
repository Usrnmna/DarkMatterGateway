#pragma once
#include "ir_core.hpp"

namespace configured {
inline constexpr ir::Profile profiles[] = {
    {"enabled", ir::Kind::Light, true, 2, "light_on", "light_off", "", "", ""},
    {"disabled", ir::Kind::Raw, false, 0, "", "", "", "", ""}
};
struct Action { const char *device; const char *action; const char *code; };
inline constexpr Action actions[] = {
    {"enabled", "on", "light_on"}, {"disabled", "on", "disabled_on"}
};
}
