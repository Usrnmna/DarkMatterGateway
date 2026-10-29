#pragma once

#include "esp_err.h"
#include "ir_core.hpp"

namespace ir_service {

enum class Receiver { Carrier, Envelope38, Envelope56 };

// Initialize GPIO inhibit, queue, storage, and the serialized IR worker.
esp_err_t init();

// Learning is synchronous for the console caller. It never overwrites a saved
// command: capture to RAM, inspect, and explicitly save with save_pending().
esp_err_t learn(Receiver receiver);
// Commit the pending capture only after external confirmation. An existing name
// requires overwrite=true; success includes validation and exact read-back.
esp_err_t save_pending(const char *name, bool overwrite);
// Validate a stored record before returning its waveform; invalid records remain
// available for diagnosis rather than being erased automatically.
esp_err_t load(const char *name, ir::Waveform &waveform);
// Erase and commit one named record without affecting other codes or fabrics.
esp_err_t remove(const char *name);
esp_err_t list();
esp_err_t print_pending();
esp_err_t print_saved(const char *name);

// Queue a bounded copy of a validated stored command. ESP_OK means queued;
// successful local transmission is reported separately and proves no delivery.
esp_err_t send(const char *name, unsigned zone, unsigned repeats = 1,
               unsigned gap_ms = 40);
// Explicitly test the current RAM capture before confirming and saving it.
esp_err_t send_pending(unsigned zone, unsigned repeats = 1, unsigned gap_ms = 40);
// Busy includes capture, active/queued transmission, and a latched driver fault.
bool busy();

} // namespace ir_service
