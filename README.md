# reTerminal Sticky Peripheral Examples

[简体中文](README-ZH.md)

Minimal ESP-IDF examples for the onboard peripherals of Seeed Studio reTerminal Sticky. One firmware automatically selects the SSD1677 or SSD2677 e-paper driver. Each peripheral has a small `demo_xxx.cpp/.h` pair that developers can read, copy and extend, including with AI coding tools.

The application runs the peripheral demos sequentially and ends with a button test and two optional power tests. Its example APIs require a single owner task as described below.

## Requirements and build

- reTerminal Sticky with an SSD1677 or SSD2677 panel; USB-C data cable.
- ESP-IDF **v5.4**, target ESP32-S3, 32 MB flash, 8 MB octal PSRAM.
- Optional FAT-formatted MicroSD card; no test file is required.

Open an ESP-IDF v5.4 terminal in the extracted/cloned project directory:

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port, for example `COM8` on Windows. Exit the monitor with `Ctrl+]`. `set-target` resets local configuration; use it for initial setup or when changing targets. `sdkconfig.defaults` contains the shared defaults; the generated `sdkconfig` is local. Keep `dependencies.lock`; the first configuration may need network access for Component Manager dependencies. No separate screen-specific firmware or manual driver selection is needed.

## What runs after boot

| Step | Source | Behavior |
|---|---|---|
| Board | `demo_board` | Hold board power; deselect SD/display; create SPI2 and I2C1/I2C0; install/reuse GPIO ISR service |
| Environment | `demo_sht40` | CRC-checked temperature/humidity at `0x44`; keep valid startup temperature for the display |
| Display startup | `demo_epaper` | Detect the controller and show “Testing peripherals” before the remaining peripheral demos |
| Buzzer | `demo_buzzer` | Four short PWM tones |
| Charger | `demo_charger` | Enable, briefly disable, restore active-low charger enable; print raw GPIO40 status |
| RTC | `demo_rtc` | Probe PCF8563 at `0x51`, disable CLKOUT and read valid stored time; never write a demo date |
| Battery | `demo_battery` | Read BQ27220 at `0x55`: voltage, current, SOC, SOH, capacity, temperature, cycle count and status |
| IMU | `demo_imu` | LSM6DS3TR-C at `0x6A`: one accel/gyro sample at 104 Hz, ±2 g / ±250 dps; then power down both |
| Microphone | `demo_microphone` | 16 kHz / 16-bit mono PDM; ten reads of up to 512 samples, five RMS log values |
| MicroSD | `demo_sdcard` | Mount, report capacity, probe one root directory entry, unmount; empty cards work; no writes or auto-format |
| Display | `demo_epaper` | Four grayscale bands for five seconds, then a monochrome TOUCH TEST notice and corner targets |
| Touch | `demo_touch` | Capture GT911 for 15 seconds in a separate task; queue and batch 8×8 black dots; detailed coordinates at DEBUG |
| Finish / idle | `demo_epaper` | Show “Demo complete”; enter display standby only if this frame succeeds |
| Buttons / power | `demo_buttons`, `demo_power` | Test all three buttons for 10 seconds, then enable the power actions below |

The final summary counts successful and unavailable/failed operations. Missing optional SD media is reported; an empty root directory succeeds. Board power/SPI initialization failure stops the sequence; an unavailable I2C bus or peripheral is reported and the other demos continue. If display initialization or touch-page preparation fails, touch runs in capture/log-only mode without drawing. The completion frame is attempted once if a panel handle is available. If that refresh fails, standby is explicitly skipped; skipped standby is not counted as successful. A successful touch result means its polling run completed; it does not prove that a finger touched the screen.

## Source layout and reuse

```text
main/
  main.cpp                 Sequential calls and result summary
  pin_config.h             Board pins and I2C addresses
  demo_board.cpp/.h        Shared buses and board startup
  demo_xxx.cpp/.h          Individual peripheral examples
components/
  seeed_epaper/            Complete e-paper component and controller backends
  gt911/                   Touch controller component
  pcf8563/                 RTC register helpers and date validation
  bq27220/                 Fuel-gauge component
  button/                  ESP IoT Button with its existing local ISR patch
  debug_logging/           Optional driver diagnostics
```

