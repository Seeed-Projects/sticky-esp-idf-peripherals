#pragma once
#include "epaper_panel.h"
// Requires demo_board_init(). Caller borrows the panel; this module owns it.
// 需先初始化板级总线；面板句柄借给调用者，由本模块持有。
esp_err_t demo_epaper_init(seeed_epaper_panel_handle_t *panel,
                           const float *temperature_c = nullptr);
// Init shows the running notice; run shows Gray4 and then the touch target frame.
esp_err_t demo_epaper_run(seeed_epaper_panel_handle_t panel);
// Call after demo_touch_run returns; a finished sequence does not imply all checks passed.
esp_err_t demo_epaper_finish(seeed_epaper_panel_handle_t panel, bool checks_failed);
// Single owner task only: add pixels without I/O, then flush a batch once.
esp_err_t demo_epaper_add_dot(seeed_epaper_panel_handle_t panel, uint16_t x, uint16_t y);
esp_err_t demo_epaper_flush(seeed_epaper_panel_handle_t panel);
esp_err_t demo_epaper_draw_dot(seeed_epaper_panel_handle_t panel, uint16_t x, uint16_t y);
// Call after touch polling stops. Closes high voltage, keeps EPD_EN high.
esp_err_t demo_epaper_standby(seeed_epaper_panel_handle_t panel);
esp_err_t demo_epaper_power_notice(seeed_epaper_panel_handle_t panel, const char *title,
                                   const char *message, const char *hint);
esp_err_t demo_epaper_sleep(seeed_epaper_panel_handle_t panel);
