#pragma once
#include "esp_err.h"
#include "proflame2_controller.hpp"

namespace proflame2_service {
// Disabled initialization touches no UART or GPIO and starts no worker.
esp_err_t init();
// ESP_OK means accepted, not appliance action or successful readback. The worker
// prints a separate result with a local operation ID, absent from wire frames.
esp_err_t refresh();
esp_err_t request(proflame2::Change change);
proflame2::Snapshot snapshot();
esp_err_t print_status(); // Cached diagnostics only; never transmits.
bool busy();
// Sticky traffic inhibition; never sends OFF or claims to stop combustion.
void quiesce();
// Atomically reject an active operation or quiesce before scheduling a reset.
esp_err_t prepare_reset();
} // namespace proflame2_service
