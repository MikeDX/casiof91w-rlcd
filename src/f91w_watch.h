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

/* Waveshare board: GPIO0=BOOT = Casio C, GPIO18=KEY = Casio A. Long KEY = Casio L. */
void f91w_watch_init(uint8_t *framebuffer);
void f91w_watch_update(bool key_down, bool boot_down);
void f91w_watch_draw(void);
void f91w_watch_set_setup_mode(bool active);
void f91w_watch_set_connect_mode(bool active);
uint32_t f91w_watch_refresh_ms(void);
uint8_t *f91w_watch_framebuffer(void);
