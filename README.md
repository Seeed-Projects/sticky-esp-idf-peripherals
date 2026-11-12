# reTerminal Sticky Peripheral Demo

A minimal ESP-IDF example project for validating and learning how to use the onboard hardware of the Seeed Studio reTerminal Sticky.

The demos run sequentially after boot and keep each peripheral implementation in a separate `demo_xxx.cpp/.h` pair so that users can easily reuse the relevant code in their own applications.

## Requirements

- reTerminal Sticky
- ESP-IDF v5.4
- A USB-C data cable
- Optional: a FAT-formatted MicroSD card containing `TEST.TXT` in its root directory

Target configuration:

- MCU: ESP32-S3
- Flash: 32 MB
- PSRAM: 8 MB octal PSRAM

## Peripherals Covered

| Peripheral | Device / method | Interface | Demo |
|---|---|---|---|
| Buzzer | LEDC PWM | GPIO48 | `demo_buzzer.cpp` |
| Buttons | ESP IoT Button | GPIO5 / GPIO6 / GPIO4 | `demo_buttons.cpp` |
| Battery charger | BQ25616 control signals | GPIO39 / GPIO40 | `demo_charger.cpp` |
| Real-time clock | PCF8563 | I2C1, address `0x51` | `demo_rtc.cpp` |
| Battery fuel gauge | BQ27220 | I2C1, address `0x55` | `demo_battery.cpp` |
| Temperature and humidity | SHT40 | I2C1, address `0x44` | `demo_sht40.cpp` |
| 6-axis IMU | LSM6DS3TR-C | I2C1, address `0x6A` | `demo_imu.cpp` |
| PDM microphone | Digital PDM input | GPIO19 / GPIO20 / GPIO38 | `demo_microphone.cpp` |
| MicroSD card | SDSPI + FATFS | Shared SPI2 bus | `demo_sdcard.cpp` |
| E-paper display | SSD1677 | Shared SPI2 bus | `demo_epaper.cpp` |
| Capacitive touch | GT911 | I2C0 | `demo_touch.cpp` |

## Project Structure

```text
Sticky_peripheral_demo/
|-- CMakeLists.txt
|-- dependencies.lock
|-- sdkconfig.defaults
|-- main/
|   |-- CMakeLists.txt
|   |-- idf_component.yml
|   |-- main.cpp
|   |-- pin_config.h
|   `-- demo_xxx.cpp/.h
`-- components/
    |-- bq27220/
    |-- button/
    |-- debug_logging/
    |-- gt911/
    |-- pcf8563/
    `-- seeed_epaper/
```

`components/` contains source dependencies required by the project. Components downloaded by the ESP-IDF Component Manager are recreated automatically during configuration.

The first build requires network access so that the Component Manager can download the dependencies declared in `main/idf_component.yml`. Keep `dependencies.lock` in the project to use the versions tested with this example.

## Build, Flash, and Monitor

Open an ESP-IDF v5.4 terminal, then run:

```bash
cd <project-directory>
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `<project-directory>` with the extracted project directory and `PORT` with the device serial port.

To exit the serial monitor, press `Ctrl+]`.

## Demo Sequence

The application performs the following tests after boot:

1. Holds the board power circuit enabled and initializes the shared services.
2. Plays a short sequence of buzzer tones.
3. Registers callbacks for the UP, DOWN, and OK buttons.
4. Checks the charger status and restores charging to normal operation.
5. Initializes the PCF8563, disables CLKOUT, and prints the current RTC time. The demo does not change the RTC time.
6. Reads voltage, current, state of charge, state of health, temperature, cycle count, and status from the BQ27220.
7. Reads temperature and relative humidity from the SHT40.
8. Reads one accelerometer and gyroscope sample from the LSM6DS3TR-C.
9. Samples the PDM microphone and prints several RMS volume values.
10. Mounts the MicroSD card read-only, lists its root directory, and attempts to read `/sdcard/TEST.TXT`.
11. Initializes the e-paper panel and displays four grayscale bands.
12. Runs the touch drawing test for 30 seconds and prints raw, logical-screen, and framebuffer coordinates.

Errors remain visible as `ESP_LOGE` messages. After the sequence finishes, the button callbacks remain active.

## Pin Map

### Power and user input

| Signal | GPIO | Notes |
|---|---:|---|
| POWER / OK button | 4 | Active low |
| UP button | 5 | Active low |
| DOWN button | 6 | Active low |
| Power hold | 45 | Kept high after boot |
| Power lock | 46 | Kept high after boot |
| Buzzer | 48 | LEDC PWM output |

### Battery and charger

| Signal | GPIO | Notes |
|---|---:|---|
| Charger enable | 39 | Active low |
| Charger status | 40 | Input |
| Fuel-gauge interrupt | 7 | Shared board signal; not used by this demo |

### I2C buses

| Bus | SCL | SDA | Devices |
|---|---:|---:|---|
| I2C0 | 2 | 3 | GT911 touch controller |
| I2C1 | 0 | 1 | PCF8563, BQ27220, SHT40, LSM6DS3TR-C |

The GT911 additionally uses GPIO42 for power enable, GPIO21 for interrupt, and GPIO41 for reset.

### PDM microphone

| Signal | GPIO |
|---|---:|
| PDM clock | 19 |
| PDM data | 20 |
| Microphone enable | 38 |

### Shared SPI2 bus

The MicroSD card and e-paper display share the SPI2 clock, MOSI, and MISO signals. Each demo explicitly deselects the SD card before the e-paper demo starts.

| Signal | GPIO |
|---|---:|
| SPI clock | 13 |
| SPI MOSI | 14 |
| SPI MISO | 12 |
| E-paper CS | 15 |
| E-paper DC | 16 |
| E-paper reset | 17 |
| E-paper busy | 18 |
| E-paper enable | 47 |
| MicroSD CS | 8 |
| MicroSD enable | 10 |
| MicroSD detect | 11 |

## Verification Notes

- RTC: the displayed time comes from the PCF8563. Set it separately if the stored time is incorrect.
- Battery: voltage should normally be within the battery operating range. The BQ27220 capacity registers depend on the fuel-gauge profile programmed on the device; validate that profile before treating the reported capacity as calibrated mAh.
- IMU: with the display facing upward on a level surface, one accelerometer axis should measure approximately `+1 g`, while the other two remain near zero. Small stationary gyroscope offsets are normal without calibration.
- MicroSD: this example intentionally performs no write operation. Place a file named `TEST.TXT` in the card root to test file reading.
- Touch: coordinates are converted from GT911 raw coordinates to the logical display orientation and then to the e-paper framebuffer orientation.

## Using an Individual Demo

Each peripheral is isolated in its own source/header pair. To reuse one in another project:

1. Copy the relevant `demo_xxx.cpp/.h` files.
2. Copy or declare the required pins from `pin_config.h`.
3. Include the corresponding component dependency.
4. Reuse the applicable bus initialization from `main.cpp`.

Some resources are intentionally shared. In particular, coordinate access to I2C1 and ensure that the MicroSD card is deselected before using the e-paper display on SPI2.
