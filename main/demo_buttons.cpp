#include "demo_buttons.h"

#include "button_gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "iot_button.h"
#include "pin_config.h"

static const char *TAG = "demo_buttons";

static button_handle_t s_btn_up = nullptr;
static button_handle_t s_btn_down = nullptr;
static button_handle_t s_btn_ok = nullptr;

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
    const char *label = static_cast<const char *>(usr_data);
    button_event_t event = iot_button_get_event(static_cast<button_handle_t>(button_handle));
    ESP_LOGI(TAG, "Button [%s] event: %s", label, button_event_name(event));
}

static void register_button_event(button_handle_t handle, button_event_t event, const char *label)
{
    esp_err_t ret = iot_button_register_cb(handle, event, nullptr, button_event_callback, const_cast<char *>(label));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register %s callback for %s failed: %s",
                 button_event_name(event), label, esp_err_to_name(ret));
    }
}

static void create_gpio_button(const char *label, int gpio_num, button_handle_t *handle)
{
    button_config_t btn_cfg = {};
    btn_cfg.long_press_time = 2000;
    btn_cfg.short_press_time = 180;

    button_gpio_config_t gpio_cfg = {};
    gpio_cfg.gpio_num = gpio_num;
    gpio_cfg.active_level = 0;
    gpio_cfg.enable_power_save = false;
    gpio_cfg.disable_pull = false;

    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, handle);
    if (ret != ESP_OK || *handle == nullptr) {
        ESP_LOGE(TAG, "Failed to create %s button: %s", label, esp_err_to_name(ret));
        return;
    }

    register_button_event(*handle, BUTTON_PRESS_DOWN, label);
    register_button_event(*handle, BUTTON_PRESS_UP, label);
    register_button_event(*handle, BUTTON_SINGLE_CLICK, label);
    register_button_event(*handle, BUTTON_LONG_PRESS_START, label);
}

void demo_buttons_init()
{
    ESP_LOGI(TAG, "=== Buttons Demo Start ===");

    create_gpio_button("UP", PIN_BTN_UP, &s_btn_up);
    create_gpio_button("DOWN", PIN_BTN_DOWN, &s_btn_down);
    create_gpio_button("OK", PIN_BTN_OK, &s_btn_ok);

    ESP_LOGI(TAG, "Buttons ready: UP, DOWN, OK (events remain active)");
    ESP_LOGI(TAG, "=== Buttons Demo Ready ===");
}
