/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */

#include "f91w_radio.h"

#include <WiFi.h>
#include <time.h>

#include "f91w_rtc.h"
#include "f91w_settings.h"

#if __has_include("esp_bt.h")
#include "esp_bt.h"
#define F91W_HAS_BT 1
#endif

void f91w_radio_init(void)
{
    WiFi.persistent(true);

#ifdef F91W_HAS_BT
    /* Free BT controller RAM/power when unused (safe if BT was never started). */
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
#endif
}

void f91w_radio_off(void)
{
    /* disconnect(..., eraseap=false) — MUST keep false or SSID/password are wiped every NTP. */
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
}

static void radio_idle(f91w_radio_idle_fn idle, uint32_t ms)
{
    if (idle) {
        idle();
        return;
    }
    delay(ms);
}

static bool wifi_has_ip(void)
{
    return WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0;
}

static bool wait_ntp(uint32_t timeout_ms, f91w_radio_idle_fn idle)
{
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        if (time(nullptr) > 1700000000) {
            return true;
        }
        radio_idle(idle, 100);
    }
    return false;
}

bool f91w_wifi_ntp_sync(const F91WSettings &settings, uint32_t connect_timeout_ms,
                        uint32_t ntp_timeout_ms, f91w_radio_idle_fn idle)
{
    if (!wifi_has_ip()) {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
        WiFi.setSleep(false);
        WiFi.begin();

        uint32_t start = millis();
        while (!wifi_has_ip() && millis() - start < connect_timeout_ms) {
            radio_idle(idle, 10);
        }
    }
    if (!wifi_has_ip()) {
        Serial.printf("WiFi connect failed for NTP (status %d)\n", (int)WiFi.status());
        f91w_radio_off();
        return false;
    }

    Serial.printf("WiFi for NTP: %s\n", WiFi.localIP().toString().c_str());
    configTzTime(settings.timezone, settings.ntp_server);

    if (!wait_ntp(ntp_timeout_ms, idle)) {
        Serial.println("NTP sync timeout");
        f91w_radio_off();
        return false;
    }

    f91w_rtc_write_from_system();
    time_t now = time(nullptr);
    Serial.printf("NTP synced: %s", ctime(&now));
    settings_mark_ntp_synced();
    f91w_radio_off();
    Serial.println("WiFi off");
    return true;
}
