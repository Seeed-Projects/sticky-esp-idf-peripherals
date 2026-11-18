#pragma once

#include "demo_power.h"
#include "esp_err.h"

// Call from one task after demo_board_init(); check esp_err_t results.

esp_err_t demo_buttons_init();
esp_err_t demo_buttons_wait_power_request(DemoPowerMode &mode);
