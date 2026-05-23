# Casio F-91W on Waveshare ESP32-S3-RLCD-4.2

Faithful F-91W LCD on the 400×300 reflective ST7305. NTP time with PCF85063 RTC fallback.

## Setup

1. Copy `include/config.h.example` to `include/config.h` and set WiFi + timezone.
2. Regenerate segments when the SVG changes (from `~/src/Casio-F-91W`):

```bash
python3 f91w_segment_converter.py \
  --input  ~/src/Casio-F-91W/demo/index.html \
  --output ~/f91w-output/ \
  --inkscape /usr/bin/inkscape \
  --threshold 100 \
  --preview

cp ~/f91w-output/f91w_segments.h lib/f91w_segments/
```

3. Build and flash:

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

The board may have a **third button** (often power/reset) — only **GPIO0** and **GPIO18** are wired for firmware on the Waveshare RLCD 4.2.

## LCD segments

Exported from the Casio HTML: all digit segments, `mode_1`/`mode_2`, colon dots, signal bell, 24H/12H marks, LAP, alarm bars. Case silkscreen text (`alarm`, `24h` label) is outside the glass crop and not exported.

Colon: **steady** in time/alarm/set (real watch); blinks only while stopwatch is running.
