# Changelog

## [1.0.0] — 2025-05-23

First public release.

### Features

- Casio F-91W behaviour on Waveshare ESP32-S3 RLCD 4.2″ (400×300 ST7305)
- Time, alarm, stopwatch, set, temperature and humidity screens
- NTP over WiFi with PCF85063 RTC fallback
- WiFi captive portal (`F91W-Setup`) — no `config.h` required
- Factory reset: hold BOOT + KEY for 3 seconds on boot
- [Web flasher](https://mikedx.github.io/casiof91w-rlcd/) via ESP Web Tools (GitHub Pages)

### Hardware

- Waveshare ESP32-S3 AI RLCD 4.2″ development board
- Onboard SHTC3 (temp/humidity), PCF85063 (RTC)

[1.0.0]: https://github.com/mikedx/casiof91w-rlcd/releases/tag/v1.0.0
