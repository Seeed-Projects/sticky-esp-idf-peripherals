#include "demo_board.h"
#include "demo_battery.h"
#include "demo_buttons.h"
#include "demo_buzzer.h"
#include "demo_charger.h"
#include "demo_epaper.h"
#include "demo_imu.h"
#include "demo_microphone.h"
#include "demo_rtc.h"
#include "demo_sdcard.h"
#include "demo_sht40.h"
#include "demo_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char kTag[] = "peripherals";
unsigned s_ok = 0, s_failed = 0;
void report(const char *name, esp_err_t result)
{
    if (result == ESP_OK) {
        ++s_ok;
        ESP_LOGI(kTag, "%s: OK", name);
    } else {
        ++s_failed;
        ESP_LOGW(kTag, "%s: unavailable/failed (%s)", name, esp_err_to_name(result));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}
} // namespace
extern "C" void app_main()
{
    esp_log_level_set("gpio", ESP_LOG_WARN);
    esp_log_level_set("sdspi_transaction", ESP_LOG_WARN);
    ESP_LOGI(kTag, "============================================================");
    ESP_LOGI(kTag, "  reTerminal Sticky | Onboard Peripheral Examples");
    ESP_LOGI(kTag, "============================================================");
    const esp_err_t board = demo_board_init();
    if (board != ESP_OK) {
        ESP_LOGE(kTag, "Board initialization failed: %s; sequence stopped", esp_err_to_name(board));
        return;
    }
    const auto sensor_bus = demo_board_sensor_bus();
    // Get startup waveform temperature, then show a visible notice before other demos.
    float temperature_c = 0;
    const esp_err_t temperature = demo_sht40_run(sensor_bus, &temperature_c);
    report("SHT40", temperature);
    seeed_epaper_panel_handle_t panel = nullptr;
    report("Display startup",
           demo_epaper_init(&panel, temperature == ESP_OK ? &temperature_c : nullptr));
    esp_err_t result = demo_buzzer_init();
    report("Buzzer", result == ESP_OK ? demo_buzzer_run() : result);
    const esp_err_t buttons = demo_buttons_init();
    report("Buttons", buttons);
    result = demo_charger_init();
    report("Charger", result == ESP_OK ? demo_charger_run() : result);
    report("RTC", demo_rtc_run(sensor_bus));
    report("Battery", demo_battery_run(sensor_bus));
    report("IMU", demo_imu_run(sensor_bus));
    report("Microphone", demo_microphone_run());
    report("MicroSD", demo_sdcard_run());
    const esp_err_t display_ready = demo_epaper_run(panel);
    report("E-paper", display_ready);
    // A valid handle does not mean the touch page was displayed successfully.
    // Keep testing touch independently, without drawing on an unprepared display.
    report("Touch", demo_touch_run(demo_board_touch_bus(),
                                   display_ready == ESP_OK ? panel : nullptr));
    if (panel != nullptr) {
        // One recovery attempt only, including after a display/touch failure.
        const esp_err_t finished = demo_epaper_finish(panel, s_failed != 0);
        report("Finished notice", finished);
        if (finished == ESP_OK) {
            // Only a successful monochrome frame establishes a known standby state.
            report("Display standby", demo_epaper_standby(panel));
        } else {
            ESP_LOGW(kTag, "Display standby skipped: finished frame failed; panel power retained");
        }
    }
    ESP_LOGI(kTag, "Sequence finished: %u OK, %u unavailable/failed; inspect per-demo logs", s_ok,
             s_failed);
    if (buttons == ESP_OK) {
        ESP_LOGI(kTag, "UP/DOWN/OK callbacks remain active (logs only)");
    }
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
