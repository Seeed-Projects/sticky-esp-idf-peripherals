#pragma once

#include "esp_err.h"

enum class DemoPowerMode { DeepSleep, Off };

esp_err_t demo_power_init();
esp_err_t demo_power_release_holds();
esp_err_t demo_power_enter(DemoPowerMode mode);
