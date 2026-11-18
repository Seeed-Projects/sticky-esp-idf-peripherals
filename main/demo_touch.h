#pragma once
#include "esp_err.h"

// Call from one task after demo_board_init(); check esp_err_t results.

#include "driver/i2c_master.h"
#include "epaper_panel.h"

inline constexpr unsigned kDemoTouchDurationSeconds = 15;

void demo_touch_transform_coordinate(uint16_t touch_x, uint16_t touch_y, uint16_t &display_x,
                                     uint16_t &display_y);

esp_err_t demo_touch_run(i2c_master_bus_handle_t touch_bus,
                         seeed_epaper_panel_handle_t epaper_panel);
