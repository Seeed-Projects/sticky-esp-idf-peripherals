#include "demo_microphone.h"

#include <cmath>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

static const char *TAG = "demo_microphone";

esp_err_t demo_microphone_run()
{
    ESP_LOGI(TAG, "=== Microphone Demo Start ===");

    gpio_config_t power_cfg = {};
    power_cfg.pin_bit_mask = 1ULL << PIN_MIC_EN;
    power_cfg.mode = GPIO_MODE_OUTPUT;
    power_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    power_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_cfg.intr_type = GPIO_INTR_DISABLE;
    esp_err_t ret = gpio_config(&power_cfg);
    if (ret == ESP_OK) {
        ret = gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 1);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Microphone power enable failed: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== Microphone Demo End ===");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    i2s_chan_handle_t rx_channel = nullptr;
    i2s_chan_config_t channel_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ret = i2s_new_channel(&channel_cfg, nullptr, &rx_channel);

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .clk = static_cast<gpio_num_t>(PIN_MIC_CLK),
                .din = static_cast<gpio_num_t>(PIN_MIC_DATA),
                .invert_flags =
                    {
                        .clk_inv = false,
                    },
            },
    };
    if (ret == ESP_OK) {
        ret = i2s_channel_init_pdm_rx_mode(rx_channel, &pdm_cfg);
    }
    bool enabled = false;
    if (ret == ESP_OK) {
        ret = i2s_channel_enable(rx_channel);
        enabled = ret == ESP_OK;
    }

    if (ret == ESP_OK) {
        int16_t samples[512] = {};
        for (int reading = 0; reading < 10; ++reading) {
            size_t bytes_read = 0;
            ret = i2s_channel_read(rx_channel, samples, sizeof(samples), &bytes_read, 1000);
            if (ret != ESP_OK) {
                break;
            }
            if (bytes_read == 0 || bytes_read % sizeof(samples[0]) != 0) {
                ret = ESP_ERR_INVALID_SIZE;
                break;
            }

            const size_t sample_count = bytes_read / sizeof(samples[0]);
            double mean = 0.0;
            for (size_t i = 0; i < sample_count; ++i) {
                mean += samples[i];
            }
            mean /= sample_count;

            double sum_squares = 0.0;
            for (size_t i = 0; i < sample_count; ++i) {
                const double centered = samples[i] - mean;
                sum_squares += centered * centered;
            }
            const double rms = std::sqrt(sum_squares / sample_count);
            if ((reading % 2) == 1) {
                ESP_LOGI(TAG, "Microphone RMS: %.1f", rms);
            }
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PDM microphone test failed: %s", esp_err_to_name(ret));
    }
    if (rx_channel != nullptr) {
        const esp_err_t disable = enabled ? i2s_channel_disable(rx_channel) : ESP_OK;
        const esp_err_t remove = i2s_del_channel(rx_channel);
        if (disable != ESP_OK || remove != ESP_OK) {
            ESP_LOGE(TAG, "I2S cleanup failed");
        }
        if (ret == ESP_OK) {
            ret = disable != ESP_OK ? disable : remove;
        }
    }
    const esp_err_t power = gpio_set_level(static_cast<gpio_num_t>(PIN_MIC_EN), 0);
    if (ret == ESP_OK) {
        ret = power;
    }
    ESP_LOGI(TAG, "=== Microphone Demo End ===");
    return ret;
}
