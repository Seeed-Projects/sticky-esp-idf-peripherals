#pragma once
#include "esp_err.h"

// Call from one task after demo_board_init(); check esp_err_t results.

#include <cstdint>

esp_err_t demo_buzzer_init();
esp_err_t demo_buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);
esp_err_t demo_buzzer_stop();
esp_err_t demo_buzzer_run();
