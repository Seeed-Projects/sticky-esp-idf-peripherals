#include "demo_charger.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"
namespace {
constexpr char kTag[] = "demo_charger";
bool s_ready = false;
} // namespace
esp_err_t demo_charger_init()
{
    if (s_ready) {
        return ESP_OK;
    }
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << PIN_BAT_CHG_EN;
    config.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&config), kTag, "configure charger enable");
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 0), kTag,
                        "enable charger");
    config = {};
    config.pin_bit_mask = 1ULL << PIN_CHARGE_STATE;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    const esp_err_t result = gpio_config(&config);
    s_ready = result == ESP_OK;
    return result;
}
esp_err_t demo_charger_run()
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 0), kTag,
                        "enable charger");
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(kTag, "Charger enabled; GPIO40 raw status=%d (not a measured charging current)",
             gpio_get_level(static_cast<gpio_num_t>(PIN_CHARGE_STATE)));
    for (int i = 0; i < 5; ++i) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    const esp_err_t disabled = gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 1);
    if (disabled == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(kTag, "Charger disabled; GPIO40 raw status=%d",
                 gpio_get_level(static_cast<gpio_num_t>(PIN_CHARGE_STATE)));
    }
    const esp_err_t restored = gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 0);
    if (restored != ESP_OK) {
        ESP_LOGE(kTag, "Restore charger enable failed: %s", esp_err_to_name(restored));
    } else {
        ESP_LOGI(kTag, "Charger enable restored (active low)");
    }
    return disabled != ESP_OK ? disabled : restored;
}
