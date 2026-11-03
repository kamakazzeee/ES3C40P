# ES3C40P touchscreen hub

PlatformIO + Arduino firmware for the LCDWIKI ES3C40P: 4.0" 480x320 IPS (ST7796S) with capacitive
touch, ESP32-S3 (16 MB flash, 8 MB OPI PSRAM). A touch menu with Weather (Open-Meteo), SD Image Viewer,
Wi-Fi Scanner and Environment Sensor screens, a startup chime, and 5-minute sensor logging to
`/sensor_log.csv` on the SD card.

## Code layout

- `src/main.cpp`: `setup()` starts the hardware; `loop()` passes taps to the current screen.
- `include/pins.h`: every pin and I2C address.
- `src/hardware/`: one file pair per part (board, touch, battery, env_sensor, audio, sd_storage).
  Plain functions, with private state in an unnamed namespace in the `.cpp`.
- `src/net/`: `wifi_connection` and `weather_client` (fetches data, never draws).
- `src/ui/ui.*`: the shared `display` object, `drawHeader()`, `Button`, `drawButton()`,
  `drawSmoothCentre()`.
- `src/screens/`: `Screen` base class (`enter`, `draw`, `onTap`, `update`, `leave`) and one class
  per menu option, each with a global instance (`weatherScreen`, ...). `src/app.*` switches screens.
- New menu option: add a screen class, then one line in `menuItems` in `menu_screen.cpp`.
- Use one `Button` for both drawing and tap testing so they can't drift apart.
- GitHub Actions (`.github/workflows/build.yml`) compiles every push with the example secrets.

## Build, upload, monitor

- Toolchain: Arduino-ESP32 2.0.17 (ESP-IDF 4.4), compiled as C++11: no generic (`auto`) lambda
  parameters; use `std::function`. I2S uses the legacy `driver/i2s.h` API.
- `pio` lives in `%USERPROFILE%\.platformio\penv\Scripts\` (not on PATH).
- Upload: `pio run -t upload --upload-port COM3`. The board is on COM3 via the S3's native USB
  (VID 303A, PID 1001).
- A running PlatformIO Serial Monitor holds COM3 and makes uploads fail with "Access is denied";
  close it (or stop the `platformio device monitor` process) first.
- Serial is 115200 over USB CDC. Boot messages print ~0.5 s after reset, so a reader must reopen the
  port immediately after resetting (RTS pulse) to catch them.
- USB unresponsive: hold BOOT, tap RESET, release BOOT, then upload.
- `include/User_Setup.h` is the TFT_eSPI config; `scripts/patch_tft_setup.py` copies it into the
  library before every build. Edit display settings there, never in `.pio/libdeps`.
- Partition table is `default_16MB.csv` (6.25 MB app slots, 3.4 MB filesystem).

## Pin map

| Function | GPIO / address |
|---|---|
| Display SPI (80 MHz) | CS 10, DC 46, SCK 12, MOSI 11, MISO 13; backlight 45 (high = on, PWM-capable); reset = chip reset |
| Touch FT6336U, I2C 0x38 | SDA 16, SCL 15, RST 18, INT 17 |
| I2C bus (shared) | 16/15: touch 0x38, ES8311 codec 0x18, BMP280 sensor 0x76 on the I2C header |
| Audio ES8311 + I2S | MCLK 4, BCLK 5, LRCK 7, data out 8 (to codec DSDIN), data in 6 (from codec ASDOUT) |
| Speaker amp FM8002E | enable 1, LOW = on |
| microSD (SDIO 4-bit) | CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 |
| WS2812B RGB LED | 42 |
| Battery ADC | 9 (divider: volts = reading x 2) |
| BOOT button | 0 |
| Free expansion pins | 2, 3, 14, 21 |
| UART header | TX 43, RX 44 |

## Things learned the hard way

- Raw touch Y is screen X and raw touch X is (319 - screen Y) in rotation 1; `readTap()` does the
  conversion. It was once mirrored, which full-width buttons hid.
- Touch INT does not stay low while touched: it pulses low for <1 ms about every 10 ms and sends
  nothing on release. Use the FALLING-edge interrupt (`onTouchInterrupt` sets `touchDataReady`) and
  treat 40 ms without a pulse as release. Polling the pin misses most pulses.
- The ESP32-32E / E32R40T documentation pack in the vendor folder is for a different board (original
  ESP32, XPT2046 resistive touch, CH340); its pin numbers do not apply here. The ES3C40P references are
  `4.0inch_IPS_ESP32-S3_ES3C40P_User_Manual.pdf` and `ESP32s3.pdf` (pin table pp. 9-12).
- Built-in TFT_eSPI fonts have no degree sign. Smooth fonts are generated with
  `scripts/make_smooth_font.py FONT.ttf SIZE NAME [--chars ...] [--weight N]` into `include/fonts/`
  and drawn with `drawSmoothCentre()`, which loads and unloads the font around each string; a loaded
  smooth font replaces built-in font numbers, so always unload before using `drawString(..., font)`.
  Smooth text blends against a known background colour (currently always black).
- Smooth fonts cost ~7-14 ms per line; a full-screen fill is ~34 ms at 80 MHz SPI.
- The sensor on this unit is a BMP280 (no humidity), not a BME280.

## Conventions

- Keep the existing style: `constexpr` constants (pins in `include/pins.h`), camelCase, small helper
  functions, sparse comments that explain why.
- `include/wifi_secrets.h` holds real credentials and is git-ignored; never commit it.
  `include/wifi_secrets.example.h` is the template.
- Temporary test or benchmark code goes in, gets measured over serial, then is removed with
  `git restore` before anything is committed.
