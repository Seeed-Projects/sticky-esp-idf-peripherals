#include "demo_epaper.h"
#include "demo_font.h"
#include "demo_touch.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {
constexpr char kTag[] = "demo_epaper";
constexpr uint16_t kWidth = 800, kHeight = 480, kDotSize = 8;
constexpr size_t kMonoStride = kWidth / 8, kMonoSize = kMonoStride * kHeight;
constexpr size_t kGrayStride = kWidth / 4, kGraySize = kGrayStride * kHeight;
constexpr seeed_epaper_area_t kScreen = {0, 0, kWidth, kHeight};
seeed_epaper_panel_handle_t s_panel = nullptr;
spi_device_handle_t s_spi = nullptr;
uint8_t *s_pixels = nullptr;
bool s_ready = false;
bool s_refresh_failed = false;
bool s_dirty = false;
unsigned s_partial_count = 0;
// Small maintenance policy for repeated touch dots, not a controller limit.
constexpr unsigned kPartialLimit = 20;

esp_err_t refresh(seeed_epaper_refresh_mode_t mode)
{
    const int64_t start = esp_timer_get_time();
    const esp_err_t result = seeed_epaper_panel_refresh_area(
        s_panel, &kScreen, s_pixels, kMonoStride, SEEED_EPAPER_PIXEL_FORMAT_MONO1_MSB, mode);
    s_refresh_failed = result != ESP_OK;
    if (result == ESP_OK) {
        s_dirty = false;
        s_partial_count = mode == SEEED_EPAPER_REFRESH_FULL ? 0 : s_partial_count + 1;
        if (s_ready) {
            // Touch updates are already represented by press/countdown logs.
            ESP_LOGD(kTag, "%s refresh OK, elapsed=%lld ms",
                     mode == SEEED_EPAPER_REFRESH_FULL ? "Mono full" : "Mono partial",
                     static_cast<long long>((esp_timer_get_time() - start) / 1000));
        } else {
            ESP_LOGI(kTag, "%s refresh OK, elapsed=%lld ms",
                     mode == SEEED_EPAPER_REFRESH_FULL ? "Mono full" : "Mono partial",
                     static_cast<long long>((esp_timer_get_time() - start) / 1000));
        }
    } else {
        ESP_LOGE(kTag, "Refresh failed: %s", esp_err_to_name(result));
    }
    return result;
}

// Status artwork only: use the Dashboard's 180-degree logical-to-panel mapping.
// Touch points already use native framebuffer coordinates and are not rotated here.
void status_pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return;
    }
    const int px = kWidth - 1 - x, py = kHeight - 1 - y;
    s_pixels[py * kMonoStride + px / 8] &= ~(0x80 >> (px % 8));
}

void status_rect(int x, int y, int width, int height)
{
    for (int i = 0; i < width; ++i) {
        status_pixel(x + i, y);
        status_pixel(x + i, y + height - 1);
    }
    for (int i = 0; i < height; ++i) {
        status_pixel(x, y + i);
        status_pixel(x + width - 1, y + i);
    }
}

void status_text(int y, const char *text, int scale)
{
    const int width = static_cast<int>(std::strlen(text)) * (kFontWidth + kFontSpacing) * scale;
    int x = (kWidth - width) / 2;
    for (const char *cursor = text; *cursor != 0; ++cursor) {
        const FontGlyph &glyph = font_get_glyph(*cursor);
        for (int col = 0; col < kFontWidth; ++col) {
            for (int row = 0; row < kFontHeight; ++row) {
                if ((glyph.columns[col] & (1U << row)) == 0) {
                    continue;
                }
                for (int dy = 0; dy < scale; ++dy) {
                    for (int dx = 0; dx < scale; ++dx) {
                        status_pixel(x + col * scale + dx, y + row * scale + dy);
                    }
                }
            }
        }
        x += (kFontWidth + kFontSpacing) * scale;
    }
}

void status_header(const char *section)
{
    std::memset(s_pixels, 0xFF, kMonoSize);
    status_text(32, "reTerminal Sticky", 3);
    status_text(68, section, 2);
    status_rect(40, 104, 720, 1);
}

void status_frame(const char *title, const char *message, const char *hint, const char *footer)
{
    status_header("PERIPHERAL DEMO");
    status_text(174, title, 3);
    status_text(244, message, 2);
    status_text(300, hint, 2);
    status_rect(40, 410, 720, 1);
    status_text(438, footer, 2);
}

