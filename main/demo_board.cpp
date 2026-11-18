#include "demo_board.h"
#include "demo_power.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"
namespace {
constexpr char kTag[] = "demo_board";
i2c_master_bus_handle_t s_sensor_bus = nullptr;
i2c_master_bus_handle_t s_touch_bus = nullptr;
bool s_ready = false;
esp_err_t output(int pin, int level)
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&config), kTag, "configure output");
    return gpio_set_level(static_cast<gpio_num_t>(pin), level);
}
void init_i2c(i2c_port_num_t port, int sda, int scl, i2c_master_bus_handle_t *bus)
{
    i2c_master_bus_config_t config = {};
    config.i2c_port = port;
    config.sda_io_num = static_cast<gpio_num_t>(sda);
    config.scl_io_num = static_cast<gpio_num_t>(scl);
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    const esp_err_t result = i2c_new_master_bus(&config, bus);
    if (result != ESP_OK) {
        *bus = nullptr;
        ESP_LOGE(kTag, "I2C%d unavailable: %s", static_cast<int>(port), esp_err_to_name(result));
    }
}
} // namespace
esp_err_t demo_board_init()
{
    if (s_ready) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_HOLD), 1),
                        kTag, "preload power hold");
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_LOCK), 1),
                        kTag, "preload power lock");
    ESP_RETURN_ON_ERROR(output(PIN_POWER_HOLD, 1), kTag, "power hold");
    ESP_RETURN_ON_ERROR(output(PIN_POWER_LOCK, 1), kTag, "power lock");
    ESP_RETURN_ON_ERROR(demo_power_release_holds(), kTag, "release retained outputs");
    // Deselect both devices before SPI clocks. SD_EN is active high.
    ESP_RETURN_ON_ERROR(output(PIN_SD_CS, 1), kTag, "SD CS");
    ESP_RETURN_ON_ERROR(output(PIN_SD_EN, 1), kTag, "SD power");
    ESP_RETURN_ON_ERROR(output(PIN_EPD_CS, 1), kTag, "EPD CS");
    ESP_RETURN_ON_ERROR(output(PIN_EPD_DC, 1), kTag, "EPD DC");
    ESP_RETURN_ON_ERROR(output(PIN_EPD_RST, 1), kTag, "EPD reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_err_t result = gpio_install_isr_service(0);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    spi_bus_config_t spi = {};
    spi.mosi_io_num = PIN_EPD_MOSI;
    spi.miso_io_num = PIN_EPD_MISO;
    spi.sclk_io_num = PIN_EPD_CLK;
    spi.quadwp_io_num = -1;
    spi.quadhd_io_num = -1;
    spi.data4_io_num = -1;
    spi.data5_io_num = -1;
    spi.data6_io_num = -1;
    spi.data7_io_num = -1;
    spi.max_transfer_sz = 48000;
    // An unknown existing SPI configuration must not be treated as our own.
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &spi, SPI_DMA_CH_AUTO), kTag,
                        "initialize shared SPI2");
    s_ready = true;
    init_i2c(I2C_NUM_1, PIN_SENSOR_SDA, PIN_SENSOR_SCL, &s_sensor_bus);
    init_i2c(I2C_NUM_0, PIN_TOUCH_SDA, PIN_TOUCH_SCL, &s_touch_bus);
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}
i2c_master_bus_handle_t demo_board_sensor_bus()
{
    return s_sensor_bus;
}
i2c_master_bus_handle_t demo_board_touch_bus()
{
    return s_touch_bus;
}
