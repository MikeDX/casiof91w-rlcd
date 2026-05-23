# Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"

**MikeDX** — [https://github.com/mikedx/casiof91w-rlcd](https://github.com/mikedx/casiof91w-rlcd)

Faithful F-91W LCD behaviour on a 400×300 reflective ST7305 panel. Time from NTP over WiFi, with PCF85063 RTC fallback. Onboard SHTC3 adds temperature and humidity screens (not on the real watch).

Copyright © 2025 MikeDX. Released under the [MIT License](LICENSE).

## Hardware

| Item | Details |
|------|---------|
| **Board** | Waveshare **ESP32-S3 AI RLCD Total Reflection Screen 4.2 inch** development board |
| **Product link** | [AliExpress listing](https://www.aliexpress.com/item/1005010730699439.html) |
| **MCU** | ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB OPI PSRAM) |
| **Display** | ST7305 reflective LCD, 400×300, 1-bit monochrome |
| **Sensors** | SHTC3 (temp/humidity), PCF85063 (RTC) on I2C |

## Inspiration (not a fork)

Watch modes, button logic, and LCD layout follow the excellent browser simulation
**[alexisphilip/Casio-F-91W](https://github.com/alexisphilip/Casio-F-91W)** as a
**behavioural reference only**. **No JavaScript or emulator code from that repo is
included here** — this firmware is original C++ for the ESP32.

Pre-built segment bitmaps ship in `lib/f91w_segments/`. To recreate them from
that project’s SVG, use the optional tool in
[`tools/segment_converter/`](tools/segment_converter/README.md).

Casio and F-91W are trademarks of Casio Computer Co., Ltd. This is an independent hobby project and is not affiliated with or endorsed by Casio.

## Setup

1. Copy `include/config.h.example` to `include/config.h` and set WiFi + timezone.
2. Build and flash:

```bash
pio run -t clean
pio run -t upload
pio device monitor
```

Requires `board_build.psram_type = opi` and `board_build.arduino.memory_type = qio_opi` in `platformio.ini`.

## Buttons (Waveshare → Casio)

| Pin | Casio | Action |
|-----|-------|--------|
| GPIO0 BOOT | **C** | **Always** cycle modes: Time → Alarm → Stopwatch → Set → **TE/°C** → **HU/%** → Time |
| GPIO18 KEY | **A** | Stopwatch: start/stop. Time: toggle 12/24h. Alarm: cycle bell/signal. Set/alarm edit: +1 (hold 1s = fast) |
| GPIO18 KEY long (2s) | **L** | Lap/reset in stopwatch; advance field in alarm/set; hold in time = quick temp peek |

The board may have a **third button** (often power/reset) — only **GPIO0** and **GPIO18** are wired for firmware on this Waveshare RLCD 4.2 board.

## LCD segments

Digit segments, `mode_1`/`mode_2`, colon dots, signal bell, 24H/12H marks, LAP, and alarm bars are rasterised into `lib/f91w_segments/f91w_segments.h` (checked into the repo).

Colon: **steady** in time/alarm/set (real watch); blinks only while stopwatch is running.

24H/12H indicators: shown in **time** and **set** only (matches real F-91W behaviour).
