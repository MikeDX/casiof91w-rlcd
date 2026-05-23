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
#include <time.h>

#include "f91w_display.h"
#include "f91w_rtc.h"
#include "f91w_settings.h"
#include "f91w_watch.h"

static const uint32_t NTP_SYNC_MS = 3600000UL;
static const uint32_t CLOCK_REFRESH_MS = 100UL;
static const uint32_t PORTAL_TIMEOUT_MS = 180000UL;
static const uint32_t FACTORY_RESET_MS = 3000UL;

static uint8_t *fb = nullptr;
static uint32_t last_ntp = 0;
static uint32_t last_draw = 0;
static F91WSettings wifi_settings;
static bool portal_saved = false;

static void wait_ntp_sync(uint32_t timeout_ms)
{
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        time_t now = time(nullptr);
        if (now > 1700000000) {
            Serial.printf("NTP synced: %s", ctime(&now));
            return;
        }
        delay(100);
    }
    Serial.println("NTP sync timeout");
}

static void push_display(void)
{
    f91w_watch_draw();
    st7305_push(fb);
    last_draw = millis();
}

static bool both_buttons_held(void)
{
    return digitalRead(0) == LOW && digitalRead(18) == LOW;
}

/* Display must be up — shows SET + blinking colon while holding both buttons. */
static void factory_reset_if_requested(void)
{
    if (!both_buttons_held()) {
        return;
    }

    Serial.println("Hold BOOT+KEY 3s for factory reset...");
    f91w_watch_set_setup_mode(true);

    uint32_t held = millis();
    while (both_buttons_held()) {
        push_display();
        uint32_t elapsed = millis() - held;
        if (elapsed > FACTORY_RESET_MS) {
            Serial.println("Factory reset — clearing NVS and WiFi");
            settings_clear();
            delay(500);
            ESP.restart();
        }
        delay(50);
    }

    f91w_watch_set_setup_mode(false);
    Serial.println("Factory reset cancelled");
}

static bool connect_wifi_portal(void)
{
    settings_load(wifi_settings);

    if (!settings_is_provisioned()) {
        Serial.println("Not provisioned — erasing stale WiFi from flash");
        settings_wifi_erase_stale();
    }

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
        portal_saved = true;
        Serial.println("Portal config saved");
    });

    wm.setAPCallback([](WiFiManager *wm) {
        (void)wm;
        f91w_watch_set_setup_mode(true);
        Serial.println("Portal started — connect to WiFi AP: F91W-Setup");
    });

    portal_saved = false;
    f91w_watch_set_setup_mode(true);
    push_display();

    /* Non-blocking autoConnect returns false when it opens the portal — not an error. */
    wm.autoConnect("F91W-Setup");

    uint32_t portal_start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        wm.process();
        if (millis() - last_draw >= CLOCK_REFRESH_MS) {
            push_display();
        }
        if (millis() - portal_start > PORTAL_TIMEOUT_MS) {
            Serial.println("WiFi timeout — rebooting");
            return false;
        }
        delay(10);
    }

    f91w_watch_set_setup_mode(false);

    strlcpy(wifi_settings.ntp_server, param_ntp.getValue(), sizeof(wifi_settings.ntp_server));
    strlcpy(wifi_settings.timezone, param_tz.getValue(), sizeof(wifi_settings.timezone));
    settings_save(wifi_settings);

    if (portal_saved) {
        settings_mark_provisioned();
    }

    Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("F-91W on ESP32-S3 RLCD 4.2");

    pinMode(0, INPUT_PULLUP);   /* BOOT — Casio C: cycle modes */
    pinMode(18, INPUT_PULLUP);  /* KEY  — Casio A: action; long = L */

    f91w_rtc_init();
    st7305_init();

    Serial.printf("PSRAM: %u bytes\n", (unsigned)ESP.getPsramSize());
    fb = st7305_alloc_framebuffer();
    if (!fb) {
        Serial.println("Framebuffer alloc failed");
        while (true) delay(1000);
    }
    f91w_watch_init(fb);
    push_display();
    Serial.println("Display started");

    /* After display is live — SET on LCD while holding both buttons 3s */
    factory_reset_if_requested();

    if (!connect_wifi_portal()) {
        ESP.restart();
    }

    configTzTime(wifi_settings.timezone, wifi_settings.ntp_server);
    wait_ntp_sync(5000);
    f91w_rtc_write_from_system();
    last_ntp = millis();

    push_display();
    Serial.println("Watch ready");
}

void loop()
{
    uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED && (now - last_ntp > NTP_SYNC_MS)) {
        configTzTime(wifi_settings.timezone, wifi_settings.ntp_server);
        wait_ntp_sync(3000);
        f91w_rtc_write_from_system();
        last_ntp = now;
    }

    bool key = digitalRead(18) == LOW;
    bool boot = digitalRead(0) == LOW;
    f91w_watch_update(key, boot);

    if (now - last_draw >= CLOCK_REFRESH_MS) {
        push_display();
    }
}
