# SEEED_EPAPER

ESP-IDF e-paper component for the reTerminal Sticky peripheral examples.
MIT terms are in the root [LICENSE](../../LICENSE).

## Controllers and modes

Backends: UC8179, SSD1677, SSD2677. This example detects SSD1677 or SSD2677
in `main/demo_epaper.cpp`. Both Sticky variants support monochrome
full refresh, monochrome partial refresh, and four-level grayscale full refresh.
Query `seeed_epaper_panel_get_info()` for the selected controller's capabilities.

## Integration

The caller owns the SPI bus and device handle supplied to `seeed_epaper_new_panel()`.
The component handles GPIO, SPI commands, pixel conversion, and refresh sessions.

- `seeed_epaper_panel_prepare()`: prepare a refresh session.
- `seeed_epaper_panel_write_bitmap()`: write monochrome or converted pixels.
- `seeed_epaper_panel_write_bitmap_gray4()`: write packed 2bpp Gray4 data.
- `seeed_epaper_panel_commit()`: drive the physical refresh.
- `seeed_epaper_panel_standby()` / `sleep()`: enter controller power states.

See [epaper_panel.h](epaper_panel.h) for signatures, formats, and mode enums.
Gray4 stores four pixels per byte, most significant pixel first, and uses a
full-frame refresh. Partial updates do not guarantee small SPI transfers;
SSD2677 can build a full-screen comparison frame internally.

For the minimal framebuffer example, detection, startup temperature and refresh
recovery policy, see `main/demo_epaper.cpp` and the [project README](../../README.md).

## Source layout

- `epaper_panel.h`: public API and formats.
- `epaper_panel.c`: shared GPIO/SPI, pixel conversion and sessions.
- `epaper_panel_priv.h`: internal backend interface.
- `driver/uc8179.c`, `driver/ssd1677.c`, `driver/ssd2677.c`: controller backends.
