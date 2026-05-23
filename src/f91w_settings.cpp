/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */

#include "f91w_settings.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include "f91w_rtc.h"

static const char *NS = "f91w";

void settings_load(F91WSettings &s)
{
    Preferences prefs;
    prefs.begin(NS, true);
    strlcpy(s.ntp_server, prefs.getString("ntp_server", "pool.ntp.org").c_str(), sizeof(s.ntp_server));
    strlcpy(s.timezone, prefs.getString("timezone", "GMT0BST,M3.5.0/1,M10.5.0").c_str(), sizeof(s.timezone));
    prefs.end();
}

void settings_save(const F91WSettings &s)
{
    Preferences prefs;
    prefs.begin(NS, false);
    String old_tz = prefs.getString("timezone", "");
    prefs.putString("ntp_server", s.ntp_server);
    prefs.putString("timezone", s.timezone);
    if (old_tz != s.timezone) {
        prefs.putULong("last_ntp", 0);
        Serial.println("Timezone changed — NTP resync scheduled");
    }
    prefs.end();
}

bool settings_is_provisioned(void)
{
    Preferences prefs;
    prefs.begin(NS, true);
    bool v = prefs.getBool("provisioned", false);
    prefs.end();
    return v;
}

void settings_mark_provisioned(void)
{
    Preferences prefs;
    prefs.begin(NS, false);
    prefs.putBool("provisioned", true);
    prefs.end();
}

void settings_apply_timezone(const F91WSettings &s)
{
    setenv("TZ", s.timezone, 1);
    tzset();
    configTzTime(s.timezone, s.ntp_server);
}

void settings_migration_tz_fixup(void)
{
    Preferences prefs;
    prefs.begin(NS, false);
    if (!prefs.getBool("tz_v3", false)) {
        prefs.putBool("tz_v3", true);
        prefs.putULong("last_ntp", 0);
        Serial.println("NTP sync fix — will resync from network once");
    }
    prefs.end();
}

uint32_t settings_last_ntp_epoch(void)
{
    Preferences prefs;
    prefs.begin(NS, true);
    uint32_t v = prefs.getULong("last_ntp", 0);
    prefs.end();
    return v;
}

void settings_mark_ntp_synced(void)
{
    time_t now = time(nullptr);
    if (now < 1700000000 && f91w_rtc_present()) {
        struct tm t = f91w_rtc_read();
        if (t.tm_year >= 100) {
            now = mktime(&t);
        }
    }
    if (now < 1700000000) {
        return;
    }

    Preferences prefs;
    prefs.begin(NS, false);
    prefs.putULong("last_ntp", (uint32_t)now);
    prefs.end();
}

bool settings_ntp_sync_due(void)
{
    static const uint32_t NTP_INTERVAL_SEC = 86400;

    uint32_t last = settings_last_ntp_epoch();
    if (last == 0) {
        return true;
    }

    time_t now = time(nullptr);
    if (now < 1700000000 && f91w_rtc_present()) {
        struct tm local = {};
        if (f91w_rtc_read_local_tm(&local)) {
            local.tm_isdst = -1;
            now = mktime(&local);
        }
    }
    if (now < 1700000000) {
        return true;
    }

    return (uint32_t)now - last >= NTP_INTERVAL_SEC;
}

bool settings_wifi_quick_connect(uint32_t timeout_ms)
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.begin();

    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        if (WiFi.status() == WL_CONNECTED) {
            return true;
        }
        delay(100);
    }
    return false;
}

bool settings_try_keep_existing_wifi(void)
{
    if (settings_wifi_quick_connect(5000)) {
        Serial.println("Existing WiFi credentials OK — keeping them");
        return true;
    }
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    return false;
}

void settings_wifi_erase_stale(void)
{
    /* Old firmware used WiFi.begin() — credentials persist in ESP NVS without WiFiManager. */
    WiFi.mode(WIFI_OFF);
    delay(50);
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    WiFiManager wm;
    wm.resetSettings();
    delay(100);
}

void settings_clear(void)
{
    Preferences prefs;
    prefs.begin(NS, false);
    prefs.clear();
    prefs.end();
    settings_wifi_erase_stale();
}
