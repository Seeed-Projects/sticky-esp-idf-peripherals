#include "demo_buzzer.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

static const char *TAG = "demo_buzzer";

void demo_buzzer_init()
{
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_cfg.timer_num = LEDC_TIMER_0;
    timer_cfg.duty_resolution = LEDC_TIMER_10_BIT;
    timer_cfg.freq_hz = 4000;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Buzzer timer initialization failed: %s", esp_err_to_name(ret));

    ledc_channel_config_t channel_cfg = {};
    channel_cfg.gpio_num = PIN_BUZZER;
    channel_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    channel_cfg.channel = LEDC_CHANNEL_0;
    channel_cfg.intr_type = LEDC_INTR_DISABLE;
    channel_cfg.timer_sel = LEDC_TIMER_0;
    channel_cfg.duty = 0;
    channel_cfg.hpoint = 0;

    ret = ledc_channel_config(&channel_cfg);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Buzzer channel initialization failed: %s", esp_err_to_name(ret));
}

void demo_buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    ESP_LOGI(TAG, "Playing tone: %lu Hz for %lu ms",
             static_cast<unsigned long>(freq_hz),
             static_cast<unsigned long>(duration_ms));

    esp_err_t ret = ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Set buzzer frequency failed: %s", esp_err_to_name(ret));

    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Set buzzer duty failed: %s", esp_err_to_name(ret));

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Update buzzer duty failed: %s", esp_err_to_name(ret));

    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    demo_buzzer_stop();
}

void demo_buzzer_stop()
{
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Stop buzzer failed: %s", esp_err_to_name(ret));

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Apply buzzer stop failed: %s", esp_err_to_name(ret));
}

void demo_buzzer_run()
{
    ESP_LOGI(TAG, "=== Buzzer Demo Start ===");

    demo_buzzer_tone(523, 300);
    vTaskDelay(pdMS_TO_TICKS(500));

    demo_buzzer_tone(659, 300);
    vTaskDelay(pdMS_TO_TICKS(500));

    demo_buzzer_tone(784, 300);
    vTaskDelay(pdMS_TO_TICKS(500));

    demo_buzzer_tone(1047, 500);
    vTaskDelay(pdMS_TO_TICKS(500));

    demo_buzzer_stop();
    ESP_LOGI(TAG, "=== Buzzer Demo End ===");
}
