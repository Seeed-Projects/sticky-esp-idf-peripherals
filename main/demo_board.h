#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"
// Call once before demos. Buses belong to the board for the app lifetime.
// 演示前调用一次；总线由板级代码持有，不由外设释放。
esp_err_t demo_board_init();
i2c_master_bus_handle_t demo_board_sensor_bus();
i2c_master_bus_handle_t demo_board_touch_bus();
