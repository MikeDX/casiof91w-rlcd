/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */

#include "f91w_power.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

static constexpr gpio_num_t PIN_BOOT = GPIO_NUM_0;
static constexpr gpio_num_t PIN_KEY  = GPIO_NUM_18;
static constexpr uint32_t MIN_SLEEP_MS = 5;
/* Short chunks: ~10 polls/s for buttons; still mostly asleep vs no sleep */
static constexpr uint32_t MAX_SLEEP_CHUNK_MS = 80;

static bool gpio_sleep_ready = false;

void f91w_power_init(void)
{
    gpio_sleep_set_direction(PIN_BOOT, GPIO_MODE_INPUT);
    gpio_sleep_set_pull_mode(PIN_BOOT, GPIO_PULLUP_ONLY);
    gpio_sleep_set_direction(PIN_KEY, GPIO_MODE_INPUT);
    gpio_sleep_set_pull_mode(PIN_KEY, GPIO_PULLUP_ONLY);
    gpio_sleep_ready = true;
}

void f91w_power_light_sleep_ms(uint32_t ms)
{
    if (ms < MIN_SLEEP_MS) {
        return;
    }

    if (!gpio_sleep_ready) {
        f91w_power_init();
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);

    gpio_wakeup_enable(PIN_BOOT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(PIN_KEY, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    esp_light_sleep_start();
}

bool f91w_power_sleep_until_draw(uint32_t refresh_ms, uint32_t last_draw_ms)
{
    uint32_t now = millis();
    if (now - last_draw_ms >= refresh_ms) {
        return false;
    }

    uint32_t sleep_ms = refresh_ms - (now - last_draw_ms);
    if (refresh_ms == 1000) {
        uint32_t to_sec = 1000 - (now % 1000);
        if (to_sec > 0 && to_sec < sleep_ms) {
            sleep_ms = to_sec;
        }
    }
    if (sleep_ms > MAX_SLEEP_CHUNK_MS) {
        sleep_ms = MAX_SLEEP_CHUNK_MS;
    }

    f91w_power_light_sleep_ms(sleep_ms);
    return true;
}
