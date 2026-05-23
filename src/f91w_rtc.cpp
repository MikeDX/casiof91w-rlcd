/*
 * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2"
 *
 * Copyright (c) 2025 MikeDX
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/mikedx/casiof91w-rlcd
 */

#include "f91w_rtc.h"

#include <Arduino.h>
#include <Wire.h>
#include <sys/time.h>

static constexpr uint8_t PCF85063_ADDR = 0x51;
static constexpr int PIN_SDA = 13;
static constexpr int PIN_SCL = 14;

static bool rtc_ok = false;

static uint8_t bcd2dec(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10) + (v & 0x0F));
}

static uint8_t dec2bcd(uint8_t v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static bool readRegs(uint8_t reg, uint8_t *buf, size_t len)
{
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom((int)PCF85063_ADDR, (int)len) != (int)len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

static bool writeRegs(uint8_t reg, const uint8_t *buf, size_t len)
{
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(reg);
    for (size_t i = 0; i < len; i++) {
        Wire.write(buf[i]);
    }
    return Wire.endTransmission() == 0;
}

void f91w_rtc_init(void)
{
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);
    uint8_t sec = 0;
    rtc_ok = readRegs(0x04, &sec, 1);
    if (rtc_ok) {
        Serial.println("PCF85063 RTC found");
    } else {
        Serial.println("PCF85063 RTC not found");
    }
}

bool f91w_rtc_present(void)
{
    return rtc_ok;
}

struct tm f91w_rtc_read(void)
{
    struct tm t = {};
    if (!rtc_ok) {
        return t;
    }

    uint8_t raw[7] = {};
    if (!readRegs(0x04, raw, sizeof(raw))) {
        return t;
    }

    t.tm_sec  = bcd2dec(raw[0] & 0x7F);
    t.tm_min  = bcd2dec(raw[1] & 0x7F);
    t.tm_hour = bcd2dec(raw[2] & 0x3F);
    t.tm_mday = bcd2dec(raw[3] & 0x3F);
    t.tm_wday = raw[4] & 0x07;
    t.tm_mon  = bcd2dec(raw[5] & 0x1F) - 1;
    t.tm_year = bcd2dec(raw[6]) + 100;
  return t;
}

void f91w_rtc_read_to_system(void)
{
    struct tm t = f91w_rtc_read();
    if (t.tm_year < 100) {
        return;
    }
    time_t epoch = mktime(&t);
    if (epoch < 0) {
        return;
    }
    struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
    settimeofday(&tv, nullptr);
    Serial.println("System time set from RTC");
}

void f91w_rtc_write_from_system(void)
{
    if (!rtc_ok) {
        return;
    }

    time_t now = time(nullptr);
    struct tm t;
    if (!localtime_r(&now, &t)) {
        return;
    }

    uint8_t raw[7] = {
        dec2bcd((uint8_t)t.tm_sec),
        dec2bcd((uint8_t)t.tm_min),
        dec2bcd((uint8_t)t.tm_hour),
        dec2bcd((uint8_t)t.tm_mday),
        (uint8_t)t.tm_wday,
        dec2bcd((uint8_t)(t.tm_mon + 1)),
        dec2bcd((uint8_t)(t.tm_year - 100)),
    };
    writeRegs(0x04, raw, sizeof(raw));
}
