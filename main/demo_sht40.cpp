#include "demo_sht40.h"

#include <algorithm>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

static const char *TAG = "demo_sht40";

static uint8_t sht40_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                               : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

esp_err_t demo_sht40_run(i2c_master_bus_handle_t sensor_bus, float *temperature_c)
{
    ESP_LOGI(TAG, "=== SHT40 Demo Start ===");

    if (sensor_bus == nullptr) {
        ESP_LOGE(TAG, "Sensor I2C bus is null");
        ESP_LOGI(TAG, "=== SHT40 Demo End ===");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = SHT40_I2C_ADDR;
    dev_cfg.scl_speed_hz = 400000;

    i2c_master_dev_handle_t i2c_dev = nullptr;
    esp_err_t ret = i2c_master_bus_add_device(sensor_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK || i2c_dev == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize SHT40: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== SHT40 Demo End ===");
        return ret != ESP_OK ? ret : ESP_FAIL;
    }

    constexpr uint8_t kMeasureHighPrecision = 0xFD;
    ret = i2c_master_transmit(i2c_dev, &kMeasureHighPrecision, 1, 100);
    if (ret != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
        ret = i2c_master_transmit(i2c_dev, &kMeasureHighPrecision, 1, 100);
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t data[6] = {};
        ret = i2c_master_receive(i2c_dev, data, sizeof(data), 100);
        if (ret == ESP_OK && sht40_crc8(data, 2) == data[2] && sht40_crc8(data + 3, 2) == data[5]) {
            const uint16_t raw_temperature = (static_cast<uint16_t>(data[0]) << 8) | data[1];
            const uint16_t raw_humidity = (static_cast<uint16_t>(data[3]) << 8) | data[4];
            const float measured_temperature_c = -45.0f + 175.0f * raw_temperature / 65535.0f;
            const float humidity_percent =
                std::clamp(-6.0f + 125.0f * raw_humidity / 65535.0f, 0.0f, 100.0f);
            if (temperature_c != nullptr) {
                *temperature_c = measured_temperature_c;
            }
            ESP_LOGI(TAG, "Temperature: %.2f C", static_cast<double>(measured_temperature_c));
            ESP_LOGI(TAG, "Humidity: %.2f %%RH", static_cast<double>(humidity_percent));
        } else if (ret == ESP_OK) {
            ESP_LOGE(TAG, "SHT40 CRC check failed");
            ret = ESP_ERR_INVALID_CRC;
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT40 measurement failed: %s", esp_err_to_name(ret));
    }

    const esp_err_t operation_result = ret;
    ret = i2c_master_bus_rm_device(i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release SHT40: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "=== SHT40 Demo End ===");
    return operation_result != ESP_OK ? operation_result : ret;
}
