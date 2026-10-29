#include "support.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

struct HostSemaphore { bool available, mutex; };
struct HostQueue { unsigned capacity; std::deque<void *> items; };
struct HostEncoder {};
struct HostChannel {
    bool enabled = false;
    int pin = 0;
    unsigned carrier = 0;
    float duty = 0;
    rmt_rx_done_callback_t callback = nullptr;
    void *context = nullptr;
    void *buffer = nullptr;
    std::size_t bytes = 0;
};
struct HostIterator { std::vector<std::string> keys; std::size_t index = 0; };

namespace {
std::set<HostSemaphore *> live_semaphores;
std::set<HostQueue *> live_queues;
std::set<HostChannel *> live_channels;
std::set<HostEncoder *> live_encoders;
std::set<HostIterator *> live_iterators;
std::map<void *, std::size_t> live_allocations;
struct WorkerBlocked {};

// Fail a selected invocation without changing unrelated dependency behavior.
esp_err_t result(fake::Op operation)
{
    const auto call = ++fake::state.calls[operation];
    const auto failure = fake::state.failures.find(operation);
    return failure != fake::state.failures.end() && failure->second.call == call
               ? failure->second.error : ESP_OK;
}

void check_storage(nvs_handle_t handle)
{
    assert(handle == 1 && fake::state.storage_open);
}
} // namespace

namespace fake {
State state;
void fail(Op operation, unsigned call, esp_err_t error) { state.failures[operation] = {call, error}; }
unsigned calls(Op operation) { return state.calls[operation]; }
std::size_t allocations() { return live_allocations.size(); }
std::size_t channels() { return live_channels.size(); }
std::size_t encoders() { return live_encoders.size(); }
std::size_t semaphores() { return live_semaphores.size(); }
std::size_t queues() { return live_queues.size(); }
std::size_t iterators() { return live_iterators.size(); }

void run_worker()
{
    assert(state.worker);
    try { state.worker(state.worker_argument); }
    catch (const WorkerBlocked &) { return; }
    assert(false && "The production worker must block rather than return");
}

bool logged(const std::string &fragment)
{
    return std::any_of(state.logs.begin(), state.logs.end(), [&](const auto &line) {
        return line.find(fragment) != std::string::npos;
    });
}

// Stop peripheral ownership before reclaiming resources deliberately retained
// by a faulted service. This models test teardown, not a production recovery API.
void shutdown()
{
    for (auto *queue : live_queues) { assert(queue->items.empty()); delete queue; }
    live_queues.clear();
    for (auto *channel : live_channels) delete channel;
    live_channels.clear();
    for (auto *encoder : live_encoders) delete encoder;
    live_encoders.clear();
    for (auto *semaphore : live_semaphores) delete semaphore;
    live_semaphores.clear();
    for (auto *iterator : live_iterators) delete iterator;
    live_iterators.clear();
    for (const auto &allocation : live_allocations) std::free(allocation.first);
    live_allocations.clear();
    state = {};
}
} // namespace fake

const char *esp_err_to_name(esp_err_t) { return "HOST_SERVICE_ERROR"; }
void host_log(const char *level, const char *tag, const char *format, ...)
{
    char text[512];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    fake::state.logs.push_back(std::string(level) + ":" + tag + ":" + text);
}

esp_err_t gpio_config(const gpio_config_t *) { return result(fake::Op::GpioConfig); }
esp_err_t gpio_set_level(gpio_num_t pin, std::uint32_t level)
{
    const auto error = result(fake::Op::GpioSet);
    if (error == ESP_OK) fake::state.pins[pin] = static_cast<int>(level);
    return error;
}
int gpio_get_level(gpio_num_t pin) { return fake::state.pins[pin]; }
esp_err_t gpio_pullup_en(gpio_num_t) { return result(fake::Op::Pullup); }
esp_err_t gpio_reset_pin(gpio_num_t pin) { fake::state.pins[pin] = 0; return ESP_OK; }
esp_err_t gpio_set_direction(gpio_num_t, int) { return ESP_OK; }

