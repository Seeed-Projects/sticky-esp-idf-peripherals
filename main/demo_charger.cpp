#include "demo_charger.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

static const char *TAG = "demo_charger";

void demo_charger_init()
{
    // EN_BAT_CHGn (GPIO39): output, controls BQ25616 charger enable (active low)
    // LOW = charger enabled, HIGH = charger disabled
    gpio_config_t chg_en_cfg = {};
    chg_en_cfg.pin_bit_mask = 1ULL << PIN_BAT_CHG_EN;
    chg_en_cfg.mode = GPIO_MODE_OUTPUT;
    chg_en_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    chg_en_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&chg_en_cfg);

    // CHARGE_STATE (GPIO40): input, reads BQ25616 charge status
    gpio_config_t chg_state_cfg = {};
    chg_state_cfg.pin_bit_mask = 1ULL << PIN_CHARGE_STATE;
    chg_state_cfg.mode = GPIO_MODE_INPUT;
    chg_state_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    chg_state_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&chg_state_cfg);
}

void demo_charger_run()
{
    ESP_LOGI(TAG, "=== Charger Demo Start ===");

    // Enable charger (active low: set LOW to enable)
    gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Read charge state several times
    for (int i = 0; i < 5; i++) {
        int state = gpio_get_level(static_cast<gpio_num_t>(PIN_CHARGE_STATE));
        if (i == 0) {
            ESP_LOGI(TAG, "Charge state: %s", state ? "not charging" : "charging");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Demonstrate disabling charger
    gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 1);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Read charge state after disabling
    int state = gpio_get_level(static_cast<gpio_num_t>(PIN_CHARGE_STATE));
    ESP_LOGI(TAG, "Charge state while disabled: %s", state ? "not charging" : "charging");

    // Re-enable charger for normal operation
    gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 0);
    ESP_LOGI(TAG, "Charger restored to normal operation");

    ESP_LOGI(TAG, "=== Charger Demo End ===");
}
