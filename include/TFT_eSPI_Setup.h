#pragma once

// TFT_eSPI local setup for ESP32 (VSPI bus)
// Change the driver define below if your display controller is different.
#define ILI9341_DRIVER

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_CS 14
#define TFT_DC 27
#define TFT_RST 33
#define TFT_BL 32

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 20000000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
