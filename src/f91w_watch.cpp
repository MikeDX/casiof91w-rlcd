/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */

#include "f91w_watch.h"

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <sys/time.h>
#include <time.h>

#include "f91w_rtc.h"
#include "f91w_segments.h"

enum F91WMenu {
    MENU_TIME,
    MENU_ALARM,
    MENU_STOPWATCH,
    MENU_SET,
    MENU_SENSOR_TEMP,
    MENU_SENSOR_HUM,
};

enum AlarmAction { ALARM_VIEW, ALARM_EDIT_HR, ALARM_EDIT_MIN };
enum SetAction { SET_SEC, SET_MIN, SET_HR, SET_MONTH, SET_DAY };

static uint8_t *fb = nullptr;
static F91WMenu menu = MENU_TIME;
static AlarmAction alarm_action = ALARM_VIEW;
static SetAction set_action = SET_SEC;
static bool show_temp_overlay = false;
static bool use24h = true;
static bool alarm_on = false;
static bool signal_on = false;
static bool lap_on = false;
static int alarm_h = 7;
static int alarm_m = 0;

static bool sw_running = false;
static uint32_t sw_start_ms = 0;
static uint32_t sw_elapsed_ms = 0;
static uint32_t sw_lap_ms = 0;
static bool sw_has_lap = false;

static bool key_prev = false;
static bool boot_prev = false;
static uint32_t key_down_ms = 0;
static uint32_t boot_down_ms = 0;
static bool key_long_fired = false;
static bool boot_long_fired = false;
static uint32_t key_repeat_ms = 0;
static bool key_repeat_active = false;

static float desk_temp_c = 0.0f;
static float desk_humidity_rh = 0.0f;
static uint32_t last_temp_ms = 0;
static Preferences prefs;
static bool setup_mode = false;
static bool connect_mode = false;

static void save_alarm_nvs(void);
static void load_alarm_nvs(void);

static const uint8_t DIGITS[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111,
};

static const F91WSeg *const MODE1_SEG[8] = {
    &F91W_MODE_1_A, &F91W_MODE_1_B, &F91W_MODE_1_C, &F91W_MODE_1_D,
    &F91W_MODE_1_E, &F91W_MODE_1_F, &F91W_MODE_1_G, &F91W_MODE_1_H,
};

static const F91WSeg *const MODE2_SEG[9] = {
    &F91W_MODE_2_A, &F91W_MODE_2_B, &F91W_MODE_2_C, &F91W_MODE_2_D,
    &F91W_MODE_2_E, &F91W_MODE_2_F, &F91W_MODE_2_G, &F91W_MODE_2_H,
    &F91W_MODE_2_I,
};

static void draw_mode1_segments(const char *segs)
{
    for (int i = 0; segs[i]; i++) {
        char c = segs[i];
        if (c >= 'A' && c <= 'H') {
            f91w_on(fb, MODE1_SEG[c - 'A']);
        }
    }
}

/* PCF85063 is the wall clock — always use it when present (system time stalls after WiFi.off). */
static void watch_get_local_tm(struct tm *t)
{
    if (f91w_rtc_present()) {
        struct tm rtc = f91w_rtc_read();
        if (rtc.tm_year >= 100) {
            *t = rtc;
            return;
        }
    }
    time_t epoch = time(nullptr);
    if (!localtime_r(&epoch, t)) {
        memset(t, 0, sizeof(t));
        t->tm_hour = 12;
    }
}

static bool blinking_visible(void)
{
    int ms = (int)(millis() % 1000);
    if (ms < 250) return false;
    if (ms < 500) return true;
    if (ms < 750) return false;
    return true;
}

static void draw_digit(int fw_position, uint8_t digit)
{
    uint8_t mask = DIGITS[digit % 10];
    for (int seg = 0; seg < 7; seg++) {
        if ((mask >> seg) & 1) {
            const F91WSeg *s = F91W_DIGIT[fw_position][seg];
            if (s) f91w_on(fb, s);
        }
    }
}

static void draw_mode2_segments(const char *segs)
{
    for (int i = 0; segs[i]; i++) {
        char c = segs[i];
        if (c >= 'A' && c <= 'I') {
            f91w_on(fb, MODE2_SEG[c - 'A']);
        }
    }
}

