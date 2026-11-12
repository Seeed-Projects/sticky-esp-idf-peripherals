#include "demo_sdcard.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

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
    if (ret != ESP_OK) return ret;

    ret = gpio_set_level(static_cast<gpio_num_t>(PIN_SD_EN), 1);
    if (ret != ESP_OK) return ret;
    return gpio_set_level(static_cast<gpio_num_t>(PIN_SD_CS), 1);
}

static void list_root_directory()
{
    ESP_LOGI(TAG, "Files in MicroSD root directory:");
    DIR *directory = opendir(kMountPoint);
    if (directory == nullptr) {
        ESP_LOGE(TAG, "Failed to open %s: errno=%d (%s)",
                 kMountPoint, errno, std::strerror(errno));
        return;
    }

    int entry_count = 0;
    dirent *entry = nullptr;
    while ((entry = readdir(directory)) != nullptr) {
        char path[288] = {};
        std::snprintf(path, sizeof(path), "%s/%s", kMountPoint, entry->d_name);

        struct stat file_stat = {};
        if (stat(path, &file_stat) == 0) {
            ESP_LOGI(TAG, "  %s  %s  %ld bytes",
                     S_ISDIR(file_stat.st_mode) ? "DIR " : "FILE",
                     entry->d_name,
                     static_cast<long>(file_stat.st_size));
        } else {
            ESP_LOGI(TAG, "  %s", entry->d_name);
        }
        ++entry_count;
    }

    closedir(directory);
    ESP_LOGI(TAG, "Root directory entries: %d", entry_count);
}

static void read_test_file()
{
    constexpr const char *kFilePath = "/sdcard/TEST.TXT";
    FILE *file = std::fopen(kFilePath, "rb");
    if (file == nullptr) {
        ESP_LOGW(TAG, "Unable to open %s: errno=%d (%s)",
                 kFilePath, errno, std::strerror(errno));
        return;
    }

    char read_buffer[257] = {};
    const size_t bytes_read = std::fread(read_buffer, 1, sizeof(read_buffer) - 1, file);
    const bool read_error = std::ferror(file) != 0;
    std::fclose(file);
    read_buffer[bytes_read] = '\0';

    if (read_error) {
        ESP_LOGE(TAG, "Failed while reading %s", kFilePath);
        return;
    }

    ESP_LOGI(TAG, "Read %u bytes from %s", static_cast<unsigned>(bytes_read), kFilePath);
    ESP_LOGI(TAG, "TEST.TXT content:\n%s", read_buffer);
}

void demo_sdcard_run()
{
    ESP_LOGI(TAG, "=== MicroSD Demo Start ===");

    esp_err_t ret = configure_sd_idle_state();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MicroSD idle state setup failed: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "=== MicroSD Demo End ===");
        return;
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
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (gpio_get_level(static_cast<gpio_num_t>(PIN_SD_DETECT)) != 0) {
        ESP_LOGW(TAG, "No MicroSD card detected");
        ESP_LOGI(TAG, "=== MicroSD Demo End ===");
        return;
    }

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = PIN_SD_MOSI;
    bus_cfg.miso_io_num = PIN_SD_MISO;
    bus_cfg.sclk_io_num = PIN_SD_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4096;
    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    sdmmc_card_t *card = nullptr;
    if (ret == ESP_OK) {
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
        sdmmc_card_print_info(stdout, card);
        list_root_directory();
        read_test_file();
        esp_vfs_fat_sdcard_unmount(kMountPoint, card);
    } else {
        ESP_LOGE(TAG, "MicroSD mount failed: %s", esp_err_to_name(ret));
    }

    esp_err_t free_ret = spi_bus_free(SPI2_HOST);
    if (free_ret != ESP_OK) {
        ESP_LOGW(TAG, "Free MicroSD SPI bus result: %s", esp_err_to_name(free_ret));
    }

    ret = configure_sd_idle_state();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Restore MicroSD idle state failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "=== MicroSD Demo End ===");
}
