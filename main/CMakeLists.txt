idf_component_register(
    SRCS "main.cpp"
         "demo_board.cpp"
         "demo_power.cpp"
         "demo_buzzer.cpp"
         "demo_buttons.cpp"
         "demo_rtc.cpp"
         "demo_battery.cpp"
         "demo_charger.cpp"
         "demo_epaper.cpp"
         "demo_font.cpp"
         "demo_touch.cpp"
         "demo_sht40.cpp"
         "demo_imu.cpp"
         "demo_microphone.cpp"
         "demo_sdcard.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_driver_spi esp_driver_i2s fatfs sdmmc bq27220 pcf8563 gt911 button seeed_epaper
)