/* 8-segment mode_1 charset (CasioF91WDigitalDisplay.js) */
static void draw_char_mode1(char ch)
{
    switch (ch) {
        case '0': draw_mode1_segments("ABCDEF"); break;
        case '1': draw_mode1_segments("BC"); break;
        case '2': draw_mode1_segments("ABDEG"); break;
        case '3': draw_mode1_segments("ABCDG"); break;
        case '4': draw_mode1_segments("BCFG"); break;
        case '5': draw_mode1_segments("ACDFG"); break;
        case '6': draw_mode1_segments("ACDEFG"); break;
        case '7': draw_mode1_segments("ABC"); break;
        case '8': draw_mode1_segments("ABCDEFG"); break;
        case '9': draw_mode1_segments("ABCDFG"); break;
        case 'A': draw_mode1_segments("ABCEFG"); break;
        case 'C': draw_mode1_segments("ADEF"); break;
        case 'E': draw_mode1_segments("ADEFG"); break;
        case 'F': draw_mode1_segments("AEFG"); break;
        case 'H': draw_mode1_segments("BCEFG"); break;
        case 'I': draw_mode1_segments("BC"); break;
        case 'L': draw_mode1_segments("DEF"); break;
        case 'O': draw_mode1_segments("ABCDEF"); break;
        case 'S': draw_mode1_segments("ACDFG"); break;
        case 'T': draw_mode1_segments("AEFH"); break;
        case 'U': draw_mode1_segments("BCDEF"); break;
        case 'R': draw_mode1_segments("ABCEFGH"); break;
        case ' ': break;
        default: break;
    }
}

static void draw_char_mode2(char ch)
{
    switch (ch) {
        case '0': draw_mode2_segments("ABCDEF"); break;
        case '1': draw_mode2_segments("BC"); break;
        case '2': draw_mode2_segments("ABDEG"); break;
        case '3': draw_mode2_segments("ABCDG"); break;
        case '4': draw_mode2_segments("BCFG"); break;
        case '5': draw_mode2_segments("ACDFG"); break;
        case '6': draw_mode2_segments("ACDEFG"); break;
        case '7': draw_mode2_segments("ABC"); break;
        case '8': draw_mode2_segments("ABCDEFG"); break;
        case '9': draw_mode2_segments("ABCDFG"); break;
        case 'A': draw_mode2_segments("ABCEFG"); break;
        case 'C': draw_mode2_segments("ADEF"); break;
        case 'E': draw_mode2_segments("ADEFG"); break;
        case 'F': draw_mode2_segments("AEFG"); break;
        case 'H': draw_mode2_segments("BCEFG"); break;
        case 'I': draw_mode2_segments("BC"); break;
        case 'L': draw_mode2_segments("DEF"); break;
        case 'M': draw_mode2_segments("ABCEFHI"); break;
        case 'O': draw_mode2_segments("ABCDEF"); break;
        case 'S': draw_mode2_segments("ACDFG"); break;
        case 'T': draw_mode2_segments("AHI"); break;
        case 'U': draw_mode2_segments("BCDEF"); break;
        case 'W': draw_mode2_segments("BCDEFHI"); break;
        case 'R': draw_mode2_segments("ABCEFGH"); break;
        case ' ': break;
        default: break;
    }
}

static void draw_char_7seg(int fw_position, const char *segs)
{
    for (int i = 0; segs[i]; i++) {
        char c = segs[i];
        if (c < 'A' || c > 'G') continue;
        const F91WSeg *s = F91W_DIGIT[fw_position][c - 'A'];
        if (s) f91w_on(fb, s);
    }
}

static void draw_char_pos(int pos, char ch)
{
    if (ch >= '0' && ch <= '9') {
        draw_digit(pos, (uint8_t)(ch - '0'));
        return;
    }
    if (pos == 0) {
        draw_char_mode1(ch);
        return;
    }
    if (pos == 1) {
        draw_char_mode2(ch);
        return;
    }
    switch (ch) {
        case 'A': draw_char_7seg(pos, "ABCEFG"); break;
        case 'C': draw_char_7seg(pos, "ADEF"); break;
        case 'E': draw_char_7seg(pos, "ADEFG"); break;
        case 'F': draw_char_7seg(pos, "AEFG"); break;
        case 'H': draw_char_7seg(pos, "BCEFG"); break;
        case 'I': draw_char_7seg(pos, "BC"); break;
        case 'L': draw_char_7seg(pos, "DEF"); break;
        case 'N': draw_char_7seg(pos, "ABCEF"); break;
        case 'O': draw_char_7seg(pos, "ABCDEF"); break;
        case 'S': draw_char_7seg(pos, "ACDFG"); break;
        case 'U': draw_char_7seg(pos, "BCDEF"); break;
        case 'T': draw_char_7seg(pos, "DEF"); break;
        default: break;
    }
}