SemaphoreHandle_t xSemaphoreCreateMutex()
{
    if (result(fake::Op::Mutex) != ESP_OK) return nullptr;
    auto *value = new HostSemaphore{true, true};
    live_semaphores.insert(value);
    return value;
}
SemaphoreHandle_t xSemaphoreCreateBinary()
{
    if (result(fake::Op::BinarySemaphore) != ESP_OK) return nullptr;
    auto *value = new HostSemaphore{false, false};
    live_semaphores.insert(value);
    return value;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout)
{
    assert(live_semaphores.count(semaphore));
    if (semaphore->available) { semaphore->available = false; return pdTRUE; }
    assert(timeout != portMAX_DELAY && "Unexpected deadlock in deterministic schedule");
    fake::state.ticks += timeout;
    return pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    assert(live_semaphores.count(semaphore));
    assert(!semaphore->available);
    semaphore->available = true;
    return pdTRUE;
}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t semaphore, BaseType_t *awakened)
{
    *awakened = pdTRUE;
    return xSemaphoreGive(semaphore);
}
void vSemaphoreDelete(SemaphoreHandle_t semaphore)
{
    assert(live_semaphores.erase(semaphore));
    delete semaphore;
}
QueueHandle_t xQueueCreate(unsigned capacity, unsigned item_size)
{
    assert(item_size == sizeof(void *));
    if (result(fake::Op::Queue) != ESP_OK) return nullptr;
    auto *queue = new HostQueue{capacity, {}};
    live_queues.insert(queue);
    return queue;
}
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t timeout)
{
    assert(live_queues.count(queue) && timeout == 0);
    if (result(fake::Op::QueueSend) != ESP_OK || queue->items.size() >= queue->capacity) return pdFALSE;
    void *pointer;
    std::memcpy(&pointer, item, sizeof(pointer));
    queue->items.push_back(pointer);
    return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t timeout)
{
    assert(live_queues.count(queue) && timeout == portMAX_DELAY);
    // The exception escapes only at a would-block boundary outside the worker
    // critical section. It avoids modifying the production infinite task loop.
    if (queue->items.empty()) throw WorkerBlocked{};
    std::memcpy(item, &queue->items.front(), sizeof(void *));
    queue->items.pop_front();
    return pdTRUE;
}
void vQueueDelete(QueueHandle_t queue)
{
    assert(queue->items.empty() && live_queues.erase(queue));
    delete queue;
}
BaseType_t xTaskCreate(TaskFunction_t function, const char *, unsigned, void *argument, unsigned, void *)
{
    if (result(fake::Op::Task) != ESP_OK) return pdFALSE;
    fake::state.worker = function;
    fake::state.worker_argument = argument;
    return pdPASS;
}
TickType_t xTaskGetTickCount() { return fake::state.ticks; }
void vTaskDelay(TickType_t ticks)
{
    fake::state.ticks += ticks;
    fake::state.delays.push_back(ticks);
}

void *heap_caps_aligned_calloc(std::size_t alignment, std::size_t count, std::size_t size, unsigned)
{
    if (result(fake::Op::Allocate) != ESP_OK) return nullptr;
    void *memory = nullptr;
    if (posix_memalign(&memory, alignment, count * size) != 0) return nullptr;
    std::memset(memory, 0, count * size);
    live_allocations[memory] = count * size;
    return memory;
}
void *heap_caps_calloc(std::size_t count, std::size_t size, unsigned capabilities)
{
    return heap_caps_aligned_calloc(alignof(std::max_align_t), count, size, capabilities);
}
void heap_caps_free(void *memory)
{
    if (!memory) return;
    for (auto *channel : live_channels)
        assert(memory != channel->buffer && memory != channel->context && "Driver still owns this allocation");
    assert(live_allocations.erase(memory));
    std::free(memory);
}

