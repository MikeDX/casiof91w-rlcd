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
    prefs.putString("ntp_server", s.ntp_server);
    prefs.putString("timezone", s.timezone);
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
