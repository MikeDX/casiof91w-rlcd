#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <time.h>

#include "config.h"
#include "f91w_display.h"
#include "f91w_rtc.h"
#include "f91w_watch.h"

static uint8_t *fb = nullptr;
static uint32_t last_ntp = 0;
static uint32_t last_draw = 0;

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

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("F-91W on ESP32-S3 RLCD 4.2");

    pinMode(0, INPUT_PULLUP);   /* BOOT — Casio C: cycle modes */
    pinMode(18, INPUT_PULLUP);  /* KEY  — Casio A: action; long = L */
    /* Third physical button (if present) is usually power/reset, not a GPIO. */

    f91w_rtc_init();

    /* Bring display up before WiFi so the panel is not blank for ~20s */
    st7305_init();

    Serial.printf("PSRAM: %u bytes\n", (unsigned)ESP.getPsramSize());
    fb = st7305_alloc_framebuffer();
    if (!fb) {
        Serial.println("Framebuffer alloc failed");
        while (true) delay(1000);
    }
    f91w_watch_init(fb);
    f91w_watch_draw();
    st7305_push(fb);
    last_draw = millis();
    Serial.println("Display started");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
        configTzTime(NTP_TIMEZONE, NTP_SERVER);
        wait_ntp_sync(5000);
        f91w_rtc_write_from_system();
        last_ntp = millis();
    } else {
        Serial.println("WiFi failed — using RTC");
        f91w_rtc_read_to_system();
    }

    f91w_watch_draw();
    st7305_push(fb);
    Serial.println("Watch ready");
}

void loop()
{
    uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED && (now - last_ntp > NTP_SYNC_MS)) {
        configTzTime(NTP_TIMEZONE, NTP_SERVER);
        wait_ntp_sync(3000);
        f91w_rtc_write_from_system();
        last_ntp = now;
    }

    bool key = digitalRead(18) == LOW;
    bool boot = digitalRead(0) == LOW;
    f91w_watch_update(key, boot);

    if (now - last_draw >= CLOCK_REFRESH_MS) {
        f91w_watch_draw();
        st7305_push(fb);
        last_draw = now;
    }
}
