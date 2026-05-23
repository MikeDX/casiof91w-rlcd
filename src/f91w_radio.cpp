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
#include <esp_sntp.h>
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
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
#endif
}

void f91w_radio_off(void)
{
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
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            return true;
        }
        radio_idle(idle, 100);
    }
    return false;
}

static void log_ntp_result(void)
{
    time_t now = time(nullptr);
    struct tm utc = {};
    struct tm local = {};
    gmtime_r(&now, &utc);
    localtime_r(&now, &local);
    Serial.printf("NTP synced UTC %02d:%02d:%02d  local %02d:%02d:%02d\n",
                  utc.tm_hour, utc.tm_min, utc.tm_sec,
                  local.tm_hour, local.tm_min, local.tm_sec);
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
    settings_apply_timezone(settings);

    /* Must not treat pre-NTP RTC wall time as "already synced". */
    struct timeval cleared = {.tv_sec = 0, .tv_usec = 0};
    settimeofday(&cleared, nullptr);

    if (!wait_ntp(ntp_timeout_ms, idle)) {
        Serial.println("NTP sync timeout");
        f91w_radio_off();
        return false;
    }

    f91w_rtc_write_from_system();
    log_ntp_result();
    settings_mark_ntp_synced();
    f91w_radio_off();
    Serial.println("WiFi off");
    return true;
}
