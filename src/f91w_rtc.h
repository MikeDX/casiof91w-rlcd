/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */
#pragma once

#include <time.h>

void f91w_rtc_init(void);
void f91w_rtc_read_to_system(void);
void f91w_rtc_write_from_system(void);
struct tm f91w_rtc_read(void);
bool f91w_rtc_read_local_tm(struct tm *local);
bool f91w_rtc_present(void);
