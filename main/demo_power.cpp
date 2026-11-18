#include "demo_power.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {
constexpr char kTag[] = "demo_power";
constexpr gpio_num_t kHold = static_cast<gpio_num_t>(PIN_POWER_HOLD);
constexpr gpio_num_t kLock = static_cast<gpio_num_t>(PIN_POWER_LOCK);
constexpr gpio_num_t kExternal = static_cast<gpio_num_t>(PIN_EXTERNAL_POWER);
constexpr gpio_num_t kButtons[] = {static_cast<gpio_num_t>(PIN_BTN_OK),
                                  static_cast<gpio_num_t>(PIN_BTN_UP),
                                  static_cast<gpio_num_t>(PIN_BTN_DOWN)};
constexpr uint64_t kWakeMask = (1ULL << PIN_BTN_OK) | (1ULL << PIN_BTN_UP) | (1ULL << PIN_BTN_DOWN);
constexpr gpio_num_t kPeripheralOutputs[] = {
    static_cast<gpio_num_t>(PIN_TOUCH_RST), static_cast<gpio_num_t>(PIN_TOUCH_EN),
    static_cast<gpio_num_t>(PIN_MIC_EN), static_cast<gpio_num_t>(PIN_SD_EN),
    static_cast<gpio_num_t>(PIN_EPD_EN)};
constexpr gpio_num_t kHeldOutputs[] = {
    kHold, kLock, static_cast<gpio_num_t>(PIN_TOUCH_RST),
    static_cast<gpio_num_t>(PIN_TOUCH_EN), static_cast<gpio_num_t>(PIN_MIC_EN),
    static_cast<gpio_num_t>(PIN_SD_EN), static_cast<gpio_num_t>(PIN_EPD_EN)};
}

esp_err_t demo_power_release_holds()
{
    gpio_deep_sleep_hold_dis();
    for (gpio_num_t pin : kHeldOutputs) {
        ESP_RETURN_ON_ERROR(gpio_hold_dis(pin), kTag, "release retained output");
    }
    return ESP_OK;
}

esp_err_t demo_power_init()
{
    gpio_config_t input = {};
    input.pin_bit_mask = 1ULL << PIN_EXTERNAL_POWER;
    input.mode = GPIO_MODE_INPUT;
    return gpio_config(&input);
}

static bool buttons_released()
{
    for (gpio_num_t button : kButtons) {
        if (gpio_get_level(button) == 0) {
            return false;
        }
    }
    return true;
}

static void wait_for_button_release()
{
    ESP_LOGI(kTag, "Waiting for all keys to be released before power transition");
    TickType_t released_since = xTaskGetTickCount();
    bool released = false;
    while (true) {
        const TickType_t now = xTaskGetTickCount();
        if (!buttons_released()) {
            released = false;
        } else if (!released) {
            released_since = now;
            released = true;
        } else if (now - released_since >= pdMS_TO_TICKS(60)) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t demo_power_enter(DemoPowerMode mode)
{
    for (gpio_num_t pin : kPeripheralOutputs) {
        ESP_RETURN_ON_ERROR(gpio_set_level(pin, 0), kTag, "preload peripheral off");
        ESP_RETURN_ON_ERROR(gpio_set_direction(pin, GPIO_MODE_OUTPUT), kTag, "drive peripheral off");
    }
    wait_for_button_release();

    if (mode == DemoPowerMode::Off) {
        ESP_RETURN_ON_ERROR(gpio_hold_dis(kHold), kTag, "release HOLD GPIO");
        ESP_RETURN_ON_ERROR(gpio_hold_dis(kLock), kTag, "release LOCK GPIO");
        esp_err_t result = gpio_set_level(kHold, 0);
        if (result == ESP_OK) {
            result = gpio_set_level(kLock, 0);
        }
        if (result == ESP_OK) {
            esp_rom_delay_us(10);
            result = gpio_set_level(kLock, 1);
        }
        if (result == ESP_OK) {
            esp_rom_delay_us(10);
            result = gpio_set_level(kLock, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
        if (result == ESP_OK && gpio_get_level(kExternal) != 0) {
            ESP_LOGI(kTag, "External power is still present; restarting");
            wait_for_button_release();
            esp_restart();
        }
        gpio_set_level(kHold, 1);
        gpio_set_level(kLock, 1);
        ESP_LOGE(kTag, "Main power did not turn off");
        return result == ESP_OK ? ESP_ERR_TIMEOUT : result;
    }

    esp_err_t result = ESP_OK;
    for (gpio_num_t button : kButtons) {
        result = rtc_gpio_pullup_en(button);
        if (result != ESP_OK) {
            break;
        }
    }
    if (result == ESP_OK) {
        result = esp_sleep_enable_ext1_wakeup_io(kWakeMask, ESP_EXT1_WAKEUP_ANY_LOW);
    }
    if (result == ESP_OK) {
        for (gpio_num_t pin : kHeldOutputs) {
            result = gpio_hold_en(pin);
            if (result != ESP_OK) {
                break;
            }
        }
    }
    if (result == ESP_OK) {
        gpio_deep_sleep_hold_en();
        result = esp_deep_sleep_try_to_start();
    }
    demo_power_release_holds();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
    for (gpio_num_t button : kButtons) {
        rtc_gpio_deinit(button);
    }
    return result;
}
