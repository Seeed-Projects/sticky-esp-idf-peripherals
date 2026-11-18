#include "demo_touch.h"

#include <algorithm>
#include <atomic>

#include "demo_epaper.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "gt911.h"
#include "pin_config.h"

static const char *TAG = "demo_touch";
static constexpr uint16_t kNativeWidth = 800;
static constexpr uint16_t kNativeHeight = 480;
static constexpr uint16_t kLogicalWidth = 480;
static constexpr uint16_t kLogicalHeight = 800;

static uint16_t scale_coordinate(uint16_t value, uint16_t source_max, uint16_t target_max)
{
    const uint16_t clamped = std::min(value, source_max);
    return static_cast<uint16_t>((static_cast<uint32_t>(clamped) * target_max + source_max / 2U) /
                                 source_max);
}

static void transform_touch_coordinate(uint16_t touch_x, uint16_t touch_y, uint16_t &logical_x,
                                       uint16_t &logical_y, uint16_t &framebuffer_x,
                                       uint16_t &framebuffer_y)
{
    logical_x = scale_coordinate(touch_x, kNativeWidth, kLogicalWidth - 1);
    const uint16_t mapped_y = std::min(touch_y, kNativeHeight);
    logical_y = scale_coordinate(kNativeHeight - mapped_y, kNativeHeight, kLogicalHeight - 1);

    framebuffer_x = kNativeWidth - logical_y - 1;
    framebuffer_y = logical_x;
}

void demo_touch_transform_coordinate(uint16_t touch_x, uint16_t touch_y, uint16_t &display_x,
                                     uint16_t &display_y)
{
    // Preserve this demo's portrait-to-native mapping. No extra 180-degree
    // Canvas rotation is used here; check all four physical corners on hardware.
    uint16_t logical_x = 0;
    uint16_t logical_y = 0;
    transform_touch_coordinate(touch_x, touch_y, logical_x, logical_y, display_x, display_y);
}

namespace {
constexpr TickType_t kDuration = pdMS_TO_TICKS(kDemoTouchDurationSeconds * 1000);
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(20);
constexpr UBaseType_t kPointCapacity = 128;
struct Dot {
    uint16_t x, y;
};
// Static lifetime protects the task's resources even if a stop acknowledgement times out.
GT911 s_controller;
QueueHandle_t s_points = nullptr;
SemaphoreHandle_t s_stopped = nullptr;
std::atomic<bool> s_stop{false};
std::atomic<esp_err_t> s_error{ESP_OK};
bool s_active = false; // API is called by one owner task, never concurrently.
TickType_t s_start;
unsigned s_queued = 0, s_dropped = 0;

void capture_task(void *)
{
    GTPoint previous[GT911::kMaxTouchPoints] = {};
    int8_t previous_count = 0;
    uint32_t last_remaining_seconds = kDemoTouchDurationSeconds + 1;
    while (!s_stop.load() && xTaskGetTickCount() - s_start < kDuration) {
        // Count down in the capture task so blocking display refreshes cannot
        // hide progress. No extra delay is introduced for logging.
        const TickType_t elapsed = xTaskGetTickCount() - s_start;
        if (elapsed >= kDuration) {
            break;
        }
        const uint32_t remaining_seconds =
            (kDuration - elapsed + pdMS_TO_TICKS(1000) - 1) / pdMS_TO_TICKS(1000);
        if (remaining_seconds != last_remaining_seconds) {
            ESP_LOGI(TAG, "Touch countdown: %u seconds remaining",
                     static_cast<unsigned>(remaining_seconds));
            last_remaining_seconds = remaining_seconds;
        }
        GTPoint points[GT911::kMaxTouchPoints] = {};
        const int8_t count = s_controller.read_points(points, GT911::kMaxTouchPoints);
        if (count == GT911::kNoUpdate) {
            // Preserve the last report until an explicit release/new sample.
        } else if (count < 0) {
            s_error.store(ESP_FAIL);
            break;
        } else {
            for (int8_t i = 0; i < count; ++i) {
                bool unchanged = false;
                bool new_press = true;
                for (int8_t j = 0; j < previous_count; ++j) {
                    if (points[i].id == previous[j].id) {
                        new_press = false;
                        unchanged = points[i].x == previous[j].x && points[i].y == previous[j].y;
                        break;
                    }
                }
                if (unchanged) {
                    continue;
                }
                Dot dot;
                demo_touch_transform_coordinate(points[i].x, points[i].y, dot.x, dot.y);
                // One INFO line per new touch ID; motion remains at DEBUG.
                if (new_press) {
                    ESP_LOGI(TAG, "Press: raw=(%u,%u) screen=(%u,%u)", points[i].x, points[i].y,
                             dot.x, dot.y);
                }
                // No I/O or state-changing call is hidden in a logging argument.
                ESP_LOGD(TAG, "Touch raw=(%u,%u) framebuffer=(%u,%u)", points[i].x, points[i].y,
                         dot.x, dot.y);
                if (xQueueSend(s_points, &dot, 0) == pdTRUE) {
                    ++s_queued;
                } else {
                    ++s_dropped; // Bounded overload: drop newest sample, never block capture.
                }
            }
            std::copy(points, points + count, previous);
            previous_count = count;
        }
        vTaskDelay(kPollInterval);
    }
    if (!s_stop.load() && s_error.load() == ESP_OK) {
        ESP_LOGI(TAG, "Touch countdown: 0 seconds remaining");
    }
    xSemaphoreGive(s_stopped);
    vTaskDelete(nullptr); // No module resource is accessed after acknowledging stop.
}

// Bound each batch even while the producer is adding new points.
esp_err_t draw_batch(seeed_epaper_panel_handle_t panel, const Dot &first)
{
    Dot dot = first;
    for (UBaseType_t i = 0; i < kPointCapacity; ++i) {
        if (panel != nullptr) {
            const esp_err_t result = demo_epaper_add_dot(panel, dot.x, dot.y);
            if (result != ESP_OK) {
                return result;
            }
        }
        if (i + 1 == kPointCapacity || xQueueReceive(s_points, &dot, 0) != pdTRUE) {
            break;
        }
    }
    return panel != nullptr ? demo_epaper_flush(panel) : ESP_OK;
}

void release_resources()
{
    s_controller.deinit(); // Only after capture has acknowledged its final I2C access.
    vQueueDelete(s_points);
    vSemaphoreDelete(s_stopped);
    s_points = nullptr;
    s_stopped = nullptr;
    s_active = false;
}
} // namespace