static void weekday_letters(struct tm *t, char *a, char *b)
{
    static const char *names[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    int w = t->tm_wday;
    if (w < 0 || w > 6) w = 0;
    *a = names[w][0];
    *b = names[w][1];
}

/* pos_ones = *_1 (right), pos_tens = *_2 (left) — matches Casio SVG naming */
static void draw_two_digits(int pos_ones, int pos_tens, int value, bool blank_leading)
{
    int tens = value / 10;
    int ones = value % 10;
    if (tens > 0 || !blank_leading) {
        draw_digit(pos_tens, (uint8_t)tens);
    }
    draw_digit(pos_ones, (uint8_t)ones);
}

static void draw_colon(bool on)
{
    if (on) {
        f91w_on(fb, &F91W_DOT_TOP);
        f91w_on(fb, &F91W_DOT_BOTTOM);
    }
}

static void draw_indicators(bool bell, bool signal, bool h24, bool lap)
{
    if (signal && F91W_IND[0]) f91w_on(fb, F91W_IND[0]);
    if (bell && F91W_IND[1]) f91w_on(fb, F91W_IND[1]);
    if (h24 && F91W_IND[3]) f91w_on(fb, F91W_IND[3]);
    if (!h24 && F91W_IND[2]) f91w_on(fb, F91W_IND[2]);
    if (lap && F91W_IND[4]) f91w_on(fb, F91W_IND[4]);
}

/* Real F-91W: colon is steady in time/alarm; only blinks in running stopwatch */
static bool colon_lit(void)
{
    if (menu == MENU_STOPWATCH && sw_running) {
        return (millis() / 500) & 1;
    }
    return menu == MENU_TIME || menu == MENU_ALARM || menu == MENU_SET ||
           menu == MENU_STOPWATCH; /* steady when stopped; blinks only while running */
}

static uint32_t stopwatch_display_ms(void)
{
    if (sw_has_lap) return sw_lap_ms;
    if (sw_running) return sw_elapsed_ms + (millis() - sw_start_ms);
    return sw_elapsed_ms;
}

static void shtc3_read_temp(void)
{
    static constexpr uint8_t SHTC3_ADDR = 0x70;
    static constexpr uint16_t MEAS_T_RH_POLLING = 0x7CA2;

    Wire.beginTransmission(SHTC3_ADDR);
    Wire.write((uint8_t)(MEAS_T_RH_POLLING >> 8));
    Wire.write((uint8_t)(MEAS_T_RH_POLLING & 0xFF));
    if (Wire.endTransmission() != 0) return;

    delay(15);

    uint8_t raw[6] = {};
    if (Wire.requestFrom((int)SHTC3_ADDR, 6) != 6) return;

    for (int i = 0; i < 6; i++) raw[i] = Wire.read();
    uint16_t traw = ((uint16_t)raw[0] << 8) | raw[1];
    uint16_t hraw = ((uint16_t)raw[3] << 8) | raw[4];
    desk_temp_c = 175.0f * (float)traw / 65536.0f - 45.0f;
    desk_humidity_rh = 100.0f * (float)hraw / 65536.0f;
}

static void cycle_menu(void)
{
    switch (menu) {
        case MENU_TIME:       menu = MENU_ALARM; alarm_action = ALARM_VIEW; break;
        case MENU_ALARM:      menu = MENU_STOPWATCH; break;
        case MENU_STOPWATCH:  menu = MENU_SET; set_action = SET_SEC; break;
        case MENU_SET:            menu = MENU_SENSOR_TEMP; break;
        case MENU_SENSOR_TEMP:    menu = MENU_SENSOR_HUM; break;
        case MENU_SENSOR_HUM:     menu = MENU_TIME; break;
    }
    show_temp_overlay = false;
}

static int clamp99(int v)
{
    if (v < 0) return 0;
    if (v > 99) return 99;
    return v;
}

/* TE + XXc (temperature in main digits, c in minutes_1) */
static void draw_sensor_temp_screen(void)
{
    int temp = clamp99((int)(desk_temp_c + 0.5f));
    draw_char_pos(1, 'T');
    draw_char_pos(0, 'E');
    draw_two_digits(4, 5, temp, false);
    draw_char_pos(6, 'C'); /* °C suffix */
    draw_colon(false);
}

/* HU + XX (%RH in main digits) */
static void draw_sensor_hum_screen(void)
{
    int rh = clamp99((int)(desk_humidity_rh + 0.5f));
    draw_char_pos(1, 'H');
    draw_char_pos(0, 'U');
    draw_two_digits(4, 5, rh, false);
    draw_colon(false);
}

static void stopwatch_start_stop(void)
{
    if (sw_running) {
        sw_elapsed_ms += millis() - sw_start_ms;
        sw_running = false;
    } else {
        sw_start_ms = millis();
        sw_running = true;
        sw_has_lap = false;
        lap_on = false;
    }
}

static void adjust_system_time(int field, int delta)
{
    time_t epoch = time(nullptr);
    struct tm t;
    if (!localtime_r(&epoch, &t)) return;
    switch (field) {
        case SET_SEC:   t.tm_sec = (t.tm_sec + delta) % 60; break;
        case SET_MIN:   t.tm_min = (t.tm_min + delta) % 60; break;
        case SET_HR:    t.tm_hour = (t.tm_hour + delta) % 24; break;
        case SET_MONTH: t.tm_mon = (t.tm_mon + delta) % 12; break;
        case SET_DAY:   t.tm_mday = (t.tm_mday + delta - 1) % 31 + 1; break;
        default: break;
    }
    epoch = mktime(&t);
    if (epoch >= 0) {
        struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
        settimeofday(&tv, nullptr);
    }
}

static void key_increment(void)
{
    if (menu == MENU_ALARM) {
        if (alarm_action == ALARM_EDIT_HR) {
            alarm_h = (alarm_h + 1) % 24;
            save_alarm_nvs();
        } else if (alarm_action == ALARM_EDIT_MIN) {
            alarm_m = (alarm_m + 1) % 60;
            save_alarm_nvs();
        }
    } else if (menu == MENU_SET) {
        adjust_system_time(set_action, 1);
    }
}

static void key_advance_field(void)
{
    if (menu == MENU_ALARM) {
        if (alarm_action == ALARM_VIEW) alarm_action = ALARM_EDIT_HR;
        else if (alarm_action == ALARM_EDIT_HR) alarm_action = ALARM_EDIT_MIN;
        else alarm_action = ALARM_VIEW;
    } else if (menu == MENU_SET) {
        if (set_action == SET_SEC) set_action = SET_MIN;
        else if (set_action == SET_MIN) set_action = SET_HR;
        else if (set_action == SET_HR) set_action = SET_MONTH;
        else if (set_action == SET_MONTH) set_action = SET_DAY;
        else set_action = SET_SEC;
    }
}

static void save_alarm_nvs(void)
{
    prefs.begin("f91w", false);
    prefs.putInt("alarm_h", alarm_h);
    prefs.putInt("alarm_m", alarm_m);
    prefs.putBool("alarm_on", alarm_on);
    prefs.putBool("signal_on", signal_on);
    prefs.putBool("use24h", use24h);
    prefs.end();
}

static void load_alarm_nvs(void)
{
    prefs.begin("f91w", true);
    alarm_h = prefs.getInt("alarm_h", 7);
    alarm_m = prefs.getInt("alarm_m", 0);
    alarm_on = prefs.getBool("alarm_on", false);
    signal_on = prefs.getBool("signal_on", false);
    use24h = prefs.getBool("use24h", true);
    prefs.end();
}

/* Casio A — short release */
static void on_key_release(void)
{
    if (menu == MENU_TIME) {
        use24h = !use24h;
        save_alarm_nvs();
        return;
    }
    if (menu == MENU_ALARM && alarm_action == ALARM_VIEW) {
        if (alarm_on && signal_on) {
            alarm_on = false;
            signal_on = false;
        } else if (alarm_on) {
            alarm_on = false;
            signal_on = true;
        } else if (signal_on) {
            alarm_on = true;
            signal_on = true;
        } else {
            alarm_on = true;
            signal_on = false;
        }
        save_alarm_nvs();
    }
}

/* Casio L — long KEY (no physical L on Waveshare) */
static void on_key_long(void)
{
    if (menu == MENU_STOPWATCH) {
        if (sw_running) {
            if (sw_has_lap) {
                sw_has_lap = false;
                lap_on = false;
            } else {
                sw_lap_ms = stopwatch_display_ms();
                sw_has_lap = true;
                lap_on = true;
            }
        } else if (sw_has_lap) {
            sw_has_lap = false;
            lap_on = false;
        } else {
            sw_elapsed_ms = 0;
            sw_lap_ms = 0;
        }
        return;
    }
    if (menu == MENU_TIME) {
        show_temp_overlay = true;
        return;
    }
    key_advance_field();
}

/* Casio C — BOOT always cycles modes (including out of stopwatch) */
static void on_boot_release(void)
{
    cycle_menu();
}

void f91w_watch_init(uint8_t *framebuffer)
{
    fb = framebuffer;
    f91w_clear(fb);
    load_alarm_nvs();
    last_temp_ms = 0;
}

void f91w_watch_set_setup_mode(bool active)
{
    setup_mode = active;
}

void f91w_watch_set_connect_mode(bool active)
{
    connect_mode = active;
}

bool f91w_watch_allows_light_sleep(void)
{
    if (setup_mode || connect_mode) {
        return false;
    }
    return f91w_watch_refresh_ms() >= 1000;
}

uint32_t f91w_watch_refresh_ms(void)
{
    if (setup_mode || connect_mode) {
        return 100;
    }
    if (menu == MENU_STOPWATCH && sw_running) {
        return 50;
    }
    if (menu == MENU_STOPWATCH) {
        return 200;
    }
    if (menu == MENU_ALARM && alarm_action != ALARM_VIEW) {
        return 100;
    }
    if (menu == MENU_SET) {
        return 100;
    }
    if (menu == MENU_SENSOR_TEMP || menu == MENU_SENSOR_HUM) {
        return 5000;
    }
    if (show_temp_overlay) {
        return 1000;
    }
    return 1000;
}

uint8_t *f91w_watch_framebuffer(void)
{
    return fb;
}

void f91w_watch_update(bool key_down, bool boot_down)
{
    uint32_t now = millis();

    if (key_down && !key_prev) {
        key_down_ms = now;
        key_long_fired = false;
        key_repeat_active = false;
        if (menu == MENU_STOPWATCH) {
            stopwatch_start_stop(); /* Casio A: start/stop on press */
        } else if (menu == MENU_ALARM && alarm_action != ALARM_VIEW) {
            key_increment();
        } else if (menu == MENU_SET) {
            key_increment();
        }
    }
    if (key_down && !key_long_fired && (now - key_down_ms > 2000)) {
        key_long_fired = true;
        key_repeat_active = false;
        on_key_long();
    }
    if (key_down && key_repeat_active && (now - key_repeat_ms > 100)) {
        key_repeat_ms = now;
        key_increment();
    }
    if (key_down && !key_long_fired && !key_repeat_active &&
        (now - key_down_ms > 1000)) {
        key_repeat_active = true;
        key_repeat_ms = now;
    }
    if (!key_down && key_prev) {
        if (!key_long_fired) on_key_release();
        key_repeat_active = false;
        if (menu == MENU_TIME) show_temp_overlay = false;
    }
    key_prev = key_down;

    if (boot_down && !boot_prev) {
        boot_down_ms = now;
        boot_long_fired = false;
    }
    if (!boot_down && boot_prev && !boot_long_fired) {
        on_boot_release();
    }
    boot_prev = boot_down;

    if ((menu == MENU_SENSOR_TEMP || menu == MENU_SENSOR_HUM || show_temp_overlay) &&
        (now - last_temp_ms > 10000)) {
        shtc3_read_temp();
        last_temp_ms = now;
    }

    struct tm t;
    watch_get_local_tm(&t);
    if (alarm_on && t.tm_hour == alarm_h && t.tm_min == alarm_m && t.tm_sec < 2) {
        Serial.println("ALARM");
    }
}

void f91w_watch_draw(void)
{
    if (!fb) return;

    f91w_clear(fb);

    if (setup_mode) {
        draw_char_pos(5, 'S');
        draw_char_pos(4, 'E');
        draw_char_pos(6, 'T');
        draw_colon((millis() / 500) % 2);
        draw_indicators(false, false, false, false);
        return;
    }

    if (connect_mode) {
        draw_char_pos(5, 'C');
        draw_char_pos(4, 'O');
        draw_colon((millis() / 500) % 2);
        draw_char_pos(6, 'N');
        draw_char_pos(7, 'N');
        draw_indicators(false, false, false, false);
        return;
    }

    struct tm t;
    watch_get_local_tm(&t);

    bool blink = blinking_visible();
    bool colon_on = colon_lit();

    char w0 = ' ', w1 = ' ';
    weekday_letters(&t, &w0, &w1);

    if (show_temp_overlay) {
        draw_sensor_temp_screen();
        draw_indicators(alarm_on, signal_on, false, false);
    } else if (menu == MENU_TIME) {
        int hour = t.tm_hour;
        if (!use24h && hour > 12) hour -= 12;
        if (!use24h && hour == 0) hour = 12;

        draw_char_pos(1, w0);
        draw_char_pos(0, w1);
        draw_two_digits(2, 3, t.tm_mday, true);
        draw_two_digits(4, 5, hour, true);
        draw_two_digits(6, 7, t.tm_min, false);
        draw_two_digits(8, 9, t.tm_sec, false);
        draw_colon(colon_on);
        draw_indicators(alarm_on, signal_on, use24h, false);
    } else if (menu == MENU_ALARM) {
        draw_char_pos(1, 'A');
        draw_char_pos(0, 'L');
        bool blink_hr = alarm_action == ALARM_EDIT_HR && !blink;
        bool blink_min = alarm_action == ALARM_EDIT_MIN && !blink;
        if (!blink_hr) draw_two_digits(4, 5, alarm_h, true);
        if (!blink_min) draw_two_digits(6, 7, alarm_m, false);
        draw_colon(colon_on);
        draw_indicators(alarm_on, signal_on, false, false);
    } else if (menu == MENU_STOPWATCH) {
        uint32_t ms = stopwatch_display_ms();
        uint32_t total_sec = ms / 1000;
        int minutes = (int)(total_sec / 60);
        int seconds = (int)(total_sec % 60);
        int centis = (int)((ms % 1000) / 10);

        draw_char_pos(1, 'S');
        draw_char_pos(0, 'T');
        draw_two_digits(4, 5, minutes, true);
        draw_two_digits(6, 7, seconds, false);
        draw_digit(9, (uint8_t)(centis / 10));
        draw_digit(8, (uint8_t)(centis % 10));
        draw_colon(colon_on);
        draw_indicators(alarm_on, signal_on, false, lap_on);
    } else if (menu == MENU_SET) {
        int hour = t.tm_hour;
        if (!use24h && hour > 12) hour -= 12;
        if (!use24h && hour == 0) hour = 12;

        draw_char_pos(1, w0);
        draw_char_pos(0, w1);
        draw_two_digits(2, 3, t.tm_mday, true);

        if (set_action == SET_MONTH || set_action == SET_DAY) {
            bool blink_mo = set_action == SET_MONTH && !blink;
            bool blink_day = set_action == SET_DAY && !blink;
            if (!blink_mo) draw_two_digits(4, 5, t.tm_mon + 1, true);
            if (!blink_day) draw_two_digits(2, 3, t.tm_mday, true);
            draw_colon(false);
        } else {
            bool blink_sec = set_action == SET_SEC && !blink;
            bool blink_min = set_action == SET_MIN && !blink;
            bool blink_hr = set_action == SET_HR && !blink;
            if (!blink_hr) draw_two_digits(4, 5, hour, true);
            if (!blink_min) draw_two_digits(6, 7, t.tm_min, false);
            if (!blink_sec) draw_two_digits(8, 9, t.tm_sec, false);
            draw_colon(colon_on);
        }
        draw_indicators(alarm_on, signal_on, use24h, false);
    } else if (menu == MENU_SENSOR_TEMP) {
        draw_sensor_temp_screen();
        draw_indicators(false, false, false, false);
    } else if (menu == MENU_SENSOR_HUM) {
        draw_sensor_hum_screen();
        draw_indicators(false, false, false, false);
    }
}
