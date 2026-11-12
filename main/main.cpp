#include <cstdio>

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
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

static const char *TAG = "main";
static i2c_master_bus_handle_t sensor_bus = nullptr;
static i2c_master_bus_handle_t touch_bus = nullptr;

static void configure_log_levels()
{
    esp_log_level_set("gpio", ESP_LOG_WARN);
    esp_log_level_set("sdspi_transaction", ESP_LOG_WARN);
    esp_log_level_set("button", ESP_LOG_WARN);
}

static void init_gpio_isr_service()
{
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "GPIO ISR service install failed: %s", esp_err_to_name(ret));
    }
}

static void init_sensor_i2c_bus()
{
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_1;
    bus_cfg.sda_io_num = static_cast<gpio_num_t>(PIN_SENSOR_SDA);
    bus_cfg.scl_io_num = static_cast<gpio_num_t>(PIN_SENSOR_SCL);
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = 1;
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &sensor_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor I2C bus initialization failed: %s", esp_err_to_name(ret));
    }
}

static void init_touch_i2c_bus()
{
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = static_cast<gpio_num_t>(PIN_TOUCH_SDA);
    bus_cfg.scl_io_num = static_cast<gpio_num_t>(PIN_TOUCH_SCL);
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = 1;
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &touch_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch I2C bus initialization failed: %s", esp_err_to_name(ret));
    }
}

static void power_on_hold()
{
    gpio_config_t io_cfg = {};
    io_cfg.pin_bit_mask = (1ULL << PIN_POWER_HOLD) | (1ULL << PIN_POWER_LOCK);
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_cfg);
    gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_HOLD), 1);
    gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_LOCK), 1);
}

extern "C" void app_main(void)
{
    configure_log_levels();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  reTerminal Sticky Peripheral Demo");
    ESP_LOGI(TAG, "========================================");

    // Step 1: Power hold
    power_on_hold();
    vTaskDelay(pdMS_TO_TICKS(100));

    init_gpio_isr_service();

    init_sensor_i2c_bus();
    init_touch_i2c_bus();
    vTaskDelay(pdMS_TO_TICKS(100));

    // Step 2: Buzzer demo
    ESP_LOGI(TAG, "");
    demo_buzzer_init();
    demo_buzzer_run();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 3: Buttons demo
    ESP_LOGI(TAG, "");
    demo_buttons_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 4: Charger demo
    ESP_LOGI(TAG, "");
    demo_charger_init();
    demo_charger_run();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 5: RTC demo
    ESP_LOGI(TAG, "");
    demo_rtc_run(sensor_bus);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 6: Battery demo
    ESP_LOGI(TAG, "");
    demo_battery_run(sensor_bus);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 7: SHT40 demo
    ESP_LOGI(TAG, "");
    demo_sht40_run(sensor_bus);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 8: IMU demo
    ESP_LOGI(TAG, "");
    demo_imu_run(sensor_bus);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 9: PDM microphone demo
    ESP_LOGI(TAG, "");
    demo_microphone_run();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 10: MicroSD demo
    ESP_LOGI(TAG, "");
    demo_sdcard_run();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 11: E-paper demo
    ESP_LOGI(TAG, "");
    seeed_epaper_panel_handle_t epaper_panel = demo_epaper_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Step 12: Touch demo
    ESP_LOGI(TAG, "");
    demo_touch_run(touch_bus, epaper_panel);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All demos completed!");
    ESP_LOGI(TAG, "  E-paper gray4 and touch drawing demos completed.");
    ESP_LOGI(TAG, "  Buttons remain active via callbacks.");
    ESP_LOGI(TAG, "  Press UP/DOWN/OK to see log output.");
    ESP_LOGI(TAG, "========================================");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
