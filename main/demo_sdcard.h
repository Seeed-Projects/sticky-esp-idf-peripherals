#pragma once
#include "esp_err.h"

// Call from one task after demo_board_init(); check esp_err_t results.

esp_err_t demo_sdcard_run();
bool demo_sdcard_can_power_down();
