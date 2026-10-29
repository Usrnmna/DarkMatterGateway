#pragma once

// Host declarations for the ESP-IDF 5.4.1 APIs consumed by ir_service.cpp.
// These types model API contracts, not target ABI, ISR timing, or DMA hardware.
#include <cstddef>
#include <cstdint>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_NO_MEM = 0x101, ESP_ERR_INVALID_ARG = 0x102;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103, ESP_ERR_INVALID_SIZE = 0x104;
constexpr esp_err_t ESP_ERR_NOT_FOUND = 0x105, ESP_ERR_TIMEOUT = 0x107;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
constexpr esp_err_t ESP_ERR_NVS_NO_FREE_PAGES = 0x110d;
const char *esp_err_to_name(esp_err_t error);
void host_log(const char *level, const char *tag, const char *format, ...);
#define ESP_LOGI(...) host_log("I", __VA_ARGS__)
#define ESP_LOGE(...) host_log("E", __VA_ARGS__)
#define IRAM_ATTR

using gpio_num_t = int;
constexpr gpio_num_t GPIO_NUM_4 = 4, GPIO_NUM_5 = 5, GPIO_NUM_6 = 6;
constexpr gpio_num_t GPIO_NUM_8 = 8, GPIO_NUM_9 = 9, GPIO_NUM_10 = 10;
constexpr gpio_num_t GPIO_NUM_14 = 14, GPIO_NUM_18 = 18, GPIO_NUM_21 = 21;
constexpr int GPIO_MODE_OUTPUT = 2;
struct gpio_config_t {
    std::uint64_t pin_bit_mask;
    int mode, pull_up_en, pull_down_en, intr_type;
};
esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_set_level(gpio_num_t pin, std::uint32_t level);
int gpio_get_level(gpio_num_t pin);
esp_err_t gpio_pullup_en(gpio_num_t pin);
esp_err_t gpio_reset_pin(gpio_num_t pin);
esp_err_t gpio_set_direction(gpio_num_t pin, int mode);

using BaseType_t = int;
using TickType_t = std::uint32_t;
constexpr BaseType_t pdTRUE = 1, pdFALSE = 0, pdPASS = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(ms) static_cast<TickType_t>(ms)
struct HostSemaphore;
struct HostQueue;
using SemaphoreHandle_t = HostSemaphore *;
using QueueHandle_t = HostQueue *;
using TaskFunction_t = void (*)(void *);
SemaphoreHandle_t xSemaphoreCreateMutex();
SemaphoreHandle_t xSemaphoreCreateBinary();
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t semaphore, BaseType_t *awakened);
void vSemaphoreDelete(SemaphoreHandle_t semaphore);
QueueHandle_t xQueueCreate(unsigned count, unsigned item_size);
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t timeout);
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t timeout);
void vQueueDelete(QueueHandle_t queue);
BaseType_t xTaskCreate(TaskFunction_t function, const char *name, unsigned stack,
                      void *argument, unsigned priority, void *handle);
TickType_t xTaskGetTickCount();
void vTaskDelay(TickType_t ticks);

constexpr unsigned MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_DMA = 2, MALLOC_CAP_8BIT = 4;
void *heap_caps_calloc(std::size_t count, std::size_t size, unsigned capabilities);
void *heap_caps_aligned_calloc(std::size_t alignment, std::size_t count,
                               std::size_t size, unsigned capabilities);
void heap_caps_free(void *allocation);

// Four-byte symbols retain the hardware's 15-bit duration limits without the
// SDK's anonymous union extension, so host tests can compile with -pedantic.
struct rmt_symbol_word_t {
    std::uint16_t duration0 : 15;
    std::uint16_t level0 : 1;
    std::uint16_t duration1 : 15;
    std::uint16_t level1 : 1;
};
static_assert(sizeof(rmt_symbol_word_t) == 4);
struct HostChannel;
struct HostEncoder;
using rmt_channel_handle_t = HostChannel *;
using rmt_encoder_handle_t = HostEncoder *;
constexpr int RMT_CLK_SRC_DEFAULT = 0;
struct rmt_rx_done_event_data_t {
    rmt_symbol_word_t *received_symbols;
    std::size_t num_symbols;
    struct { std::uint32_t is_last : 1; } flags;
};
using rmt_rx_done_callback_t = bool (*)(rmt_channel_handle_t,
                                       const rmt_rx_done_event_data_t *, void *);
