/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */
#pragma once

#include <stdint.h>

void f91w_power_init(void);

/* Light sleep until timer (and BOOT/KEY low). No-op if ms too short. */
void f91w_power_light_sleep_ms(uint32_t ms);

/* Sleep until next display tick; returns true if slept. */
bool f91w_power_sleep_until_draw(uint32_t refresh_ms, uint32_t last_draw_ms);
