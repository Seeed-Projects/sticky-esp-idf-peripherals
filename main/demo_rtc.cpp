#include "demo_rtc.h"

#include <ctime>

#include "esp_err.h"
#include "esp_log.h"
#include "pcf8563.h"
#include "pin_config.h"

static const char *TAG = "demo_rtc";

// Logs std::tm using calendar values instead of std::tm internal offsets.
// 按日历习惯输出时间，而不是直接输出 std::tm 内部偏移值。
static void log_time(const char *label, const std::tm &timeinfo)
{
    ESP_LOGI(TAG, "%s: %04d-%02d-%02d %02d:%02d:%02d",
             label,
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

void demo_rtc_run(i2c_master_bus_handle_t sensor_bus)
{
    ESP_LOGI(TAG, "=== RTC Demo Start ===");

    if (sensor_bus == nullptr) {
        ESP_LOGE(TAG, "Sensor I2C bus is null");
        ESP_LOGI(TAG, "=== RTC Demo End ===");
        return;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = PCF8563_I2C_ADDR;
    dev_cfg.scl_speed_hz = 400000;

    i2c_master_dev_handle_t i2c_dev = nullptr;
    esp_err_t ret = i2c_master_bus_add_device(sensor_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK || i2c_dev == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize PCF8563: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== RTC Demo End ===");
        return;
    }

    bool ok = pcf8563_probe(i2c_dev);
    if (!ok) ESP_LOGE(TAG, "PCF8563 not detected");

    ok = pcf8563_disable_clkout(i2c_dev);
    if (!ok) ESP_LOGE(TAG, "Failed to disable PCF8563 CLKOUT");

    std::tm current_time = {};
    ok = pcf8563_get_time(i2c_dev, current_time);
    if (ok) {
        log_time("Current RTC time", current_time);
    } else {
        ESP_LOGE(TAG, "Failed to read PCF8563 time");
    }

    ret = i2c_master_bus_rm_device(i2c_dev);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Failed to release PCF8563: %s", esp_err_to_name(ret));
    ESP_LOGI(TAG, "=== RTC Demo End ===");
}