void status_circle(int center_x, int center_y, int radius)
{
    int x = radius, y = 0, error = 1 - radius;
    while (x >= y) {
        status_pixel(center_x + x, center_y + y);
        status_pixel(center_x + y, center_y + x);
        status_pixel(center_x - y, center_y + x);
        status_pixel(center_x - x, center_y + y);
        status_pixel(center_x - x, center_y - y);
        status_pixel(center_x - y, center_y - x);
        status_pixel(center_x + y, center_y - x);
        status_pixel(center_x + x, center_y - y);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

void touch_frame()
{
    status_header("TOUCH TEST");
    status_text(126, "Tap or move your finger", 2);
    status_rect(40, 410, 720, 1);
    char hint[64];
    std::snprintf(hint, sizeof(hint), "%u-second test / Countdown in serial log",
                  kDemoTouchDurationSeconds);
    status_text(438, hint, 2);
    // Simple corner targets; the whole screen remains touchable.
    for (int x : {84, 716}) {
        for (int y : {190, 370}) {
            status_circle(x, y, 14);
        }
    }
}

esp_err_t show_gray_bands()
{
    auto *pixels =
        static_cast<uint8_t *>(heap_caps_malloc(kGraySize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    const uint8_t bands[] = {0x00, 0x55, 0xAA, 0xFF};
    for (size_t row = 0; row < kHeight; ++row) {
        std::memset(pixels + row * kGrayStride, bands[row / (kHeight / 4)], kGrayStride);
    }
    const int64_t start = esp_timer_get_time();
    esp_err_t result =
        seeed_epaper_panel_write_bitmap_gray4(s_panel, &kScreen, pixels, kGrayStride);
    if (result == ESP_OK) {
        result = seeed_epaper_panel_commit(s_panel, &kScreen, SEEED_EPAPER_REFRESH_GRAY4);
    }
    heap_caps_free(pixels);
    if (result == ESP_OK) {
        ESP_LOGI(kTag, "Gray4 bands OK, elapsed=%lld ms",
                 static_cast<long long>((esp_timer_get_time() - start) / 1000));
    }
    return result;
}

esp_err_t initialize(const float *temperature_c)
{
    gpio_config_t power = {};
    power.pin_bit_mask = 1ULL << PIN_EPD_EN;
    power.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&power), kTag, "configure panel power");
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(PIN_EPD_EN), 1), kTag,
                        "power panel");
    vTaskDelay(pdMS_TO_TICKS(100));
    spi_device_interface_config_t spi = {};
    spi.clock_speed_hz = 10 * 1000 * 1000;
    spi.mode = 0;
    spi.spics_io_num = PIN_EPD_CS;
    // Controller ID and temperature are read over bidirectional MOSI.
    spi.flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE;
    spi.queue_size = 4;
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI2_HOST, &spi, &s_spi), kTag, "add panel device");
    seeed_epaper_panel_config_t config = {};
    config.spi_handle = s_spi;
    config.pin_cs = static_cast<gpio_num_t>(PIN_EPD_CS);
    config.pin_dc = static_cast<gpio_num_t>(PIN_EPD_DC);
    config.pin_rst = static_cast<gpio_num_t>(PIN_EPD_RST);
    config.pin_busy = static_cast<gpio_num_t>(PIN_EPD_BUSY);
    config.pin_enable = GPIO_NUM_NC;
    config.busy_timeout_ms = 30000;
    config.reset_low_ms = 20;
    config.reset_high_ms = 10;
    config.busy_level = 0;
    config.busy_level_valid = true;
    config.enable_level = 1;
    config.enable_level_valid = true;
    config.manual_cs = false;
    config.mirror_x = true; // Preserve the original demo framebuffer orientation.
    // The SSD-family constructor probes 0x70; 0x07 -> SSD2677, otherwise SSD1677.
    ESP_RETURN_ON_ERROR(seeed_epaper_new_panel(SEEED_EPAPER_PANEL_SSD1677, &config, &s_panel), kTag,
                        "create automatically detected panel");
    seeed_epaper_panel_model_t model;
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_get_model(s_panel, &model), kTag, "get panel model");
    ESP_LOGI(kTag, "Detected controller: %s",
             model == SEEED_EPAPER_PANEL_SSD2677 ? "SSD2677" : "SSD1677");
    if (temperature_c != nullptr) {
        ESP_RETURN_ON_ERROR(seeed_epaper_panel_set_external_temperature(s_panel, *temperature_c),
                            kTag, "set waveform temperature");
        ESP_LOGI(kTag, "Waveform temperature: SHT40 %.1f C", static_cast<double>(*temperature_c));
    } else {
        ESP_LOGW(kTag, "Waveform temperature: internal fallback");
    }
    s_pixels =
        static_cast<uint8_t *>(heap_caps_malloc(kMonoSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (s_pixels == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    status_frame("Testing peripherals", "Please wait...",
                 "Touch test will start automatically", "See the serial log for test details.");
    return refresh(SEEED_EPAPER_REFRESH_FULL);
}
} // namespace

esp_err_t demo_epaper_init(seeed_epaper_panel_handle_t *panel, const float *temperature_c)
{
    if (panel == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *panel = nullptr;
    if (s_ready) {
        *panel = s_panel;
        return ESP_OK;
    }
    // A previous cleanup failure must not be hidden by overwriting a live handle.
    if (s_panel != nullptr || s_spi != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t result = initialize(temperature_c);
    if (result == ESP_OK) {
        s_ready = true;
        *panel = s_panel;
        return ESP_OK;
    }
    ESP_LOGE(kTag, "Display initialization failed: %s", esp_err_to_name(result));
    heap_caps_free(s_pixels);
    s_pixels = nullptr;
    if (s_panel != nullptr) {
        const esp_err_t cleanup = seeed_epaper_panel_del(s_panel);
        if (cleanup == ESP_OK) {
            s_panel = nullptr;
        } else {
            ESP_LOGE(kTag, "Panel cleanup failed: %s", esp_err_to_name(cleanup));
        }
    }
    if (s_panel == nullptr && s_spi != nullptr) {
        const esp_err_t cleanup = spi_bus_remove_device(s_spi);
        if (cleanup == ESP_OK) {
            s_spi = nullptr;
        } else {
            ESP_LOGE(kTag, "SPI device cleanup failed: %s", esp_err_to_name(cleanup));
        }
    }
    // SPI2 belongs to demo_board; do not free it or abruptly cut EPD_EN.
    return result;
}

esp_err_t demo_epaper_run(seeed_epaper_panel_handle_t panel)
{
    if (!s_ready || panel != s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t gray_result = show_gray_bands();
    if (gray_result != ESP_OK) {
        s_refresh_failed = true;
        return gray_result;
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
    touch_frame();
    return refresh(SEEED_EPAPER_REFRESH_FULL);
}

esp_err_t demo_epaper_finish(seeed_epaper_panel_handle_t panel, bool checks_failed)
{
    if (!s_ready || panel != s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    status_frame("Demo complete",
                 checks_failed ? "Some checks need attention." : "Touch test has ended.",
                 checks_failed ? "See the serial log for details."
                               : "Reset the board to run again.",
                 checks_failed ? "Reset the board to run again."
                               : "Buttons remain active. See serial log.");
    return refresh(SEEED_EPAPER_REFRESH_FULL);
}

esp_err_t demo_epaper_add_dot(seeed_epaper_panel_handle_t panel, uint16_t x, uint16_t y)
{
    if (!s_ready || panel != s_panel || s_pixels == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (x >= kWidth || y >= kHeight) {
        return ESP_ERR_INVALID_ARG;
    }
    if (x > kWidth - kDotSize) {
        x = kWidth - kDotSize;
    }
    if (y > kHeight - kDotSize) {
        y = kHeight - kDotSize;
    }
    for (uint16_t row = y; row < y + kDotSize; ++row) {
        for (uint16_t col = x; col < x + kDotSize; ++col) {
            uint8_t &pixel_byte = s_pixels[row * kMonoStride + col / 8];
            const uint8_t mask = 0x80 >> (col % 8);
            if ((pixel_byte & mask) != 0) {
                pixel_byte &= ~mask;
                s_dirty = true;
            }
        }
    }
    return ESP_OK;
}

esp_err_t demo_epaper_flush(seeed_epaper_panel_handle_t panel)
{
    if (!s_ready || panel != s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_dirty && !s_refresh_failed) {
        return ESP_OK;
    }
    // Submit a full monochrome comparison frame for either controller.
    // The official component handles the previous/current RAM representation.
    return refresh(s_refresh_failed || s_partial_count >= kPartialLimit
                       ? SEEED_EPAPER_REFRESH_FULL
                       : SEEED_EPAPER_REFRESH_PARTIAL);
}

esp_err_t demo_epaper_draw_dot(seeed_epaper_panel_handle_t panel, uint16_t x, uint16_t y)
{
    const esp_err_t result = demo_epaper_add_dot(panel, x, y);
    return result == ESP_OK ? demo_epaper_flush(panel) : result;
}

esp_err_t demo_epaper_power_notice(seeed_epaper_panel_handle_t panel, const char *title,
                                   const char *message, const char *hint)
{
    if (!s_ready || panel != s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    status_frame(title, message, hint, "");
    return refresh(SEEED_EPAPER_REFRESH_FULL);
}

esp_err_t demo_epaper_sleep(seeed_epaper_panel_handle_t panel)
{
    const esp_err_t result = demo_epaper_standby(panel);
    return result == ESP_OK ? seeed_epaper_panel_sleep(panel) : result;
}

esp_err_t demo_epaper_standby(seeed_epaper_panel_handle_t panel)
{
    if (!s_ready || panel != s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t result = seeed_epaper_panel_standby(panel);
    if (result != ESP_OK) {
        s_refresh_failed = true;
    }
    return result;
}
