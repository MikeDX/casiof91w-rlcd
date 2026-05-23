/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_heap_caps.h>
#include <string.h>

#include "f91w_display.h"
#include "f91w_power.h"
#include "f91w_radio.h"
#include "f91w_rtc.h"
#include "f91w_settings.h"
#include "f91w_watch.h"

static const uint32_t NTP_SYNC_MS = 86400000UL;
static const uint32_t WIFI_CONNECT_MS = 20000UL;
static const uint32_t NTP_WAIT_MS = 8000UL;
static const uint32_t PORTAL_TIMEOUT_MS = 180000UL;
static const uint32_t FACTORY_RESET_MS = 3000UL;
/* 80 MHz min on Waveshare OPI PSRAM — 40 MHz WDT-resets during PSRAM/SPI push */
static const uint32_t CPU_MHZ = 80;

static uint8_t *fb = nullptr;
static uint8_t *fb_last = nullptr;
static uint32_t last_ntp_check = 0;
static uint32_t last_draw = 0;
static F91WSettings wifi_settings;

static void push_display_if_changed(void)
{
    f91w_watch_draw();
    st7305_push(fb);
    if (fb_last) {
        memcpy(fb_last, fb, ST7305_FB_SIZE);
    }
    last_draw = millis();
}

static void push_display_force(void)
{
    f91w_watch_draw();
    if (fb_last) {
        memcpy(fb_last, fb, ST7305_FB_SIZE);
    }
    st7305_push(fb);
    last_draw = millis();
}

static void invalidate_display_cache(void)
{
    if (fb_last) {
        memset(fb_last, 0xFF, ST7305_FB_SIZE);
    }
}

/* Keep LCD + buttons alive during blocking WiFi waits. */
static void service_watch_ui(void)
{
    f91w_watch_update(digitalRead(18) == LOW, digitalRead(0) == LOW);
    if (millis() - last_draw >= f91w_watch_refresh_ms()) {
        push_display_if_changed();
    }
}

static bool wifi_has_ip(void)
{
    return WiFi.localIP()[0] != 0;
}

