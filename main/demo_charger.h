#pragma once
#include "esp_err.h"

// Call from one task after demo_board_init(); check esp_err_t results.
esp_err_t demo_charger_init();
esp_err_t demo_charger_run();
