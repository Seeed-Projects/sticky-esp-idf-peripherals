#include "demo_epaper.h"

#include <cstring>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

static const char *TAG = "demo_epaper";

static constexpr uint16_t kPanelWidth = 800;
static constexpr uint16_t kPanelHeight = 480;
static constexpr uint16_t kGray4StrideBytes = kPanelWidth / 4;
static constexpr size_t kGray4BufferSize = kGray4StrideBytes * kPanelHeight;
static constexpr uint16_t kGray4BandHeight = kPanelHeight / 4;
static constexpr uint16_t kDotSize = 8;
static constexpr size_t kDisplayStride = (kPanelWidth + 7) / 8;  // 100 bytes per row
static constexpr size_t kDisplayBufSize = kDisplayStride * kPanelHeight;  // 48000 bytes

// Full-screen framebuffer: tracks what we WANT on screen (1bpp MSB, 0xFF=white)
static uint8_t *s_display_buf = nullptr;

static void fill_gray4_demo_buffer(uint8_t *buffer)
{
    const uint8_t band_values[] = {0x00, 0x55, 0xAA, 0xFF};

    for (uint16_t band = 0; band < 4; ++band) {
        const uint16_t start_row = band * kGray4BandHeight;
        const uint16_t end_row = start_row + kGray4BandHeight;

        for (uint16_t row = start_row; row < end_row; ++row) {
            std::memset(buffer + row * kGray4StrideBytes,
                        band_values[band],
                        kGray4StrideBytes);
        }
    }
}

static void show_gray4_demo(seeed_epaper_panel_handle_t panel)
{
    uint8_t *buffer = static_cast<uint8_t *>(heap_caps_malloc(kGray4BufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate gray4 demo buffer");
        return;
    }

    fill_gray4_demo_buffer(buffer);

    seeed_epaper_area_t area = {};
    area.x = 0;
    area.y = 0;
    area.width = kPanelWidth;
    area.height = kPanelHeight;

    ESP_LOGI(TAG, "Gray4 demo: displaying 4-level grayscale bands");
    esp_err_t ret = seeed_epaper_panel_write_bitmap_gray4(panel, &area, buffer, kGray4StrideBytes);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gray4 demo failed: %s", esp_err_to_name(ret));
        heap_caps_free(buffer);
        return;
    }

    ret = seeed_epaper_panel_commit(panel, &area, SEEED_EPAPER_REFRESH_GRAY4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gray4 demo commit failed: %s", esp_err_to_name(ret));
    }

    heap_caps_free(buffer);
}

seeed_epaper_panel_handle_t demo_epaper_init(void)
{
    ESP_LOGI(TAG, "=== E-paper Demo Start ===");

    gpio_config_t power_cfg = {};
    power_cfg.pin_bit_mask = 1ULL << PIN_EPD_EN;
    power_cfg.mode = GPIO_MODE_OUTPUT;
    power_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    power_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&power_cfg);
    gpio_set_level(static_cast<gpio_num_t>(PIN_EPD_EN), 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = PIN_EPD_MOSI;
    bus_cfg.miso_io_num = PIN_EPD_MISO;
    bus_cfg.sclk_io_num = PIN_EPD_CLK;
    bus_cfg.max_transfer_sz = 48000;
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return nullptr;
    }
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = 10 * 1000 * 1000;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = PIN_EPD_CS;
    dev_cfg.queue_size = 1;
    spi_device_handle_t spi_dev = nullptr;
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return nullptr;
    }

    seeed_epaper_panel_config_t panel_cfg = {};
    panel_cfg.spi_handle = spi_dev;
    panel_cfg.pin_dc = static_cast<gpio_num_t>(PIN_EPD_DC);
    panel_cfg.pin_rst = static_cast<gpio_num_t>(PIN_EPD_RST);
    panel_cfg.pin_busy = static_cast<gpio_num_t>(PIN_EPD_BUSY);
    panel_cfg.pin_enable = GPIO_NUM_NC;
    panel_cfg.busy_timeout_ms = 10000;
    panel_cfg.reset_low_ms = 10;
    panel_cfg.reset_high_ms = 10;
    panel_cfg.busy_level = 1;
    panel_cfg.enable_level = 1;
    panel_cfg.mirror_x = true;
    seeed_epaper_panel_handle_t panel = nullptr;
    ret = seeed_epaper_new_panel(SEEED_EPAPER_PANEL_SSD1677, &panel_cfg, &panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel create failed: %s", esp_err_to_name(ret));
        return nullptr;
    }

    ret = seeed_epaper_panel_clear(panel, true, SEEED_EPAPER_REFRESH_FULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initial clear failed: %s", esp_err_to_name(ret));
    }

    show_gray4_demo(panel);

    vTaskDelay(pdMS_TO_TICKS(5000));

    ret = seeed_epaper_panel_clear(panel, true, SEEED_EPAPER_REFRESH_FULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch drawing clear failed: %s", esp_err_to_name(ret));
    }

    s_display_buf = static_cast<uint8_t *>(
        heap_caps_malloc(kDisplayBufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (s_display_buf != nullptr) {
        std::memset(s_display_buf, 0xFF, kDisplayBufSize);
    } else {
        ESP_LOGE(TAG, "Failed to allocate display framebuffer");
    }

    ESP_LOGI(TAG, "=== E-paper Demo Ready ===");
    return panel;
}

void demo_epaper_draw_dot(seeed_epaper_panel_handle_t panel, uint16_t x, uint16_t y)
{
    if (panel == nullptr || s_display_buf == nullptr) {
        ESP_LOGE(TAG, "Cannot draw dot: panel or framebuffer is null");
        return;
    }

    if (x + kDotSize > kPanelWidth) {
        x = kPanelWidth - kDotSize;
    }
    if (y + kDotSize > kPanelHeight) {
        y = kPanelHeight - kDotSize;
    }

    // Draw 8x8 black dot into our framebuffer (bit=0 means black in MONO1_MSB)
    for (uint16_t row = y; row < y + kDotSize; ++row) {
        for (uint16_t col = x; col < x + kDotSize; ++col) {
            s_display_buf[row * kDisplayStride + col / 8] &= ~(0x80 >> (col % 8));
        }
    }

    // SSD1677 partial refresh drives ALL pixels using DTM1 (old) vs DTM2 (new).
    // Write the full screen so every pixel has correct old/new state.
    // Panel internally reads its framebuffer as DTM1 (previous state).
    seeed_epaper_area_t full_area = {0, 0, kPanelWidth, kPanelHeight};

    esp_err_t ret = seeed_epaper_panel_prepare(panel, SEEED_EPAPER_REFRESH_PARTIAL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Prepare partial failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = seeed_epaper_panel_write_bitmap(panel, &full_area, s_display_buf, kDisplayStride,
                                          SEEED_EPAPER_PIXEL_FORMAT_MONO1_MSB,
                                          SEEED_EPAPER_REFRESH_PARTIAL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write bitmap failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = seeed_epaper_panel_commit(panel, &full_area, SEEED_EPAPER_REFRESH_PARTIAL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Commit failed: %s", esp_err_to_name(ret));
        return;
    }

}
