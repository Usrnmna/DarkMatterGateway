#pragma once
#include "ir_core.hpp"

namespace configured {
// Mutable test inputs exercise the generator's supported maximum of eight
// profiles. Production profiles remain generated immutable configuration.
inline ir::Profile profiles[8]{};
}
