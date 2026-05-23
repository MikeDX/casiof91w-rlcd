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

#define ST7305_FB_SIZE 15000

void st7305_init(void);
uint8_t *st7305_alloc_framebuffer(void);
void st7305_clear(uint8_t *fb);
void st7305_set_pixel(uint8_t *fb, int x, int y, bool on);
void st7305_push(uint8_t *fb);
