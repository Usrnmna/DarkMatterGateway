#pragma once
// HOST TEST ONLY: a forged assertion must not bypass unassigned physical pins.
// Never selected by the configuration generator or target build.
#include "proflame2_controller.hpp"
namespace proflame2_config {
inline constexpr bool kEnabled = true, kQualifiedForControl = true;
inline constexpr const char *kDeviceId = "synthetic";
inline constexpr proflame2::Timings kTimings{100,10,200,2000,20};
inline constexpr proflame2::Capabilities kCapabilities{true,true,true,true,true,true};
}
