# ESP12-GeigerCounter-SPI-New12864

An ESP-12 (ESP8266) driven Geiger counter display: reads pulses from a Geiger
tube module on an interrupt pin, computes counts-per-minute over three
fixed windows (20 s / 1 min / 10 min, shown left to right; the 10-minute
column reads 0 until the first 10 minutes have passed), converts to an
estimated dose rate, and shows it plus a dose-rate status on a 128x64 SPI
LCD, alongside a WiFi-synced clock. Counting starts at power-up and works
without WiFi; only the clock needs a network.

<img src="Geiger1.jpg" alt="Radioactivity Monitor" width="400"><br/>
<img src="Geiger2.jpg" alt="Radioactivity Monitor" width="400"><br/>
<img src="Geiger3.jpg" alt="Radioactivity Monitor" width="400">

## Hardware

See [Wiring.txt](Wiring.txt) for pin mappings - it covers three LCD module
variants (the "New 12864" ST7565-family SPI module this sketch targets by
default, plus notes for "Mini 12864" and "Big Blue 12864" variants) and the
button/buzzer/Geiger-pulse pin assignments.

## Setup

1. Install dependencies: `U8g2`, `WiFiManager` (Arduino Library Manager).
2. **Before flashing, replace the placeholder values** at the top of the
   .ino: `WIFI_SSIDS`/`WIFI_PASSWORDS` with your own network(s) - or better,
   enable `USE_WIFI_MANAGER` instead of hardcoding credentials at all, which
   puts up a "ESP8266-Setup" WiFi config portal on first boot.

## Dependencies

`StringHelpers`, `AlarmBeeper`, `BacklightController`, `WiFiMultiConnect`,
`BootSplashBitmap` ([source](https://github.com/bobhuang1/ESP8266-Functions-Common)),
vendored directly into this repo - re-copy from there if any of them are
updated.

## Notes

- `#define LANGUAGE_CN` / comment it out to switch the on-screen text between
  Chinese and English.
- The status line only says whether the dose *rate* is at normal background
  (below 0.5 uSv/h), above it, or at the alarm level (3.42 uSv/h and up). It
  makes no health-effect claims, and the tube conversion (`CPM_TO_USVH`) is
  uncalibrated - don't rely on this for radiation safety decisions.
- The button mutes the per-pulse clicks only. The alarm (400 ms on / 200 ms
  off at or above the alarm level) always sounds.
- GPIO0 (backlight) and GPIO2 (Geiger input) are boot-strap pins: a Geiger
  module that holds its output LOW at power-up stops the ESP-12 from booting.
  Move the input to GPIO4/5/14 if that happens.


## License

This project is free software, released under the **GNU General Public License v3.0**. You may redistribute and/or modify it under those terms; see [LICENSE.md](LICENSE.md) for the full text.