All `esp_err_t` entry points must be checked. Run the examples from one task after `demo_board_init()`. Board-owned buses remain alive for the application lifetime. The APIs do not provide an application-wide lock or a concurrent/reentrant driver framework. If you add tasks, serialize each device's complete operation, including mount/read/unmount and prepare/write/commit sequences. Keep slow display/SD work out of button callbacks and interrupts.

To reuse a peripheral, copy its source/header pair, the relevant `pin_config.h` definitions, and the required component; keep the corresponding `REQUIRES` entries from `main/CMakeLists.txt`. Either reuse `demo_board` or explicitly supply equivalent bus/power setup. Sensor examples borrow I2C1 and remove only their temporary device handles. Buttons intentionally remain registered after initialization. `demo_epaper_init()` shows the running notice, `demo_epaper_run()` performs the Gray4 and touch-target demonstration, and `demo_epaper_finish()` shows the stopped notice. The display module owns its SPI device, panel and framebuffer; the returned panel handle is borrowed and must not be deleted by the caller.

For example, after board initialization:

```cpp
const esp_err_t result = demo_sht40_run(demo_board_sensor_bus());
if (result != ESP_OK) {
    ESP_LOGW("example", "SHT40 unavailable: %s", esp_err_to_name(result));
}
```

Include `demo_board.h`, `demo_sht40.h` and `esp_log.h` for this snippet. To call SD or display individually, shared SPI2 must already be initialized and the other device deselected. Do not add a second `spi_bus_initialize()` or call `spi_bus_free()` from a peripheral.

## Display and touch details

The SSD-family constructor sends `0x70` and reads the ID over bidirectional MOSI: `0x07` selects SSD2677; other successfully read values select SSD1677. A failed SPI transaction is an error, not a timeout-based fallback. The official component selects each controller's BUSY polarity and reset timing. The display device uses `SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE`; SD has its own device configuration. All unused SPI bus data pins are `-1`, including those that would otherwise claim sensor SCL GPIO0.

The normal log shows the selected controller and successful refresh modes/times. For raw probe ID and detailed driver timing, enable **Application Debug Logging → Enable e-paper/touch debug logs** in `idf.py menuconfig`, then rebuild.

The component exposes mono full, mono partial and Gray4 full refresh. Gray4 is packed **2 bpp** (four pixels per byte, most significant pixel first); touch uses an **800×480 1 bpp** buffer, with bit 0 black and bit 1 white. `demo_epaper_add_dot()` changes only the framebuffer; `demo_epaper_flush()` sends a dirty batch. Both must be called by the same owner task. Unchanged black pixels do not cause a refresh. `demo_epaper_draw_dot()` remains a convenience wrapper for add + flush. Invalid coordinates are rejected and valid edge dots are clamped inward.

A partial refresh submits a complete comparison frame, not a guaranteed small SPI window. After 20 successful partial refreshes, the next dirty update uses mono full refresh; a failed refresh also makes the next flush full. Successful full refresh clears the count.

A successful startup SHT40 reading sets the external waveform temperature once. Otherwise the component retains its internal-temperature path. Temperature is not sampled on every refresh. Touch preserves the portrait/logical-to-native framebuffer mapping of this example; the touch coordinate transform is unchanged. Status text uses the same 180-degree logical-to-panel conversion as Dashboard, without importing Canvas or a page framework. The GT911 component distinguishes no new report from a release report. A separate task polls about every 20 ms plus I2C time, even while the owner task refreshes the screen. A fixed 128-point queue drops newest samples when full and reports the drop count; repeated coordinates for the same touch ID are suppressed. The owner drains at most 128 points per batch and refreshes once, without adding the old 200 ms delay. This improves capture, not the panel waveform duration: screen updates remain visibly delayed, and this is not a continuous pen/line drawing application. A new touch ID prints one INFO line with raw and screen coordinates; holding/moving does not repeat that line until release and a new press. Detailed motion coordinates use `ESP_LOGD` and are off by default; capture never depends on logging. To see them, compile with DEBUG maximum verbosity and enable DEBUG for `demo_touch`.

The capture task prints the remaining seconds independently of display refresh. Successful touch refresh timing is DEBUG only; startup refresh results and refresh errors remain visible. Capture stops after 15 seconds. The owner waits for the task's stop acknowledgement before releasing its controller/queue, then flushes at most one final batch. A slow in-flight refresh may extend the overall demo. If acknowledgement times out, resources stay alive, touch cannot be restarted without reboot, and an error is returned.