struct rmt_rx_event_callbacks_t { rmt_rx_done_callback_t on_recv_done; };
struct rmt_rx_channel_config_t {
    gpio_num_t gpio_num;
    int clk_src;
    std::uint32_t resolution_hz;
    std::size_t mem_block_symbols;
    int intr_priority;
    struct { std::uint32_t invert_in : 1, with_dma : 1, io_loop_back : 1, allow_pd : 1; } flags;
};
struct rmt_receive_config_t {
    std::uint32_t signal_range_min_ns, signal_range_max_ns;
    struct { std::uint32_t en_partial_rx : 1; } flags;
};
struct rmt_tx_channel_config_t {
    gpio_num_t gpio_num;
    int clk_src;
    std::uint32_t resolution_hz;
    std::size_t mem_block_symbols, trans_queue_depth;
    int intr_priority;
    struct {
        std::uint32_t invert_out : 1, with_dma : 1, io_loop_back : 1, io_od_mode : 1, allow_pd : 1;
    } flags;
};
struct rmt_carrier_config_t {
    std::uint32_t frequency_hz;
    float duty_cycle;
    struct { std::uint32_t polarity_active_low : 1, always_on : 1; } flags;
};
struct rmt_copy_encoder_config_t {};
struct rmt_transmit_config_t {
    int loop_count;
    struct { std::uint32_t eot_level : 1, queue_nonblocking : 1; } flags;
};
esp_err_t rmt_new_rx_channel(const rmt_rx_channel_config_t *, rmt_channel_handle_t *);
esp_err_t rmt_rx_register_event_callbacks(rmt_channel_handle_t, const rmt_rx_event_callbacks_t *, void *);
esp_err_t rmt_enable(rmt_channel_handle_t);
esp_err_t rmt_receive(rmt_channel_handle_t, void *, std::size_t, const rmt_receive_config_t *);
esp_err_t rmt_disable(rmt_channel_handle_t);
esp_err_t rmt_del_channel(rmt_channel_handle_t);
esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t *, rmt_channel_handle_t *);
esp_err_t rmt_apply_carrier(rmt_channel_handle_t, const rmt_carrier_config_t *);
esp_err_t rmt_new_copy_encoder(const rmt_copy_encoder_config_t *, rmt_encoder_handle_t *);
esp_err_t rmt_transmit(rmt_channel_handle_t, rmt_encoder_handle_t, const void *, std::size_t,
                       const rmt_transmit_config_t *);
esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t, int);
esp_err_t rmt_del_encoder(rmt_encoder_handle_t);

using nvs_handle_t = std::uint32_t;
constexpr int NVS_READWRITE = 1, NVS_TYPE_BLOB = 0x42;
struct HostIterator;
using nvs_iterator_t = HostIterator *;
struct nvs_entry_info_t { char namespace_name[16]; char key[16]; int type; };
esp_err_t nvs_flash_init_partition(const char *partition);
esp_err_t nvs_open_from_partition(const char *, const char *, int, nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, std::size_t *);
esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, std::size_t);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_erase_key(nvs_handle_t, const char *);
esp_err_t nvs_entry_find(const char *, const char *, int, nvs_iterator_t *);
esp_err_t nvs_entry_info(nvs_iterator_t, nvs_entry_info_t *);
esp_err_t nvs_entry_next(nvs_iterator_t *);
void nvs_release_iterator(nvs_iterator_t);
// Erase APIs remain observable so a future automatic-erase regression is caught.
esp_err_t nvs_flash_erase();
esp_err_t nvs_flash_erase_partition(const char *);
esp_err_t nvs_erase_all(nvs_handle_t);
