#include "demo_sdcard.h"

#include <cerrno>
#include <dirent.h>

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"
#include "sdmmc_cmd.h"

static const char *TAG = "demo_sdcard";
static esp_err_t s_cleanup_error = ESP_OK;

bool demo_sdcard_can_power_down()
{
    return s_cleanup_error == ESP_OK;
}
static constexpr const char *kMountPoint = "/sdcard";

// The MicroSD card and e-paper display share SPI2 SCK/MOSI/MISO.
// Keep the card powered and deselected whenever the SD driver is not active.
static esp_err_t configure_sd_idle_state()
{
    gpio_config_t output_cfg = {};
    output_cfg.pin_bit_mask = (1ULL << PIN_SD_EN) | (1ULL << PIN_SD_CS);
    output_cfg.mode = GPIO_MODE_OUTPUT;
    output_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    output_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    output_cfg.intr_type = GPIO_INTR_DISABLE;
    esp_err_t ret = gpio_config(&output_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = gpio_set_level(static_cast<gpio_num_t>(PIN_SD_CS), 1);
    if (ret != ESP_OK) {
        return ret;
    }
    return gpio_set_level(static_cast<gpio_num_t>(PIN_SD_EN), 1);
}

// Probe one directory entry only. No file preparation, counting or recursive scan.
static esp_err_t probe_root_directory()
{
    DIR *directory = opendir(kMountPoint);
    if (directory == nullptr) {
        ESP_LOGE(TAG, "Root directory open failed: errno=%d", errno);
        return ESP_FAIL;
    }
    errno = 0;
    const dirent *entry = readdir(directory);
    const int read_error = errno;
    const bool empty = entry == nullptr && read_error == 0;
    const int close_result = closedir(directory);
    if (read_error != 0 || close_result != 0) {
        ESP_LOGE(TAG, "Root directory read/close failed: errno=%d", read_error);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Root directory readable%s", empty ? " (empty)" : " (entry available)");
    return ESP_OK;
}

esp_err_t demo_sdcard_run()
{
    ESP_LOGI(TAG, "=== MicroSD Demo Start ===");

    if (s_cleanup_error != ESP_OK) {
        return s_cleanup_error;
    }
    esp_err_t ret = configure_sd_idle_state();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MicroSD idle state setup failed: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== MicroSD Demo End ===");
        return ret;
    }

    gpio_config_t detect_cfg = {};
    detect_cfg.pin_bit_mask = 1ULL << PIN_SD_DETECT;
    detect_cfg.mode = GPIO_MODE_INPUT;
    detect_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    detect_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    detect_cfg.intr_type = GPIO_INTR_DISABLE;
    ret = gpio_config(&detect_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MicroSD detect pin setup failed: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== MicroSD Demo End ===");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (gpio_get_level(static_cast<gpio_num_t>(PIN_SD_DETECT)) != 0) {
        ESP_LOGW(TAG, "No MicroSD card detected");
        ESP_LOGI(TAG, "=== MicroSD Demo End ===");
        return ESP_ERR_NOT_FOUND;
    }

    sdmmc_card_t *card = nullptr;
    {
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = SPI2_HOST;
        sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_cfg.gpio_cs = static_cast<gpio_num_t>(PIN_SD_CS);
        slot_cfg.host_id = SPI2_HOST;
        esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {};
        mount_cfg.format_if_mount_failed = false;
        mount_cfg.max_files = 3;
        mount_cfg.allocation_unit_size = 16 * 1024;
        ret = esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slot_cfg, &mount_cfg, &card);
    }

    if (ret == ESP_OK) {
        const uint64_t capacity_mib =
            static_cast<uint64_t>(card->csd.capacity) * card->csd.sector_size / (1024 * 1024);
        ESP_LOGI(TAG, "Mounted, capacity=%llu MiB", static_cast<unsigned long long>(capacity_mib));
        ret = probe_root_directory();
        const esp_err_t cleanup = esp_vfs_fat_sdcard_unmount(kMountPoint, card);
        if (cleanup == ESP_OK) {
            ESP_LOGI(TAG, "Unmounted");
        } else {
            s_cleanup_error = cleanup;
            ESP_LOGE(TAG, "SD unmount failed: %s; restart before retrying SD",
                     esp_err_to_name(cleanup));
            if (ret == ESP_OK) {
                ret = cleanup;
            }
        }
    } else {
        ESP_LOGE(TAG, "MicroSD mount failed: %s", esp_err_to_name(ret));
    }

    const esp_err_t idle_result = configure_sd_idle_state();
    if (idle_result != ESP_OK) {
        ESP_LOGE(TAG, "Restore MicroSD idle state failed: %s", esp_err_to_name(idle_result));
    }
    ESP_LOGI(TAG, "=== MicroSD Demo End ===");
    return ret != ESP_OK ? ret : idle_result;
}
