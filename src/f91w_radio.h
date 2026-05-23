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

struct F91WSettings;

void f91w_radio_init(void);
void f91w_radio_off(void);

typedef void (*f91w_radio_idle_fn)(void);

bool f91w_wifi_ntp_sync(const F91WSettings &settings, uint32_t connect_timeout_ms,
                        uint32_t ntp_timeout_ms, f91w_radio_idle_fn idle = nullptr);