static const char *wifi_status_name(wl_status_t st)
{
    switch (st) {
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "NO_SSID";
        case WL_SCAN_COMPLETED: return "SCAN_DONE";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

static void wifi_log_status(const char *label)
{
    wl_status_t st = WiFi.status();
    Serial.printf("%s — %s (%d)", label, wifi_status_name(st), (int)st);
    if (WiFi.SSID().length() > 0) {
        Serial.printf(", ssid \"%s\"", WiFi.SSID().c_str());
    }
    if (wifi_has_ip()) {
        Serial.printf(", ip %s", WiFi.localIP().toString().c_str());
    }
    Serial.println();
}

static bool wifi_link_up(void)
{
    return WiFi.status() == WL_CONNECTED && wifi_has_ip();
}

static bool wifi_wait_connected(uint32_t timeout_ms)
{
    uint32_t start = millis();
    while (!wifi_link_up()) {
        service_watch_ui();
        if (millis() - start >= timeout_ms) {
            return false;
        }
        delay(10);
    }
    return true;
}

static bool wifi_has_saved_credentials(void)
{
    WiFiManager wm;
    return wm.getWiFiIsSaved();
}

static void wifi_begin_sta(void)
{
    WiFi.persistent(true);
    if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
        delay(50);
    }
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    WiFi.begin();
}

static bool wifi_try_saved_credentials(uint32_t timeout_ms)
{
    if (!wifi_has_saved_credentials()) {
        Serial.println("WiFi: no saved credentials");
        return false;
    }

    Serial.println("WiFi: joining saved network...");
    wifi_begin_sta();
    if (wifi_wait_connected(timeout_ms)) {
        Serial.printf("WiFi OK: %s (%s)\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
    }

    wifi_log_status("WiFi connect failed (20s)");
    Serial.println("WiFi: saved credentials did not connect");
    return false;
}

static bool both_buttons_held(void)
{
    return digitalRead(0) == LOW && digitalRead(18) == LOW;
}

static void factory_reset_if_requested(void)
{
    if (!both_buttons_held()) {
        return;
    }

    Serial.println("Hold BOOT+KEY 3s for factory reset...");
    f91w_watch_set_connect_mode(false);
    f91w_watch_set_setup_mode(true);

    uint32_t held = millis();
    while (both_buttons_held()) {
        push_display_force();
        if (millis() - held > FACTORY_RESET_MS) {
            Serial.println("Factory reset — clearing NVS and WiFi");
            settings_clear();
            delay(500);
            ESP.restart();
        }
        delay(50);
    }

    f91w_watch_set_setup_mode(false);
    f91w_watch_set_connect_mode(true);
    Serial.println("Factory reset cancelled");
}

static bool wifi_run_captive_portal(void)
{
    WiFiManagerParameter param_ntp("ntp", "NTP Server", wifi_settings.ntp_server, 64);
    WiFiManagerParameter param_tz("tz", "Timezone (POSIX string)", wifi_settings.timezone, 64);
    WiFiManagerParameter param_hint(
        "<p style='font-size:12px'>Find your timezone string at "
        "<a href='https://posix.carla.spiers.fr' target='_blank'>posix.carla.spiers.fr</a></p>");

    WiFiManager wm;
    wm.addParameter(&param_ntp);
    wm.addParameter(&param_tz);
    wm.addParameter(&param_hint);

    wm.setConfigPortalTimeout(180);
    wm.setConfigPortalBlocking(false);
    wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1),
                           IPAddress(192, 168, 4, 1),
                           IPAddress(255, 255, 255, 0));

    wm.setSaveConfigCallback([]() {
        Serial.println("Portal config saved");
    });

    wm.setAPCallback([](WiFiManager *wm) {
        (void)wm;
        f91w_watch_set_connect_mode(false);
        f91w_watch_set_setup_mode(true);
        invalidate_display_cache();
        Serial.println("Portal started — connect to WiFi AP: F91W-Setup");
    });

    wm.setConnectTimeout(WIFI_CONNECT_MS / 1000);
    wm.startConfigPortal("F91W-Setup");

    uint32_t portal_start = millis();
    while (!wifi_link_up()) {
        wm.process();
        service_watch_ui();
        if (millis() - portal_start > PORTAL_TIMEOUT_MS) {
            Serial.println("WiFi portal timeout — rebooting");
            if (wm.getConfigPortalActive()) {
                wm.stopConfigPortal();
            }
            return false;
        }
        delay(10);
    }

    /* WiFiManager auto-shuts the portal on STA connect (_disableConfigPortal). */
    if (wm.getConfigPortalActive()) {
        wm.stopConfigPortal();
    }

    strlcpy(wifi_settings.ntp_server, param_ntp.getValue(), sizeof(wifi_settings.ntp_server));
    strlcpy(wifi_settings.timezone, param_tz.getValue(), sizeof(wifi_settings.timezone));
    settings_save(wifi_settings);
    settings_apply_timezone(wifi_settings);
    settings_mark_provisioned();

    f91w_watch_set_setup_mode(false);
    invalidate_display_cache();
    push_display_force();

    Serial.printf("WiFi connected via portal: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

static bool connect_wifi(void)
{
    if (wifi_try_saved_credentials(WIFI_CONNECT_MS)) {
        settings_mark_provisioned();
        return true;
    }

    f91w_watch_set_connect_mode(false);
    Serial.println("WiFi: opening captive portal (F91W-Setup)");
    f91w_watch_set_setup_mode(true);
    invalidate_display_cache();
    push_display_force();
    return wifi_run_captive_portal();
}

static bool boot_needs_wifi(void)
{
    if (!settings_is_provisioned()) {
        return true;
    }
    return settings_ntp_sync_due();
}

static void show_clock_after_wifi(void)
{
    f91w_watch_set_connect_mode(false);
    f91w_watch_set_setup_mode(false);
    invalidate_display_cache();
    push_display_force();
}

static bool run_ntp_if_due(void)
{
    if (!settings_ntp_sync_due()) {
        Serial.println("NTP not due — RTC time");
        return true;
    }

    if (!f91w_wifi_ntp_sync(wifi_settings, WIFI_CONNECT_MS, NTP_WAIT_MS, service_watch_ui)) {
        Serial.println("NTP failed — using RTC");
        f91w_rtc_read_to_system();
        return false;
    }
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("F-91W on ESP32-S3 RLCD 4.2");

    f91w_radio_init();

    pinMode(0, INPUT_PULLUP);
    pinMode(18, INPUT_PULLUP);

    f91w_rtc_init();
    f91w_rtc_read_to_system();

    settings_load(wifi_settings);
    settings_apply_timezone(wifi_settings);

    setCpuFrequencyMhz(CPU_MHZ);
    Serial.printf("CPU %u MHz\n", CPU_MHZ);
    st7305_init();

    Serial.printf("PSRAM: %u bytes\n", (unsigned)ESP.getPsramSize());
    fb = st7305_alloc_framebuffer();
    if (!fb) {
        Serial.println("Framebuffer alloc failed");
        while (true) delay(1000);
    }
    fb_last = (uint8_t *)heap_caps_malloc(ST7305_FB_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!fb_last) {
        fb_last = (uint8_t *)malloc(ST7305_FB_SIZE);
    }
    if (fb_last) {
        memset(fb_last, 0xFF, ST7305_FB_SIZE);
    }

    f91w_watch_init(fb);

    if (boot_needs_wifi()) {
        f91w_watch_set_connect_mode(true);
    }
    push_display_force();
    Serial.println("Display started");

    factory_reset_if_requested();

    if (boot_needs_wifi()) {
        if (!connect_wifi()) {
            ESP.restart();
        }
        show_clock_after_wifi();
        run_ntp_if_due();
    } else {
        Serial.println("Skipping WiFi — NTP synced within 24h");
        f91w_radio_off();
    }

    f91w_watch_set_connect_mode(false);
    f91w_watch_set_setup_mode(false);
    last_ntp_check = millis();
    invalidate_display_cache();
    push_display_force();
    f91w_power_init();
    Serial.println("Watch ready (light sleep in clock mode)");
}

static bool radio_is_off(void)
{
    return WiFi.getMode() == WIFI_OFF;
}

void loop()
{
    uint32_t now = millis();

    if (now - last_ntp_check >= NTP_SYNC_MS && settings_ntp_sync_due()) {
        last_ntp_check = now;
        if (f91w_wifi_ntp_sync(wifi_settings, WIFI_CONNECT_MS, NTP_WAIT_MS)) {
            invalidate_display_cache();
            push_display_force();
        }
    }

    bool key_down = digitalRead(18) == LOW;
    bool boot_down = digitalRead(0) == LOW;
    f91w_watch_update(key_down, boot_down);

    uint32_t refresh_ms = f91w_watch_refresh_ms();
    if (now - last_draw >= refresh_ms) {
        push_display_if_changed();
    }

    if (f91w_watch_allows_light_sleep() && radio_is_off()) {
        if (key_down || boot_down) {
            delay(10);
        } else {
            f91w_power_sleep_until_draw(refresh_ms, last_draw);
        }
    } else {
        delay(1);
    }
}
