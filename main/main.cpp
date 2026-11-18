#include "demo_board.h"
#include "demo_battery.h"
#include "demo_buttons.h"
#include "demo_buzzer.h"
#include "demo_charger.h"
#include "demo_epaper.h"
#include "demo_imu.h"
#include "demo_microphone.h"
#include "demo_power.h"
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
    const esp_err_t buzzer = result == ESP_OK ? demo_buzzer_run() : result;
    report("Buzzer", buzzer);
    result = demo_charger_init();
    report("Charger", result == ESP_OK ? demo_charger_run() : result);
    report("RTC", demo_rtc_run(sensor_bus));
    report("Battery", demo_battery_run(sensor_bus));
    const esp_err_t imu = demo_imu_run(sensor_bus);
    report("IMU", imu);
    const esp_err_t microphone = demo_microphone_run();
    report("Microphone", microphone);
    const esp_err_t sd = demo_sdcard_run();
    report("MicroSD", sd);
    const esp_err_t display_ready = demo_epaper_run(panel);
    report("E-paper", display_ready);
    // A valid handle does not mean the touch page was displayed successfully.
    // Keep testing touch independently, without drawing on an unprepared display.
    const esp_err_t touch = demo_touch_run(demo_board_touch_bus(),
                                          display_ready == ESP_OK ? panel : nullptr);
    report("Touch", touch);
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
    const esp_err_t buttons = demo_buttons_init();
    report("Buttons", buttons);
    if (buttons == ESP_OK) {
        if (panel != nullptr) {
            report("Button notice", demo_epaper_power_notice(
                panel, "Button test", "UP / DOWN / AI", "10 seconds - see serial log"));
        }
        ESP_LOGI(kTag, "Button test for 10 seconds; no power actions");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    ESP_LOGI(kTag, "Sequence finished: %u OK, %u unavailable/failed; inspect per-demo logs", s_ok,
             s_failed);

    if (buttons != ESP_OK || touch != ESP_OK || microphone != ESP_OK || imu != ESP_OK ||
        buzzer != ESP_OK || !demo_sdcard_can_power_down() ||
        (sd != ESP_OK && sd != ESP_ERR_NOT_FOUND) || panel == nullptr) {
        ESP_LOGW(kTag, "Power tests unavailable: required peripheral checks did not pass");
        return;
    }
    result = demo_power_init();
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Power input setup failed: %s", esp_err_to_name(result));
        return;
    }
    result = demo_epaper_power_notice(
        panel, "Power test", "Hold UP 1s: Deep sleep", "Hold AI 1s: Power off");
    if (result == ESP_OK) {
        result = demo_epaper_standby(panel);
    }
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Power test notice failed: %s", esp_err_to_name(result));
        return;
    }
    while (true) {
        DemoPowerMode mode = DemoPowerMode::DeepSleep;
        result = demo_buttons_wait_power_request(mode);
        if (result != ESP_OK) {
            ESP_LOGW(kTag, "Power request cancelled: %s", esp_err_to_name(result));
            continue;
        }
        const bool off = mode == DemoPowerMode::Off;
        ESP_LOGI(kTag, "%s requested; preparing display", off ? "Power off" : "Deep sleep");
        result = demo_epaper_power_notice(
            panel, off ? "Power off" : "Deep sleep",
            off ? "Press AI to start" : "Press any key to wake", "");
        if (result == ESP_OK) {
            result = demo_epaper_sleep(panel);
        }
        if (result == ESP_OK) {
            ESP_LOGI(kTag, "Entering %s", off ? "board power off" : "ESP32 deep sleep");
            result = demo_power_enter(mode);
        }
        ESP_LOGE(kTag, "Power transition did not complete: %s; reset to retry",
                 esp_err_to_name(result));
        return;
    }
}