esp_err_t rmt_new_rx_channel(const rmt_rx_channel_config_t *config, rmt_channel_handle_t *output)
{
    const auto error = result(fake::Op::NewRx);
    if (error != ESP_OK) return error;
    assert(config->resolution_hz == 2000000 && config->flags.with_dma);
    auto *channel = new HostChannel;
    channel->pin = config->gpio_num;
    live_channels.insert(channel);
    *output = channel;
    return ESP_OK;
}
esp_err_t rmt_rx_register_event_callbacks(rmt_channel_handle_t channel,
                                         const rmt_rx_event_callbacks_t *callbacks, void *context)
{
    const auto error = result(fake::Op::Callbacks);
    if (error == ESP_OK) { channel->callback = callbacks->on_recv_done; channel->context = context; }
    return error;
}
esp_err_t rmt_enable(rmt_channel_handle_t channel)
{
    const auto error = result(fake::Op::Enable);
    if (error == ESP_OK) channel->enabled = true;
    return error;
}
esp_err_t rmt_receive(rmt_channel_handle_t channel, void *buffer, std::size_t bytes,
                       const rmt_receive_config_t *config)
{
    const auto error = result(fake::Op::Receive);
    if (error != ESP_OK) return error;
    assert(channel->enabled && channel->callback && !config->flags.en_partial_rx);
    assert(fake::state.pins[18] == 1);
    fake::state.receive_pins.push_back(channel->pin);
    channel->buffer = buffer;
    fake::Capture capture;
    if (!fake::state.captures.empty()) {
        capture = fake::state.captures.front();
        fake::state.captures.pop_front();
    }
    if (fake::state.on_receive) fake::state.on_receive();
    fake::state.ticks += capture.elapsed;
    assert(capture.symbols.size() * sizeof(rmt_symbol_word_t) <= bytes);
    std::memcpy(buffer, capture.symbols.data(), capture.symbols.size() * sizeof(rmt_symbol_word_t));
    fake::state.pins[channel->pin] = capture.idle_high;
    if (capture.callback) {
        rmt_rx_done_event_data_t event{};
        event.received_symbols = static_cast<rmt_symbol_word_t *>(buffer) + (capture.wrong_buffer ? 1 : 0);
        event.num_symbols = capture.reported_count ? capture.reported_count : capture.symbols.size();
        event.flags.is_last = capture.complete;
        channel->callback(channel, &event, channel->context);
    }
    return ESP_OK;
}
esp_err_t rmt_disable(rmt_channel_handle_t channel)
{
    assert(fake::state.pins[18] == 1 && "Outputs must be inhibited before teardown");
    const auto error = result(fake::Op::Disable);
    if (error == ESP_OK) channel->enabled = false;
    return error;
}
esp_err_t rmt_del_channel(rmt_channel_handle_t channel)
{
    assert(!channel->enabled && fake::state.pins[18] == 1);
    const auto error = result(fake::Op::DeleteChannel);
    if (error == ESP_OK) { assert(live_channels.erase(channel)); delete channel; }
    return error;
}
esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t *config, rmt_channel_handle_t *output)
{
    const auto error = result(fake::Op::NewTx);
    if (error != ESP_OK) return error;
    assert(config->resolution_hz == 1000000 && config->flags.with_dma);
    auto *channel = new HostChannel;
    channel->pin = config->gpio_num;
    live_channels.insert(channel);
    *output = channel;
    return ESP_OK;
}
esp_err_t rmt_apply_carrier(rmt_channel_handle_t channel, const rmt_carrier_config_t *config)
{
    const auto error = result(fake::Op::Carrier);
    if (error == ESP_OK) { channel->carrier = config->frequency_hz; channel->duty = config->duty_cycle; }
    return error;
}
esp_err_t rmt_new_copy_encoder(const rmt_copy_encoder_config_t *, rmt_encoder_handle_t *output)
{
    const auto error = result(fake::Op::Encoder);
    if (error == ESP_OK) { *output = new HostEncoder; live_encoders.insert(*output); }
    return error;
}
esp_err_t rmt_transmit(rmt_channel_handle_t channel, rmt_encoder_handle_t encoder,
                       const void *symbols, std::size_t bytes, const rmt_transmit_config_t *config)
{
    assert(channel->enabled && live_encoders.count(encoder));
    assert(fake::state.pins[18] == 0 && config->loop_count == 0 && config->flags.eot_level == 0);
    assert(config->flags.queue_nonblocking);
    const auto error = result(fake::Op::Transmit);
    if (error == ESP_OK) { channel->buffer = const_cast<void *>(symbols); channel->bytes = bytes; }
    return error;
}
esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t channel, int timeout)
{
    assert(timeout > 0 && live_allocations.count(channel->buffer));
    if (fake::state.on_wait_tx) fake::state.on_wait_tx();
    const auto error = result(fake::Op::WaitTx);
    if (error != ESP_OK) return error;
    const auto *symbols = static_cast<rmt_symbol_word_t *>(channel->buffer);
    fake::state.transmissions.push_back({channel->pin, channel->carrier, channel->duty,
                                         {symbols, symbols + channel->bytes / sizeof(*symbols)}});
    return ESP_OK;
}
esp_err_t rmt_del_encoder(rmt_encoder_handle_t encoder)
{
    const auto error = result(fake::Op::DeleteEncoder);
    if (error == ESP_OK) { assert(live_encoders.erase(encoder)); delete encoder; }
    return error;
}

