#include "f91w_display.h"

#include <Arduino.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <cstring>

static constexpr int PIN_SCK  = 11;
static constexpr int PIN_MOSI = 12;
static constexpr int PIN_DC   = 5;
static constexpr int PIN_CS   = 40;
static constexpr int PIN_RST  = 41;
static constexpr uint32_t SPI_HZ = 24000000;

static SPIClass st7305Spi(HSPI);

static void sendCmd(uint8_t cmd)
{
    digitalWrite(PIN_DC, LOW);
    digitalWrite(PIN_CS, LOW);
    st7305Spi.transfer(cmd);
    digitalWrite(PIN_CS, HIGH);
}

static void sendData(const uint8_t *data, size_t len)
{
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    st7305Spi.transferBytes(const_cast<uint8_t *>(data), nullptr, len);
    digitalWrite(PIN_CS, HIGH);
}

static void sendCmdData(uint8_t cmd, const uint8_t *data, size_t len)
{
    digitalWrite(PIN_DC, LOW);
    digitalWrite(PIN_CS, LOW);
    st7305Spi.transfer(cmd);
    if (len > 0) {
        digitalWrite(PIN_DC, HIGH);
        st7305Spi.transferBytes(const_cast<uint8_t *>(data), nullptr, len);
    }
    digitalWrite(PIN_CS, HIGH);
}

static void panelReset(void)
{
    digitalWrite(PIN_RST, HIGH);
    delay(50);
    digitalWrite(PIN_RST, LOW);
    delay(20);
    digitalWrite(PIN_RST, HIGH);
    delay(50);
}

/* Copied verbatim from Waveshare ST7305_U8g2::fullInit() */
static void fullInit(void)
{
    panelReset();

    const uint8_t d6[] = {0x17, 0x02};
    const uint8_t d1[] = {0x01};
    const uint8_t c0[] = {0x11, 0x04};
    const uint8_t c1[] = {0x69, 0x69, 0x69, 0x69};
    const uint8_t c2[] = {0x19, 0x19, 0x19, 0x19};
    const uint8_t c4[] = {0x4B, 0x4B, 0x4B, 0x4B};
    const uint8_t d8[] = {0x80, 0xE9};
    const uint8_t b2[] = {0x02};
    const uint8_t b3[] = {0xE5, 0xF6, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45};
    const uint8_t b4[] = {0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45};
    const uint8_t g_timing[] = {0x32, 0x03, 0x1F};
    const uint8_t b7[] = {0x13};
    const uint8_t b0[] = {0x64};
    const uint8_t c9[] = {0x00};
    const uint8_t m36[] = {0x48};
    const uint8_t m3a[] = {0x11};
    const uint8_t b9[] = {0x20};
    const uint8_t b8[] = {0x29};
    const uint8_t win_a[] = {0x12, 0x2A};
    const uint8_t win_b[] = {0x00, 0xC7};
    const uint8_t m35[] = {0x00};
    const uint8_t d0[] = {0xFF};

    sendCmdData(0xD6, d6, sizeof(d6));
    sendCmdData(0xD1, d1, sizeof(d1));
    sendCmdData(0xC0, c0, sizeof(c0));
    sendCmdData(0xC1, c1, sizeof(c1));
    sendCmdData(0xC2, c2, sizeof(c2));
    sendCmdData(0xC4, c4, sizeof(c4));
    sendCmdData(0xC5, c2, sizeof(c2));
    sendCmdData(0xD8, d8, sizeof(d8));
    sendCmdData(0xB2, b2, sizeof(b2));
    sendCmdData(0xB3, b3, sizeof(b3));
    sendCmdData(0xB4, b4, sizeof(b4));
    sendCmdData(0x62, g_timing, sizeof(g_timing));
    sendCmdData(0xB7, b7, sizeof(b7));
    sendCmdData(0xB0, b0, sizeof(b0));
    sendCmd(0x11);
    delay(120);
    sendCmdData(0xC9, c9, sizeof(c9));
    sendCmdData(0x36, m36, sizeof(m36));
    sendCmdData(0x3A, m3a, sizeof(m3a));
    sendCmdData(0xB9, b9, sizeof(b9));
    sendCmdData(0xB8, b8, sizeof(b8));
    sendCmd(0x21);
    sendCmdData(0x2A, win_a, sizeof(win_a));
    sendCmdData(0x2B, win_b, sizeof(win_b));
    sendCmdData(0x35, m35, sizeof(m35));
    sendCmdData(0xD0, d0, sizeof(d0));
    sendCmd(0x38);
    sendCmd(0x29);
}

void st7305_set_pixel(uint8_t *fb, int x, int y, bool on)
{
    int inv_y = 299 - y;
    int index = (x / 2) * 75 + (inv_y / 4);
    int bit = 7 - ((inv_y % 4) * 2 + (x % 2));
    if (on) {
        fb[index] &= ~(1 << bit); /* on = black ink */
    } else {
        fb[index] |= (1 << bit);  /* off = white paper */
    }
}

void st7305_clear(uint8_t *fb)
{
    memset(fb, 0xFF, ST7305_FB_SIZE);
}

void st7305_init(void)
{
    pinMode(PIN_DC, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_RST, HIGH);

    st7305Spi.begin(PIN_SCK, -1, PIN_MOSI, -1);
    st7305Spi.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
    fullInit();
    Serial.println("ST7305 display initialized");
}

uint8_t *st7305_alloc_framebuffer(void)
{
    uint8_t *buf = (uint8_t *)heap_caps_malloc(
        ST7305_FB_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = (uint8_t *)malloc(ST7305_FB_SIZE);
    }
    if (buf) {
        memset(buf, 0xFF, ST7305_FB_SIZE);
    }
    return buf;
}

void st7305_push(uint8_t *fb)
{
    const uint8_t win_a[] = {0x12, 0x2A};
    const uint8_t win_b[] = {0x00, 0xC7};
    sendCmdData(0x2A, win_a, sizeof(win_a));
    sendCmdData(0x2B, win_b, sizeof(win_b));
    sendCmd(0x2C);
    sendData(fb, ST7305_FB_SIZE);
}
