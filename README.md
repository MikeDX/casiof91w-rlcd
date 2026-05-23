# Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"

**MikeDX** — [https://github.com/mikedx/casiof91w-rlcd](https://github.com/mikedx/casiof91w-rlcd)

Faithful F-91W LCD behaviour on a 400×300 reflective ST7305 panel. Time from NTP over WiFi, with PCF85063 RTC fallback. Onboard SHTC3 adds temperature and humidity screens (not on the real watch).

Copyright © 2025 MikeDX. Released under the [MIT License](LICENSE).

## Web flasher (no install)

Flash the latest firmware from your browser (Chrome or Edge, USB cable):

**[mikedx.github.io/casiof91w-rlcd](https://mikedx.github.io/casiof91w-rlcd/)**

Pushes to `main` / `master` build the firmware via GitHub Actions and update that page automatically.
After flashing, join **`F91W-Setup`** to configure Wi‑Fi (see below).

Tagged releases ([Releases](https://github.com/mikedx/casiof91w-rlcd/releases)) include firmware `.bin` files and a zip for esptool / manual flashing.

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

### Option A — Web flasher

Use the [web installer](https://mikedx.github.io/casiof91w-rlcd/) (see above).

### Option B — PlatformIO

1. Build and flash (no WiFi config file required):

```bash
pio run -t clean
pio run -t upload
pio device monitor
```

Requires `board_build.psram_type = opi` and `board_build.arduino.memory_type = qio_opi` in `platformio.ini`.

**Flashing workflow:** plug in USB (power-on), then flash **right away** — do not wait for `Watch ready`. Once the watch is running in clock mode, **light sleep** often drops the USB serial link (`Device not configured` / monitor disconnects). That is normal; the watch keeps ticking. To flash again: **unplug → plug in → upload** (or press reset once, then upload before the long sleep kicks in).

### First boot — WiFi captive portal

1. Board starts AP **`F91W-Setup`** if no saved WiFi credentials.
2. Connect your phone; the captive portal opens (or browse `http://192.168.4.1`).
3. Enter WiFi SSID/password, NTP server, and POSIX timezone ([timezone lookup](https://posix.carla.spiers.fr)).
4. Save — the board reboots, joins WiFi, and syncs time.

On-screen WiFi status:

| LCD | Meaning |
|-----|---------|
| **`CO:NN`** (blinking colon) | Joining saved Wi‑Fi and/or waiting for NTP (buttons still work) |
| **`SET`** (blinking colon) | Captive portal active — join AP **`F91W-Setup`** |

After **`Watch ready`**, the normal clock runs from the RTC. WiFi stays off until the hourly NTP resync; the portal is **not** opened again from the main loop.

**Factory reset:** hold **BOOT (GPIO0)** and **KEY (GPIO18)** together for **3 seconds** right after boot. The LCD shows **SET** while you hold; release early to cancel. This clears WiFi and all saved settings.

### Troubleshooting

**Flashed new firmware but it just shows the time — no portal?**  
Your board still has WiFi credentials in flash from an older build (when `config.h` called `WiFi.begin()`). Those are *not* in our `f91w` NVS namespace but ESP32 keeps them anyway. Reflash this build: the first boot with `provisioned` unset will erase stale WiFi and open **F91W-Setup**. Or use factory reset above.

**Factory reset shows a blank screen?**  
Hold both buttons **after** the display has started (you should see **SET** on the LCD). An older build ran reset before the display initialized; current firmware shows **SET** during the hold.

**Serial monitor:** `pio device monitor` — healthy boot: `WiFi: joining saved network...` → `WiFi OK: …` → `NTP synced:` → `WiFi off` → `Watch ready`. Saved Wi‑Fi gets **20 seconds** to connect (then captive portal). No saved credentials skip straight to **F91W-Setup**. If the port vanishes after `Watch ready`, see **Flashing workflow** above (plug in → flash; not “run for hours then flash”).

`include/config.h.example` is kept for reference only; runtime settings live in NVS.

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

## Power

After NTP sync, **WiFi and Bluetooth are turned off**; time runs from the **PCF85063 RTC**. WiFi wakes at most **once per 24 hours** (or on first boot / captive portal) to resync NTP. Between syncs the board skips WiFi entirely on boot.

Also: **80 MHz** CPU, **1 Hz** display refresh in clock mode, **light sleep** in ~80 ms chunks when WiFi is off (GPIO wake on BOOT/KEY; no sleep while a button is held), SHTC3 read only on temp/humidity screens. Typical draw in time mode is on the order of **~0.06 W** (measure on battery; USB power includes the port/charger).

Expect much lower draw than always-on WiFi (~70 mA) — measure on your supply after flashing.

**Serial over USB:** you may see boot logs through `Watch ready (light sleep in clock mode)`, then the monitor disconnects. Light sleep + USB CDC do not play nicely for an always-on debug console. Use serial only during bring-up; for day-to-day use the LCD is the UI.

Flashing a new build **keeps WiFi and watch settings** in NVS (no erase on boot). Provisioned boards skip the captive portal and use `WiFi.begin()` directly. Factory reset (BOOT+KEY 3s) clears everything.

Avoid **Erase Flash** in esptool/PIO if you want to keep settings across flashes.

After one successful boot with NTP, WiFi credentials persist in flash (we turn the radio off without erasing them). The next boot should show **`CO:NN`** briefly, then `WiFi OK:` in serial — not the portal every time.

**Stale time right after power-on?** The RTC may still hold the previous session until NTP runs (~few seconds on boot). After `Watch ready` it should match NTP.