esp_err_t nvs_flash_init_partition(const char *partition)
{
    assert(std::string(partition) == "ir_store");
    return result(fake::Op::NvsInit);
}
esp_err_t nvs_open_from_partition(const char *partition, const char *name, int mode, nvs_handle_t *handle)
{
    assert(std::string(partition) == "ir_store" && std::string(name) == "ir_codes" && mode == NVS_READWRITE);
    const auto error = result(fake::Op::NvsOpen);
    if (error == ESP_OK) { *handle = 1; fake::state.storage_open = true; }
    return error;
}
void nvs_close(nvs_handle_t handle) { check_storage(handle); fake::state.storage_open = false; }
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *output, std::size_t *length)
{
    check_storage(handle);
    const auto error = result(fake::Op::NvsGet);
    if (error != ESP_OK) return error;
    const auto found = fake::state.visible.find(key);
    if (found == fake::state.visible.end()) return ESP_ERR_NVS_NOT_FOUND;
    const auto &bytes = found->second;
    if (output && *length < bytes.size()) return ESP_ERR_INVALID_SIZE;
    if (output) std::memcpy(output, bytes.data(), bytes.size());
    *length = bytes.size();
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *input, std::size_t length)
{
    check_storage(handle);
    const auto error = result(fake::Op::NvsSet);
    if (error == ESP_OK) {
        const auto *bytes = static_cast<const std::uint8_t *>(input);
        fake::state.visible[key] = {bytes, bytes + length};
    }
    return error;
}
esp_err_t nvs_commit(nvs_handle_t handle)
{
    check_storage(handle);
    const auto error = result(fake::Op::NvsCommit);
    // Both uncertain outcomes are selectable: an error is never evidence that
    // the preceding write rolled back or that it became durable.
    if (error == ESP_OK || fake::state.commit_before_error) fake::state.committed = fake::state.visible;
    if (error == ESP_OK && fake::state.after_commit) fake::state.after_commit();
    return error;
}
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    check_storage(handle);
    const auto error = result(fake::Op::NvsEraseKey);
    if (error != ESP_OK) return error;
    return fake::state.visible.erase(key) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_entry_find(const char *partition, const char *name, int type, nvs_iterator_t *iterator)
{
    assert(std::string(partition) == "ir_store" && std::string(name) == "ir_codes" && type == NVS_TYPE_BLOB);
    const auto error = result(fake::Op::NvsFind);
    if (error != ESP_OK) return error;
    if (fake::state.visible.empty()) return ESP_ERR_NVS_NOT_FOUND;
    *iterator = new HostIterator;
    for (const auto &entry : fake::state.visible) (*iterator)->keys.push_back(entry.first);
    live_iterators.insert(*iterator);
    return ESP_OK;
}
esp_err_t nvs_entry_info(nvs_iterator_t iterator, nvs_entry_info_t *info)
{
    assert(live_iterators.count(iterator));
    const auto error = result(fake::Op::NvsInfo);
    if (error == ESP_OK) {
        std::strcpy(info->namespace_name, "ir_codes");
        std::strncpy(info->key, iterator->keys[iterator->index].c_str(), sizeof(info->key) - 1);
        info->type = NVS_TYPE_BLOB;
    }
    return error;
}
esp_err_t nvs_entry_next(nvs_iterator_t *iterator)
{
    assert(live_iterators.count(*iterator));
    const auto error = result(fake::Op::NvsNext);
    if (error != ESP_OK) return error;
    if (++(*iterator)->index < (*iterator)->keys.size()) return ESP_OK;
    nvs_release_iterator(*iterator);
    *iterator = nullptr;
    return ESP_ERR_NVS_NOT_FOUND;
}
void nvs_release_iterator(nvs_iterator_t iterator)
{
    if (iterator) { assert(live_iterators.erase(iterator)); delete iterator; }
}
esp_err_t nvs_flash_erase() { result(fake::Op::EraseAll); fake::state.visible.clear(); return ESP_OK; }
esp_err_t nvs_flash_erase_partition(const char *) { return nvs_flash_erase(); }
esp_err_t nvs_erase_all(nvs_handle_t) { return nvs_flash_erase(); }
