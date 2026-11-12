#include "demo_battery.h"

#include "bq27220.h"
#include "esp_err.h"
#include "esp_log.h"
#include "pin_config.h"

static const char *TAG = "demo_battery";
// Stock Sticky: 750 mAh battery, R46=100 mOhm, official gauge profile=7500.
// Interpret the stock profile with 10x current/capacity scaling; never reprogram
// the gauge here. DesignCapacity is a consistency check, not a calibration test.
static constexpr uint16_t kStockDesignCapacity = 7500;
static constexpr uint8_t kDesignCapacityRegister = 0x3C;
static constexpr float kStockReportScale = 10.0f;

esp_err_t demo_battery_run(i2c_master_bus_handle_t sensor_bus)
{
    ESP_LOGI(TAG, "=== Battery Demo Start ===");

    if (sensor_bus == nullptr) {
        ESP_LOGE(TAG, "Sensor I2C bus is null");
        ESP_LOGI(TAG, "=== Battery Demo End ===");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = BQ27220_I2C_ADDR;
    dev_cfg.scl_speed_hz = 400000;

    i2c_master_dev_handle_t i2c_dev = nullptr;
    esp_err_t ret = i2c_master_bus_add_device(sensor_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK || i2c_dev == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize BQ27220: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== Battery Demo End ===");
        return ret != ESP_OK ? ret : ESP_FAIL;
    }

    bool ok = bq27220_probe(i2c_dev);
    if (!ok) {
        ESP_LOGE(TAG, "BQ27220 not detected");
        ret = ESP_ERR_NOT_FOUND;
    }

    if (ok) {
        uint16_t design_capacity = 0;
        const bool design_read =
            bq27220_read_word(i2c_dev, kDesignCapacityRegister, design_capacity);
        const bool stock_scale = design_read && design_capacity == kStockDesignCapacity;
        ESP_LOGI(TAG, "Board nominal battery: 750 mAh");
        if (stock_scale) {
            ESP_LOGI(TAG, "Gauge design raw=%u; assuming stock 10x current/capacity profile",
                     design_capacity);
        } else {
            ESP_LOGW(TAG,
                     "Gauge design raw=%u read_ok=%d; profile unverified, current/capacity shown "
                     "raw only",
                     design_capacity, design_read);
            if (!design_read) {
                ret = ESP_FAIL;
            }
        }
        uint16_t voltage_mv = 0;
        int16_t current_ma = 0;
        uint16_t percent = 0;
        uint16_t capacity_mah = 0;
        float temperature_c = 0.0f;
        uint16_t cycles = 0;
        uint16_t status_bits = 0;

        if (bq27220_read_voltage_mv(i2c_dev, voltage_mv)) {
            ESP_LOGI(TAG, "Battery voltage: %u mV", voltage_mv);
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "Battery voltage read failed");
        }

        if (bq27220_read_current_ma(i2c_dev, current_ma)) {
            if (stock_scale) {
                ESP_LOGI(TAG, "Battery current (stock scale): %.1f mA (raw=%d)",
                         static_cast<double>(current_ma / kStockReportScale), current_ma);
            } else {
                ESP_LOGI(TAG, "Gauge current raw: %d", current_ma);
            }
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "Battery current read failed");
        }

        if (bq27220_read_state_of_charge(i2c_dev, percent)) {
            ESP_LOGI(TAG, "State of charge: %u %%", percent);
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "State of charge read failed");
        }

        if (bq27220_read_state_of_health(i2c_dev, percent)) {
            ESP_LOGI(TAG, "State of health: %u %%", percent);
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "State of health read failed");
        }

        if (bq27220_read_remaining_capacity_mah(i2c_dev, capacity_mah)) {
            if (stock_scale) {
                ESP_LOGI(TAG, "Remaining capacity (stock scale): %.1f mAh (raw=%u)",
                         static_cast<double>(capacity_mah / kStockReportScale), capacity_mah);
            } else {
                ESP_LOGI(TAG, "Gauge remaining capacity raw: %u", capacity_mah);
            }
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "Remaining capacity read failed");
        }

        if (bq27220_read_full_charge_capacity_mah(i2c_dev, capacity_mah)) {
            if (stock_scale) {
                ESP_LOGI(TAG, "Full charge capacity (stock scale): %.1f mAh (raw=%u)",
                         static_cast<double>(capacity_mah / kStockReportScale), capacity_mah);
            } else {
                ESP_LOGI(TAG, "Gauge full charge capacity raw: %u", capacity_mah);
            }
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "Full charge capacity read failed");
        }

        if (bq27220_read_temperature_c(i2c_dev, temperature_c)) {
            ESP_LOGI(TAG, "Gauge temperature: %.1f C", static_cast<double>(temperature_c));
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "Temperature read failed");
        }

        if (bq27220_read_cycle_count(i2c_dev, cycles)) {
            ESP_LOGI(TAG, "Gauge cycle count: %u", cycles);
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "Cycle count read failed");
        }

        if (bq27220_read_battery_status(i2c_dev, status_bits)) {
            ESP_LOGI(TAG, "Battery status raw: 0x%04X", status_bits);
        } else {
            ret = ESP_FAIL;
            ESP_LOGW(TAG, "Battery status read failed");
        }

        Bq27220Data_t data = {};
        if (!bq27220_read_data(i2c_dev, data)) {
            ESP_LOGW(TAG, "BQ27220 data struct read failed");
            ret = ESP_FAIL;
        }
    }

    const esp_err_t operation_result = ret;
    ret = i2c_master_bus_rm_device(i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release BQ27220: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "=== Battery Demo End ===");
    return operation_result != ESP_OK ? operation_result : ret;
}
