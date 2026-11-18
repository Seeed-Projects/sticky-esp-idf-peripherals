#include "demo_buttons.h"

#include <atomic>

#include "button_gpio.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "pin_config.h"

static const char *TAG = "demo_buttons";

static button_handle_t s_btn_up = nullptr;
static button_handle_t s_btn_down = nullptr;
static button_handle_t s_btn_ok = nullptr;
static bool s_ready = false;
static std::atomic<bool> s_log_events{true};

// Maps a button event enum to a readable log string.
// 将按钮事件枚举转换成适合日志显示的英文名称。
static const char *button_event_name(button_event_t event)
{
    switch (event) {
    case BUTTON_PRESS_DOWN:
        return "PRESS_DOWN";
    case BUTTON_PRESS_UP:
        return "PRESS_UP";
    case BUTTON_PRESS_REPEAT:
        return "PRESS_REPEAT";
    case BUTTON_PRESS_REPEAT_DONE:
        return "PRESS_REPEAT_DONE";
    case BUTTON_SINGLE_CLICK:
        return "SINGLE_CLICK";
    case BUTTON_DOUBLE_CLICK:
        return "DOUBLE_CLICK";
    case BUTTON_MULTIPLE_CLICK:
        return "MULTIPLE_CLICK";
    case BUTTON_LONG_PRESS_START:
        return "LONG_PRESS_START";
    case BUTTON_LONG_PRESS_HOLD:
        return "LONG_PRESS_HOLD";
    case BUTTON_LONG_PRESS_UP:
        return "LONG_PRESS_UP";
    case BUTTON_PRESS_END:
        return "PRESS_END";
    case BUTTON_NONE_PRESS:
        return "NONE_PRESS";
    default:
        return "UNKNOWN";
    }
}

static void button_event_callback(void *button_handle, void *usr_data)
{
    if (!s_log_events.load()) {
        return;
    }
    const char *label = static_cast<const char *>(usr_data);
    button_event_t event = iot_button_get_event(static_cast<button_handle_t>(button_handle));
    ESP_LOGI(TAG, "Button [%s] event: %s", label, button_event_name(event));
}

static esp_err_t register_button_event(button_handle_t handle, button_event_t event,
                                       const char *label)
{
    esp_err_t ret = iot_button_register_cb(handle, event, nullptr, button_event_callback,
                                           const_cast<char *>(label));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register %s callback for %s failed: %s", button_event_name(event), label,
                 esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t create_gpio_button(const char *label, int gpio_num, button_handle_t *handle)
{
    button_config_t btn_cfg = {};
    btn_cfg.long_press_time = 1000;
    btn_cfg.short_press_time = 180;

    button_gpio_config_t gpio_cfg = {};
    gpio_cfg.gpio_num = gpio_num;
    gpio_cfg.active_level = 0;
    gpio_cfg.enable_power_save = false;
    gpio_cfg.disable_pull = false;

    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, handle);
    if (ret != ESP_OK || *handle == nullptr) {
        ESP_LOGE(TAG, "Failed to create %s button: %s", label, esp_err_to_name(ret));
        return ret != ESP_OK ? ret : ESP_FAIL;
    }

    const button_event_t events[] = {BUTTON_PRESS_DOWN, BUTTON_PRESS_UP, BUTTON_SINGLE_CLICK,
                                     BUTTON_LONG_PRESS_START};
    for (button_event_t event : events) {
        ret = register_button_event(*handle, event, label);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t demo_buttons_wait_power_request(DemoPowerMode &mode)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    s_log_events.store(false);
    constexpr TickType_t poll = pdMS_TO_TICKS(20);
    constexpr TickType_t debounce = pdMS_TO_TICKS(60);
    constexpr TickType_t hold = pdMS_TO_TICKS(1000);
    TickType_t idle_since = xTaskGetTickCount();
    TickType_t pressed_since = idle_since;
    bool armed = false;
    unsigned previous = 0;
    while (true) {
        const TickType_t now = xTaskGetTickCount();
        const unsigned pressed =
            (gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_UP)) == 0 ? 1U : 0U) |
            (gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_OK)) == 0 ? 2U : 0U) |
            (gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_DOWN)) == 0 ? 4U : 0U);
        if (pressed != previous) {
            pressed_since = now;
            idle_since = now;
            previous = pressed;
        }
        if (pressed == 0 && now - idle_since >= debounce) {
            armed = true;
        } else if (armed && (pressed == 1 || pressed == 2) &&
                   now - pressed_since >= hold) {
            mode = pressed == 1 ? DemoPowerMode::DeepSleep : DemoPowerMode::Off;
            return ESP_OK;
        } else if (pressed != 0 && pressed != 1 && pressed != 2) {
            armed = false;
        }
        vTaskDelay(poll);
    }
}

static esp_err_t cleanup_buttons()
{
    esp_err_t result = ESP_OK;
    button_handle_t *handles[] = {&s_btn_ok, &s_btn_down, &s_btn_up};
    for (auto *handle : handles) {
        if (*handle == nullptr) {
            continue;
        }
        const esp_err_t cleanup = iot_button_delete(*handle);
        if (cleanup == ESP_OK) {
            *handle = nullptr;
        } else {
            ESP_LOGE(TAG, "Button cleanup failed: %s", esp_err_to_name(cleanup));
            if (result == ESP_OK) {
                result = cleanup;
            }
        }
    }
    return result;
}
esp_err_t demo_buttons_init()
{
    if (s_ready) {
        return ESP_OK;
    }
    esp_err_t result = cleanup_buttons();
    if (result != ESP_OK) {
        return result;
    }
    result = create_gpio_button("UP", PIN_BTN_UP, &s_btn_up);
    if (result == ESP_OK) {
        result = create_gpio_button("DOWN", PIN_BTN_DOWN, &s_btn_down);
    }
    if (result == ESP_OK) {
        result = create_gpio_button("OK/AI", PIN_BTN_OK, &s_btn_ok);
    }
    if (result != ESP_OK) {
        cleanup_buttons();
        return result;
    }
    s_ready = true;
    ESP_LOGI(TAG, "Buttons ready: UP, DOWN, OK/AI (log callbacks only, no sleep action)");
    return ESP_OK;
}
