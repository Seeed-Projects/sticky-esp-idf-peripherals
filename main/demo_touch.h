#pragma once

#include "driver/i2c_master.h"
#include "epaper_panel.h"

void demo_touch_transform_coordinate(uint16_t touch_x,
                                     uint16_t touch_y,
                                     uint16_t &display_x,
                                     uint16_t &display_y);

void demo_touch_run(i2c_master_bus_handle_t touch_bus, seeed_epaper_panel_handle_t epaper_panel);
