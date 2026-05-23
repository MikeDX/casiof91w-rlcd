/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */
#pragma once

#include <Arduino.h>

struct F91WSettings {
    char ntp_server[64];
    char timezone[64];
};

void settings_load(F91WSettings &s);
void settings_save(const F91WSettings &s);
void settings_clear(void);
bool settings_is_provisioned(void);
void settings_mark_provisioned(void);
void settings_apply_timezone(const F91WSettings &s);
uint32_t settings_last_ntp_epoch(void);
void settings_mark_ntp_synced(void);
bool settings_ntp_sync_due(void);
void settings_wifi_erase_stale(void);
bool settings_try_keep_existing_wifi(void);
bool settings_wifi_quick_connect(uint32_t timeout_ms);
