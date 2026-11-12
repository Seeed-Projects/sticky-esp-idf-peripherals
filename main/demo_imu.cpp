#include "demo_imu.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

static const char *TAG = "demo_imu";

static esp_err_t imu_write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), 100);
}

static esp_err_t imu_read_registers(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *data,
                                    size_t length)
{
    return i2c_master_transmit_receive(device, &reg, 1, data, length, 100);
}

static int16_t read_int16_le(const uint8_t *data)
{
    return static_cast<int16_t>((static_cast<uint16_t>(data[1]) << 8) | data[0]);
}

esp_err_t demo_imu_run(i2c_master_bus_handle_t sensor_bus)
{
    ESP_LOGI(TAG, "=== IMU Demo Start ===");

    if (sensor_bus == nullptr) {
        ESP_LOGE(TAG, "Sensor I2C bus is null");
        ESP_LOGI(TAG, "=== IMU Demo End ===");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = LSM6DS3_I2C_ADDR;
    dev_cfg.scl_speed_hz = 400000;

    i2c_master_dev_handle_t i2c_dev = nullptr;
    esp_err_t ret = i2c_master_bus_add_device(sensor_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK || i2c_dev == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize LSM6DS3: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== IMU Demo End ===");
        return ret != ESP_OK ? ret : ESP_FAIL;
    }

    constexpr uint8_t kWhoAmIRegister = 0x0F;
    constexpr uint8_t kExpectedWhoAmI = 0x6A;
    uint8_t who_am_i = 0;
    ret = imu_read_registers(i2c_dev, kWhoAmIRegister, &who_am_i, 1);
    if (ret == ESP_OK && who_am_i == kExpectedWhoAmI) {
        // 104 Hz, +/-2 g accelerometer; 104 Hz, +/-250 dps gyroscope; BDU + auto-increment.
        ret = imu_write_register(i2c_dev, 0x12, 0x44);
        if (ret == ESP_OK) {
            ret = imu_write_register(i2c_dev, 0x10, 0x40);
        }
        if (ret == ESP_OK) {
            ret = imu_write_register(i2c_dev, 0x11, 0x40);
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        uint8_t data[12] = {};
        if (ret == ESP_OK) {
            ret = imu_read_registers(i2c_dev, 0x22, data, sizeof(data));
        }
        if (ret == ESP_OK) {
            const float gyro_x = read_int16_le(data + 0) * 0.00875f;
            const float gyro_y = read_int16_le(data + 2) * 0.00875f;
            const float gyro_z = read_int16_le(data + 4) * 0.00875f;
            const float accel_x = read_int16_le(data + 6) * 0.000061f;
            const float accel_y = read_int16_le(data + 8) * 0.000061f;
            const float accel_z = read_int16_le(data + 10) * 0.000061f;
            ESP_LOGI(TAG, "Accelerometer: x=%.3f g, y=%.3f g, z=%.3f g",
                     static_cast<double>(accel_x), static_cast<double>(accel_y),
                     static_cast<double>(accel_z));
            ESP_LOGI(TAG, "Gyroscope: x=%.2f dps, y=%.2f dps, z=%.2f dps",
                     static_cast<double>(gyro_x), static_cast<double>(gyro_y),
                     static_cast<double>(gyro_z));
        }
    } else if (ret == ESP_OK) {
        ESP_LOGE(TAG, "Unexpected LSM6DS3 WHO_AM_I value: 0x%02X", who_am_i);
        ret = ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LSM6DS3 test failed: %s", esp_err_to_name(ret));
    }

    if (who_am_i == kExpectedWhoAmI) {
        const esp_err_t accel_stop = imu_write_register(i2c_dev, 0x10, 0);
        const esp_err_t gyro_stop = imu_write_register(i2c_dev, 0x11, 0);
        if (ret == ESP_OK) {
            ret = accel_stop != ESP_OK ? accel_stop : gyro_stop;
        }
        if (accel_stop != ESP_OK || gyro_stop != ESP_OK) {
            ESP_LOGE(TAG, "IMU power-down failed");
        }
    }
    const esp_err_t operation_result = ret;
    ret = i2c_master_bus_rm_device(i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release LSM6DS3: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "=== IMU Demo End ===");
    return operation_result != ESP_OK ? operation_result : ret;
}