esp_err_t demo_touch_run(i2c_master_bus_handle_t touch_bus,
                         seeed_epaper_panel_handle_t epaper_panel)
{
    if (touch_bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_active) {
        return ESP_ERR_INVALID_STATE;
    }
    gpio_config_t power = {};
    power.pin_bit_mask = 1ULL << PIN_TOUCH_EN;
    power.mode = GPIO_MODE_OUTPUT;
    esp_err_t result = gpio_config(&power);
    if (result == ESP_OK) {
        result = gpio_set_level(static_cast<gpio_num_t>(PIN_TOUCH_EN), 1);
    }
    if (result != ESP_OK) {
        return result;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
    if (!s_controller.begin(PIN_TOUCH_INT, PIN_TOUCH_RST, 800, 480, touch_bus)) {
        return ESP_FAIL;
    }
    s_points = xQueueCreate(kPointCapacity, sizeof(Dot));
    s_stopped = xSemaphoreCreateBinary();
    if (s_points == nullptr || s_stopped == nullptr) {
        if (s_points != nullptr) {
            vQueueDelete(s_points);
        }
        if (s_stopped != nullptr) {
            vSemaphoreDelete(s_stopped);
        }
        s_points = nullptr;
        s_stopped = nullptr;
        s_controller.deinit();
        return ESP_ERR_NO_MEM;
    }
    s_stop.store(false);
    s_error.store(ESP_OK);
    s_queued = s_dropped = 0;
    s_active = true;
    ESP_LOGI(TAG, "Touch capture: %u seconds, 20ms poll, queue=128; one coordinate line per press",
             kDemoTouchDurationSeconds);
    s_start = xTaskGetTickCount();
    if (xTaskCreate(capture_task, "touch_capture", 4096, nullptr, 4, nullptr) != pdPASS) {
        release_resources();
        return ESP_ERR_NO_MEM;
    }
    if (epaper_panel == nullptr) {
        ESP_LOGW(TAG, "Display unavailable; capture/count only");
    }
    while (xTaskGetTickCount() - s_start < kDuration && s_error.load() == ESP_OK) {
        Dot dot;
        if (xQueueReceive(s_points, &dot, kPollInterval) == pdTRUE) {
            result = draw_batch(epaper_panel, dot);
            if (result != ESP_OK) {
                break;
            }
        }
    }
    s_stop.store(true);
    if (xSemaphoreTake(s_stopped, pdMS_TO_TICKS(500)) != pdTRUE) {
        // Keep static controller/queue/semaphore alive; do not delete a task mid-I2C.
        ESP_LOGE(TAG, "Touch stop timed out; restart before retrying touch");
        return ESP_ERR_TIMEOUT;
    }
    if (result == ESP_OK) {
        result = s_error.load();
    }
    Dot dot;
    if (result == ESP_OK && xQueueReceive(s_points, &dot, 0) == pdTRUE) {
        result = draw_batch(epaper_panel, dot); // Producer is stopped: at most one final batch.
    }
    ESP_LOGI(TAG, "Touch finished: queued=%u dropped=%u result=%s", s_queued, s_dropped,
             esp_err_to_name(result));
    if (s_dropped != 0) {
        ESP_LOGW(TAG, "Touch queue saturated; some samples were dropped");
    }
    release_resources();
    return result;
}
