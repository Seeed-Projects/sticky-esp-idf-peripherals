#pragma once
#include "esp_err.h"

// Call from one task after demo_board_init(); check esp_err_t results.

#include "driver/i2c_master.h"

esp_err_t demo_imu_run(i2c_master_bus_handle_t sensor_bus);
