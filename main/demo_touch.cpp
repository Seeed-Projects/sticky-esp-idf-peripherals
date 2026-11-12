#include "demo_touch.h"

#include <algorithm>

#include "demo_epaper.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gt911.h"
#include "pin_config.h"

static const char *TAG = "demo_touch";
static seeed_epaper_panel_handle_t s_epaper_panel = nullptr;
static constexpr uint16_t kNativeWidth = 800;
static constexpr uint16_t kNativeHeight = 480;
static constexpr uint16_t kLogicalWidth = 480;
static constexpr uint16_t kLogicalHeight = 800;

static uint16_t scale_coordinate(uint16_t value, uint16_t source_max, uint16_t target_max)
{
    const uint16_t clamped = std::min(value, source_max);
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(clamped) * target_max + source_max / 2U) / source_max);
}

static void transform_touch_coordinate(uint16_t touch_x,
                                       uint16_t touch_y,
                                       uint16_t &logical_x,
                                       uint16_t &logical_y,
                                       uint16_t &framebuffer_x,
                                       uint16_t &framebuffer_y)
{
    logical_x = scale_coordinate(touch_x, kNativeWidth, kLogicalWidth - 1);
    const uint16_t mapped_y = std::min(touch_y, kNativeHeight);
    logical_y = scale_coordinate(kNativeHeight - mapped_y,
                                 kNativeHeight,
                                 kLogicalHeight - 1);

    framebuffer_x = kNativeWidth - logical_y - 1;
    framebuffer_y = logical_x;
}

void demo_touch_transform_coordinate(uint16_t touch_x,
                                     uint16_t touch_y,
                                     uint16_t &display_x,
                                     uint16_t &display_y)
{
    // Use the same verified transform as game/2048. First map the GT911 output
    // to the portrait 480x800 logical screen, then rotate that logical point
    // by 270 degrees into the native 800x480 framebuffer.
    uint16_t logical_x = 0;
    uint16_t logical_y = 0;
    transform_touch_coordinate(touch_x, touch_y, logical_x, logical_y, display_x, display_y);
}

void demo_touch_run(i2c_master_bus_handle_t touch_bus, seeed_epaper_panel_handle_t epaper_panel)
{
    s_epaper_panel = epaper_panel;
    ESP_LOGI(TAG, "=== Touch Demo Start ===");

    if (touch_bus == nullptr) {
        ESP_LOGE(TAG, "Touch I2C bus is null");
        ESP_LOGI(TAG, "=== Touch Demo End ===");
        return;
    }

    gpio_config_t touch_power_cfg = {};
    touch_power_cfg.pin_bit_mask = 1ULL << PIN_TOUCH_EN;
    touch_power_cfg.mode = GPIO_MODE_OUTPUT;
    touch_power_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    touch_power_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    touch_power_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&touch_power_cfg);
    gpio_set_level(static_cast<gpio_num_t>(PIN_TOUCH_EN), 1);
    vTaskDelay(pdMS_TO_TICKS(250));
    GT911 gt911;
    bool ok = gt911.begin(PIN_TOUCH_INT, PIN_TOUCH_RST, 800, 480, touch_bus);
    if (!ok) ESP_LOGE(TAG, "GT911 initialization failed");

    if (ok) {
        uint16_t max_x = 0;
        uint16_t max_y = 0;
        gt911.readResolution(max_x, max_y);
        ESP_LOGI(TAG, "GT911 ready: sensor=%ux%u, logical=%ux%u",
                 max_x, max_y, kLogicalWidth, kLogicalHeight);

        gt911.onTouch([](int8_t count, const GTPoint *points) {
            for (int8_t i = 0; i < count; ++i) {
                uint16_t display_x = 0;
                uint16_t display_y = 0;
                uint16_t logical_x = 0;
                uint16_t logical_y = 0;
                transform_touch_coordinate(points[i].x,
                                           points[i].y,
                                           logical_x,
                                           logical_y,
                                           display_x,
                                           display_y);

                ESP_LOGI(TAG, "Touch %d: GT911=(%d,%d) -> logical=(%u,%u) -> framebuffer=(%u,%u), size=%d",
                         i,
                         points[i].x,
                         points[i].y,
                         static_cast<unsigned>(logical_x),
                         static_cast<unsigned>(logical_y),
                         static_cast<unsigned>(display_x),
                         static_cast<unsigned>(display_y),
                         points[i].size);
                demo_epaper_draw_dot(s_epaper_panel, display_x, display_y);
            }
        });

        constexpr TickType_t kDemoDuration = pdMS_TO_TICKS(30000);
        constexpr TickType_t kPollInterval = pdMS_TO_TICKS(200);
        const TickType_t start_tick = xTaskGetTickCount();
        uint32_t last_remaining_seconds = 31;
        ESP_LOGI(TAG, "Touch to draw dots on e-paper for 30 seconds...");

        while (true) {
            const TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
            if (elapsed_ticks >= kDemoDuration) break;

            const TickType_t remaining_ticks = kDemoDuration - elapsed_ticks;
            const uint32_t remaining_seconds =
                (remaining_ticks + pdMS_TO_TICKS(1000) - 1) / pdMS_TO_TICKS(1000);
            if (remaining_seconds != last_remaining_seconds) {
                ESP_LOGI(TAG, "Touch demo: %u seconds remaining",
                         static_cast<unsigned>(remaining_seconds));
                last_remaining_seconds = remaining_seconds;
            }
            gt911.loop();
            vTaskDelay(kPollInterval);
        }
    }

    ESP_LOGI(TAG, "=== Touch Demo End ===");
}
