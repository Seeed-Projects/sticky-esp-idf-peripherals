#include "demo_buzzer.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"
namespace {
constexpr char kTag[] = "demo_buzzer";
bool s_ready = false;
} // namespace
esp_err_t demo_buzzer_init()
{
    if (s_ready) {
        return ESP_OK;
    }
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.timer_num = LEDC_TIMER_0;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.freq_hz = 4000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), kTag, "configure timer");
    ledc_channel_config_t channel = {};
    channel.gpio_num = PIN_BUZZER;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_0;
    channel.timer_sel = LEDC_TIMER_0;
    const esp_err_t result = ledc_channel_config(&channel);
    s_ready = result == ESP_OK;
    return result;
}
esp_err_t demo_buzzer_stop()
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t duty = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    const esp_err_t update = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    return duty != ESP_OK ? duty : update;
}
esp_err_t demo_buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (freq_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(kTag, "Playing tone: %lu Hz for %lu ms", static_cast<unsigned long>(freq_hz),
             static_cast<unsigned long>(duration_ms));
    esp_err_t result = ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
    if (result == ESP_OK) {
        result = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    }
    if (result == ESP_OK) {
        result = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
    if (result == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
    }
    const esp_err_t stop = demo_buzzer_stop();
    if (stop != ESP_OK) {
        ESP_LOGE(kTag, "Silence buzzer failed: %s", esp_err_to_name(stop));
    }
    return result != ESP_OK ? result : stop;
}
esp_err_t demo_buzzer_run()
{
    const uint32_t frequencies[] = {523, 659, 784, 1047};
    for (unsigned i = 0; i < 4; ++i) {
        const esp_err_t result = demo_buzzer_tone(frequencies[i], i == 3 ? 500 : 300);
        if (result != ESP_OK) {
            return result;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return ESP_OK;
}
