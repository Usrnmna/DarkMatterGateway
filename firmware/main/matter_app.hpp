#pragma once

#include "esp_err.h"

namespace matter_app {
esp_err_t init();
// Schedule stack operations from the console or button task.
esp_err_t open_commissioning();
esp_err_t reset_fabrics();
} // namespace matter_app
