#pragma once

#include <cstdint>

void demo_buzzer_init();
void demo_buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);
void demo_buzzer_stop();
void demo_buzzer_run();
