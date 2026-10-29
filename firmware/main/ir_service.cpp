#include "ir_service.hpp"
#include "board_config.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace ir_service {
namespace {
constexpr char kTag[] = "ir_service";
constexpr char kNamespace[] = "ir_codes";
constexpr char kStoragePartition[] = "ir_store";
constexpr unsigned kQueueDepth = 4;
constexpr unsigned kMaxRepeats = 5;
constexpr unsigned kMinimumGapMs = 20;
constexpr unsigned kMaximumGapMs = 500;
constexpr int kTransmitTimeoutMs = static_cast<int>(ir::kMaxFrameUs / 1000) + 100;
constexpr std::size_t kMaximumStoredBytes = 32 + ir::kMaxDurations * sizeof(std::uint32_t);

SemaphoreHandle_t service_mutex = nullptr;
QueueHandle_t send_queue = nullptr;
nvs_handle_t storage = 0;
std::atomic<unsigned> outstanding_jobs{0};
bool initialized = false;
bool hardware_fault = false;
ir::Waveform pending;
bool pending_valid = false;

struct SendJob {
    char name[16]{};
    ir::Waveform waveform;
    unsigned zone = 0;
    unsigned repeats = 1;
    unsigned gap_ms = 40;
};

// Immediate acquisition keeps Matter callbacks responsive during a long capture.
class TryLock {
public:
    TryLock() : acquired_(initialized && xSemaphoreTake(service_mutex, 0) == pdTRUE) {}
    ~TryLock() { if (acquired_) xSemaphoreGive(service_mutex); }
    explicit operator bool() const { return acquired_; }
    TryLock(const TryLock &) = delete;
    TryLock &operator=(const TryLock &) = delete;
private:
    bool acquired_;
};

// Disabling the external buffers precedes every driver teardown, including errors.
void inhibit_outputs()
{
    gpio_set_level(board::kTransmitDisable, 1);
}

// A failed teardown retains driver-visible storage and latches a service fault.
// This prevents a DMA use-after-free; a restart is required after this rare fault.
bool close_channel(rmt_channel_handle_t channel, bool enabled)
{
    if (!channel) return true;
    esp_err_t error = enabled ? rmt_disable(channel) : ESP_OK;
    if (error == ESP_OK) error = rmt_del_channel(channel);
    if (error != ESP_OK) {
        hardware_fault = true;
        inhibit_outputs();
        ESP_LOGE(kTag, "RMT cleanup failed: %s; service disabled until restart", esp_err_to_name(error));
        return false;
    }
    return true;
}

// Storage is read under the service mutex. Corrupt or oversized records are
// rejected without deleting any namespace or automatically resetting NVS.
esp_err_t load_locked(const char *name, ir::Waveform &waveform)
{
    if (!name || !ir::valid_name(name)) return ESP_ERR_INVALID_ARG;
    std::size_t length = 0;
    esp_err_t error = nvs_get_blob(storage, name, nullptr, &length);
    if (error != ESP_OK) return error;
    if (length == 0 || length > kMaximumStoredBytes) return ESP_ERR_INVALID_SIZE;
    std::vector<std::uint8_t> bytes(length);
    error = nvs_get_blob(storage, name, bytes.data(), &length);
    if (error != ESP_OK) return error;
    std::string reason;
    if (!ir::deserialize(bytes, waveform, reason)) {
        ESP_LOGE(kTag, "Rejected stored command %s: %s", name, reason.c_str());
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

// Names are restricted to the core's safe identifier alphabet before printing.
// Each record is a JSON object suitable for recording alongside appliance notes.
void print_waveform(const char *name, const ir::Waveform &waveform)
{
    std::printf("{\"name\":\"%s\",\"carrier_hz\":%lu,\"duty_percent\":%u,\"durations_us\":[",
                name, static_cast<unsigned long>(waveform.carrier_hz),
                static_cast<unsigned>(waveform.duty_percent));
    for (std::size_t i = 0; i < waveform.durations_us.size(); ++i) {
        std::printf("%s%lu", i ? "," : "", static_cast<unsigned long>(waveform.durations_us[i]));
    }
    std::printf("]}\n");
}

struct CaptureContext {
    SemaphoreHandle_t completed;
    rmt_symbol_word_t *received;
    std::size_t count;
    bool last;
    gpio_num_t input;
    bool idle_high;
};

// The ISR only publishes the completed DMA span and wakes the console task.
static bool IRAM_ATTR capture_complete(rmt_channel_handle_t,
                                      const rmt_rx_done_event_data_t *event, void *argument)
{
    auto *context = static_cast<CaptureContext *>(argument);
    context->received = event->received_symbols;
    context->count = event->num_symbols;
    context->last = event->flags.is_last;
    context->idle_high = gpio_get_level(context->input) != 0;
    BaseType_t awakened = pdFALSE;
    xSemaphoreGiveFromISR(context->completed, &awakened);
    return awakened == pdTRUE;
}

// Single-frame capture requires a DMA EOF at the configured idle interval.
// Full buffers are rejected even if their end happens to coincide with EOF.
esp_err_t capture_locked(Receiver receiver, ir::Waveform &waveform)
{
    gpio_num_t input;
    std::uint32_t known_carrier = 0;
    switch (receiver) {
    case Receiver::Carrier: input = board::kLearnCarrier; break;
    case Receiver::Envelope38: input = board::kReceive38; known_carrier = 38000; break;
    case Receiver::Envelope56: input = board::kReceive56; known_carrier = 56000; break;
    default: return ESP_ERR_INVALID_ARG;
    }

    constexpr std::size_t bytes = board::kCaptureSymbols * sizeof(rmt_symbol_word_t);
    auto *buffer = static_cast<rmt_symbol_word_t *>(heap_caps_aligned_calloc(
        32, 1, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    auto *context = static_cast<CaptureContext *>(heap_caps_calloc(
        1, sizeof(CaptureContext), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!buffer || !context) {
        heap_caps_free(buffer);
        heap_caps_free(context);
        return ESP_ERR_NO_MEM;
    }
    context->completed = xSemaphoreCreateBinary();
    context->input = input;
    if (!context->completed) {
        heap_caps_free(buffer);
        heap_caps_free(context);
        return ESP_ERR_NO_MEM;
    }

    rmt_channel_handle_t channel = nullptr;
    bool enabled = false;
    rmt_rx_channel_config_t channel_config{};
    channel_config.gpio_num = input;
    channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
    channel_config.resolution_hz = board::kCaptureResolutionHz;
    channel_config.mem_block_symbols = board::kCaptureSymbols;
    channel_config.flags.with_dma = true;
    esp_err_t error = rmt_new_rx_channel(&channel_config, &channel);
    if (error == ESP_OK) error = gpio_pullup_en(input);
    rmt_rx_event_callbacks_t callbacks{};
    callbacks.on_recv_done = capture_complete;
    if (error == ESP_OK) error = rmt_rx_register_event_callbacks(channel, &callbacks, context);
    if (error == ESP_OK) {
        error = rmt_enable(channel);
        enabled = error == ESP_OK;
    }
    rmt_receive_config_t receive_config{};
    receive_config.signal_range_min_ns = 1000;
    receive_config.signal_range_max_ns = board::kCaptureIdleUs * 1000;
    const TickType_t started = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(board::kLearnTimeoutMs);
    while (error == ESP_OK) {
        const TickType_t elapsed = xTaskGetTickCount() - started;
        if (elapsed >= timeout) { error = ESP_ERR_TIMEOUT; break; }
        error = rmt_receive(channel, buffer, bytes, &receive_config);
        if (error != ESP_OK) break;
        if (xSemaphoreTake(context->completed, timeout - elapsed) != pdTRUE) {
            error = ESP_ERR_TIMEOUT;
            break;
        }
        if (!context->last || context->received != buffer || context->count >= board::kCaptureSymbols) break;
        bool active_signal = false;
        for (std::size_t i = 0; i < context->count; ++i) {
            const auto &symbol = buffer[i];
            if (!symbol.duration0) break;
            if (!symbol.level0) active_signal = true;
            if (!symbol.duration1) break;
            if (!symbol.level1) active_signal = true;
        }
        if (active_signal) break;
        // Idle-only completions are rearmed within the same overall deadline.
        // A signal-bearing frame is accepted or rejected once, never concatenated.
        std::memset(buffer, 0, bytes);
    }

    // Disable/delete the channel before decoding the final buffer or freeing
    // callback state. A timeout can occur while the DMA engine is still active.
    if (!close_channel(channel, enabled)) return ESP_FAIL;

    if (error == ESP_OK && (!context->last || !context->idle_high || context->received != buffer ||
                           context->count == 0 || context->count >= board::kCaptureSymbols)) {
        error = ESP_ERR_INVALID_SIZE;
    }
    if (error == ESP_OK) {
        std::vector<ir::Sample> samples;
        samples.reserve(context->count * 2);
        bool terminated = false;
        for (std::size_t i = 0; i < context->count && !terminated; ++i) {
            const auto &symbol = buffer[i];
            if (symbol.duration0 == 0) { terminated = true; break; }
            samples.push_back({symbol.level0 != 0, symbol.duration0});
            if (symbol.duration1 == 0) { terminated = true; break; }
            samples.push_back({symbol.level1 != 0, symbol.duration1});
        }
        std::string reason;
        // ESP-IDF 5.4.1 emits is_last for DMA only after normal_eof from an
        // RMT end marker. The API does not promise that the marker's zero half
        // is included in num_symbols, so its presence is not a second requirement.
        if (!ir::decode_capture(samples, board::kCaptureResolutionHz,
                               receiver == Receiver::Carrier, known_carrier, waveform, reason)) {
            ESP_LOGE(kTag, "Capture rejected: %s", reason.c_str());
            error = ESP_ERR_INVALID_ARG;
        }
    }
    vSemaphoreDelete(context->completed);
    heap_caps_free(context);
    heap_caps_free(buffer);
    return error;
}

// Split long same-level spans across 15-bit halves. No intermediate zero is
// emitted because a zero duration terminates an RMT transaction.
rmt_symbol_word_t *pack_symbols(const ir::Waveform &waveform, std::size_t &symbol_count)
{
    std::size_t halves = 0;
    for (const auto duration : waveform.durations_us) halves += (duration + 32766) / 32767;
    symbol_count = (halves + 1) / 2;
    auto *symbols = static_cast<rmt_symbol_word_t *>(heap_caps_calloc(
        symbol_count, sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!symbols) return nullptr;
    std::size_t position = 0;
    const auto append = [&](std::uint32_t ticks, bool high) {
        auto &symbol = symbols[position / 2];
        if ((position & 1) == 0) {
            symbol.duration0 = ticks;
            symbol.level0 = high;
        } else {
            symbol.duration1 = ticks;
            symbol.level1 = high;
        }
        ++position;
    };
    for (std::size_t i = 0; i < waveform.durations_us.size(); ++i) {
        auto remaining = waveform.durations_us[i];
        while (remaining) {
            const auto chunk = std::min<std::uint32_t>(remaining, 32767);
            append(chunk, (i & 1) == 0);
            remaining -= chunk;
        }
    }
    if (position & 1) append(1, false);
    return symbols;
}

// Only the worker calls this function, while holding the hardware/storage lock.
// Repetition is explicit and bounded; failed transmissions are never retried.
esp_err_t transmit_locked(const SendJob &job)
{
    if (hardware_fault) return ESP_ERR_INVALID_STATE;
    inhibit_outputs();
    std::size_t count = 0;
    auto *symbols = pack_symbols(job.waveform, count);
    if (!symbols) return ESP_ERR_NO_MEM;
    rmt_channel_handle_t channel = nullptr;
    rmt_encoder_handle_t encoder = nullptr;
    bool enabled = false;
    rmt_tx_channel_config_t config{};
    config.gpio_num = board::kTransmitPins[job.zone];
    config.clk_src = RMT_CLK_SRC_DEFAULT;
    config.resolution_hz = 1000000;
    config.mem_block_symbols = 256;
    config.trans_queue_depth = 1;
    config.flags.with_dma = true;
    esp_err_t error = rmt_new_tx_channel(&config, &channel);
    rmt_carrier_config_t carrier{};
    carrier.frequency_hz = job.waveform.carrier_hz;
    carrier.duty_cycle = job.waveform.duty_percent / 100.0f;
    if (error == ESP_OK) error = rmt_apply_carrier(channel, &carrier);
    rmt_copy_encoder_config_t encoder_config{};
    if (error == ESP_OK) error = rmt_new_copy_encoder(&encoder_config, &encoder);
    if (error == ESP_OK) {
        error = rmt_enable(channel);
        enabled = error == ESP_OK;
    }
    rmt_transmit_config_t transmit_config{};
    transmit_config.flags.eot_level = 0;
    transmit_config.flags.queue_nonblocking = true;
    for (unsigned repetition = 0; error == ESP_OK && repetition < job.repeats; ++repetition) {
        error = gpio_set_level(board::kTransmitDisable, 0);
        if (error == ESP_OK) error = rmt_transmit(channel, encoder, symbols,
                                                 count * sizeof(*symbols), &transmit_config);
        if (error == ESP_OK) error = rmt_tx_wait_all_done(channel, kTransmitTimeoutMs);
        inhibit_outputs();
        if (error == ESP_OK && repetition + 1 < job.repeats) {
            vTaskDelay(pdMS_TO_TICKS(job.gap_ms));
        }
    }
    inhibit_outputs();
    if (!close_channel(channel, enabled)) return ESP_FAIL;
    if (encoder) rmt_del_encoder(encoder);
    heap_caps_free(symbols);
    gpio_reset_pin(board::kTransmitPins[job.zone]);
    gpio_set_level(board::kTransmitPins[job.zone], 0);
    gpio_set_direction(board::kTransmitPins[job.zone], GPIO_MODE_OUTPUT);
    return error;
}

// A successfully queued pointer belongs to this worker until transmission ends.
// No queue or replay state is persisted, so a reboot cannot replay a command.
void send_worker(void *)
{
    for (;;) {
        SendJob *job = nullptr;
        if (xQueueReceive(send_queue, &job, portMAX_DELAY) != pdTRUE) continue;
        xSemaphoreTake(service_mutex, portMAX_DELAY);
        const esp_err_t error = transmit_locked(*job);
        if (error == ESP_OK) {
            ESP_LOGI(kTag, "Transmitted %s locally, zone %u, count %u; appliance response unconfirmed",
                     job->name, job->zone, job->repeats);
        } else {
            ESP_LOGE(kTag, "Transmission %s failed: %s; no automatic retry", job->name, esp_err_to_name(error));
        }
        delete job;
        --outstanding_jobs;
        xSemaphoreGive(service_mutex);
    }
}

bool valid_send_options(unsigned zone, unsigned repeats, unsigned gap_ms)
{
    return zone < sizeof(board::kTransmitPins) / sizeof(board::kTransmitPins[0]) &&
           repeats >= 1 && repeats <= kMaxRepeats &&
           gap_ms >= kMinimumGapMs && gap_ms <= kMaximumGapMs;
}

// The caller owns the mutex and supplies a private waveform copy. Ownership
// passes to the worker only after a successful nonblocking queue insertion.
esp_err_t enqueue_locked(const char *name, ir::Waveform &&waveform,
                         unsigned zone, unsigned repeats, unsigned gap_ms)
{
    std::string reason;
    if (!ir::validate(waveform, reason)) return ESP_ERR_INVALID_ARG;
    auto *job = new (std::nothrow) SendJob;
    if (!job) return ESP_ERR_NO_MEM;
    job->waveform = std::move(waveform);
    std::strncpy(job->name, name, sizeof(job->name) - 1);
    job->zone = zone;
    job->repeats = repeats;
    job->gap_ms = gap_ms;
    ++outstanding_jobs;
    if (xQueueSend(send_queue, &job, 0) != pdTRUE) {
        --outstanding_jobs;
        delete job;
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(kTag, "Queued %s for zone %u", name, zone);
    return ESP_OK;
}
} // namespace

esp_err_t init()
{
    if (initialized) return ESP_ERR_INVALID_STATE;
    gpio_set_level(board::kTransmitDisable, 1);
    gpio_config_t output_config{};
    output_config.pin_bit_mask = 1ULL << board::kTransmitDisable;
    output_config.mode = GPIO_MODE_OUTPUT;
    esp_err_t error = gpio_config(&output_config);
    if (error != ESP_OK) return error;
    inhibit_outputs();
    output_config.pin_bit_mask = 0;
    for (const auto pin : board::kTransmitPins) {
        gpio_set_level(pin, 0);
        output_config.pin_bit_mask |= 1ULL << pin;
    }
    error = gpio_config(&output_config);
    if (error != ESP_OK) return error;
    service_mutex = xSemaphoreCreateMutex();
    send_queue = xQueueCreate(kQueueDepth, sizeof(SendJob *));
    if (!service_mutex || !send_queue) error = ESP_ERR_NO_MEM;
    // Learned commands are isolated from the Matter commissioning partition.
    // Initialization failures must remain visible; automatic erase is prohibited.
    if (error == ESP_OK) error = nvs_flash_init_partition(kStoragePartition);
    if (error == ESP_OK) error = nvs_open_from_partition(kStoragePartition, kNamespace, NVS_READWRITE, &storage);
    if (error == ESP_OK && xTaskCreate(send_worker, "ir_tx", 6144, nullptr, 5, nullptr) != pdPASS) {
        error = ESP_ERR_NO_MEM;
    }
    if (error != ESP_OK) {
        if (storage) nvs_close(storage);
        storage = 0;
        if (send_queue) vQueueDelete(send_queue);
        if (service_mutex) vSemaphoreDelete(service_mutex);
        send_queue = nullptr;
        service_mutex = nullptr;
        return error;
    }
    initialized = true;
    return ESP_OK;
}

esp_err_t learn(Receiver receiver)
{
    TryLock lock;
    if (!lock || hardware_fault || outstanding_jobs != 0) return ESP_ERR_INVALID_STATE;
    inhibit_outputs();
    pending_valid = false;
    pending = {};
    ESP_LOGI(kTag, "Learning one frame for up to %lu ms; press the remote once",
             static_cast<unsigned long>(board::kLearnTimeoutMs));
    const esp_err_t error = capture_locked(receiver, pending);
    pending_valid = error == ESP_OK;
    if (pending_valid) {
        ESP_LOGI(kTag, "Captured into RAM only; inspect and explicitly save a command name");
        print_waveform("pending", pending);
    }
    inhibit_outputs();
    return error;
}

esp_err_t save_pending(const char *name, bool overwrite)
{
    if (!name || !ir::valid_name(name)) return ESP_ERR_INVALID_ARG;
    TryLock lock;
    if (!lock || !pending_valid) return ESP_ERR_INVALID_STATE;
    std::size_t existing_length = 0;
    esp_err_t error = nvs_get_blob(storage, name, nullptr, &existing_length);
    if (error == ESP_OK && !overwrite) return ESP_ERR_INVALID_STATE;
    if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) return error;
    const auto bytes = ir::serialize(pending);
    if (bytes.empty()) return ESP_ERR_INVALID_ARG;
    error = nvs_set_blob(storage, name, bytes.data(), bytes.size());
    if (error == ESP_OK) error = nvs_commit(storage);
    if (error == ESP_OK) {
        // Read-back checks the stored version/CRC and exact waveform contents.
        // Physical power-loss recovery remains a separate hardware test.
        ir::Waveform verified;
        error = load_locked(name, verified);
        if (error == ESP_OK && (verified.carrier_hz != pending.carrier_hz ||
                               verified.duty_percent != pending.duty_percent ||
                               verified.durations_us != pending.durations_us)) {
            error = ESP_FAIL;
        }
    }
    if (error == ESP_OK) {
        ESP_LOGI(kTag, "Saved %s to %s; commit and waveform read-back succeeded",
                 name, kStoragePartition);
    }
    return error;
}

esp_err_t load(const char *name, ir::Waveform &waveform)
{
    TryLock lock;
    return lock ? load_locked(name, waveform) : ESP_ERR_INVALID_STATE;
}

esp_err_t remove(const char *name)
{
    if (!name || !ir::valid_name(name)) return ESP_ERR_INVALID_ARG;
    TryLock lock;
    if (!lock) return ESP_ERR_INVALID_STATE;
    esp_err_t error = nvs_erase_key(storage, name);
    if (error == ESP_OK) error = nvs_commit(storage);
    return error;
}

esp_err_t list()
{
    TryLock lock;
    if (!lock) return ESP_ERR_INVALID_STATE;
    nvs_iterator_t iterator = nullptr;
    esp_err_t error = nvs_entry_find(kStoragePartition, kNamespace, NVS_TYPE_BLOB, &iterator);
    unsigned count = 0;
    while (error == ESP_OK) {
        nvs_entry_info_t info{};
        error = nvs_entry_info(iterator, &info);
        if (error != ESP_OK) break;
        std::printf("%s\n", info.key);
        ++count;
        error = nvs_entry_next(&iterator);
    }
    nvs_release_iterator(iterator);
    if (error != ESP_ERR_NVS_NOT_FOUND && error != ESP_OK) return error;
    std::printf("%u stored command(s)\n", count);
    return ESP_OK;
}

esp_err_t print_pending()
{
    TryLock lock;
    if (!lock || !pending_valid) return ESP_ERR_INVALID_STATE;
    print_waveform("pending", pending);
    return ESP_OK;
}

esp_err_t print_saved(const char *name)
{
    TryLock lock;
    if (!lock) return ESP_ERR_INVALID_STATE;
    ir::Waveform waveform;
    const esp_err_t error = load_locked(name, waveform);
    if (error == ESP_OK) print_waveform(name, waveform);
    return error;
}

esp_err_t send(const char *name, unsigned zone, unsigned repeats, unsigned gap_ms)
{
    if (!name || !ir::valid_name(name) || !valid_send_options(zone, repeats, gap_ms)) {
        return ESP_ERR_INVALID_ARG;
    }
    TryLock lock;
    if (!lock || hardware_fault || outstanding_jobs >= kQueueDepth) return ESP_ERR_INVALID_STATE;
    ir::Waveform waveform;
    const esp_err_t error = load_locked(name, waveform);
    if (error != ESP_OK) return error;
    return enqueue_locked(name, std::move(waveform), zone, repeats, gap_ms);
}

esp_err_t send_pending(unsigned zone, unsigned repeats, unsigned gap_ms)
{
    if (!valid_send_options(zone, repeats, gap_ms)) return ESP_ERR_INVALID_ARG;
    TryLock lock;
    if (!lock || hardware_fault || !pending_valid || outstanding_jobs >= kQueueDepth) return ESP_ERR_INVALID_STATE;
    return enqueue_locked("pending", ir::Waveform(pending), zone, repeats, gap_ms);
}

bool busy()
{
    TryLock lock;
    return !lock || hardware_fault || outstanding_jobs != 0;
}

} // namespace ir_service