The completion frame enters display standby only after a successful refresh. Display standby keeps EPD_EN high and does not put the ESP32 to sleep.

## Button and power tests

The 10-second button window logs events only. Once the power menu appears, release all keys before starting a new press. Hold UP for about one second for ESP32 deep sleep, or hold AI for about one second for board power-off. Preparation starts at the one-second threshold; the final transition waits for all keys to be released for 60 ms. DOWN does not initiate a power transition.

- Deep sleep retains the board power latch. UP, DOWN and AI are configured as wake sources; wake runs the demo again from startup.
- Power-off follows the official HOLD/LOCK sequence. On battery, press AI to power the board back on after shutdown. If external power keeps the board running, the example waits for key release and restarts once, as the official firmware does.
- The application requires the active peripherals to finish and the SD card to be unmounted. It refreshes the final notice, puts the panel controller to sleep, and then disables the panel, touch, microphone and SD enables. If the transition fails after preparation, use reset to retry.
- The external RTC is left running while backup power is available. Measure current separately on hardware; this example does not specify a shutdown current.

The bundled GT911 driver is retained from the official firmware. This example demonstrates single-finger touch diagnostics. The queue drop count covers application queue overflow only, not all possible missed controller reports.

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

The MicroSD card and e-paper display share the SPI2 clock, MOSI, and MISO signals. `demo_board_init()` initializes SPI2 once. SD and display each use their own CS; neither demo frees the shared bus.

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

## Data and failure boundaries

- **RTC:** VL, malformed BCD and impossible dates are rejected; read failure leaves the output unchanged. No network sync, time-zone conversion or automatic date write is performed. Set a correct clock separately.
- **Battery/charger:** The stock 750 mAh board uses R46=100 mΩ and an official gauge design value of 7500, consistent with 10x current/capacity scaling. The demo checks DesignCapacity()==7500 before interpreting current/capacity at 0.1 physical units per register unit, labels the stock-profile assumption and keeps raw values. If the check fails or the profile differs, it reports raw units only. This check is not a gain/offset calibration test; custom profiles must be reviewed separately. Voltage, temperature and percentages are not divided by ten. Gauge temperature is not certified battery-cell temperature. GPIO40 is raw status, not a measured charging current or a complete Full/Charging/Discharging classifier. GPIO39 is restored low after the charger demo.
- **Microphone:** RMS is computed after per-buffer DC removal; it is a relative sample amplitude, not calibrated dB SPL. This is a short diagnostic, not a one-second recording or audio file.
- **SD:** mount the existing FAT filesystem, report capacity, open the root, call `readdir()` once and close it, then unmount. No file count, recursive traversal, filename listing, test file or file-content claim. An empty root succeeds; underlying FAT access time still depends on the media. The application performs no writes and never auto-formats; the mount itself is not forced read-only. Unmount failure blocks SD retries until reboot, and the old card pointer is never reused. Do not remove the card during access.
- **IMU:** at rest, the acceleration vector magnitude is approximately 1 g; its components depend on orientation. Gyroscope readings should be near zero, with possible offsets. BDU and register auto-increment are enabled before capture.
- **Errors:** returned errors include best-effort cleanup failures. They do not imply that every external hardware fault can be recovered without reboot. Keep WARN/ERROR logs visible.

## Checking on a board

On **each panel variant**, check controller detection, the four distinct grayscale bands, the running/touch/finished notices, touch dots at the four corners/center and repeated dots across a maintenance full refresh. Check SD followed by display with no card, an empty formatted card and a card containing many entries; check sensor values, button logs and charger-enable restoration. Test touch with coordinate DEBUG logs both disabled and enabled, rapid taps/movement, a stationary finger and queue saturation. Review warnings rather than treating the summary as hardware certification. If the card was removed or cleanup failed, reboot before retrying. No sleep-current or lifetime figures are implied by this example.

## License

Project code is provided under the [MIT License](LICENSE), copyright 2026 Seeed Studio. The bundled Espressif button component retains its [Apache-2.0 license](components/button/license.txt), source notices and [local patch record](components/button/LOCAL_PATCHES.md). ESP-IDF and Component Manager dependencies retain their own licenses. Preserve these notices when redistributing.
