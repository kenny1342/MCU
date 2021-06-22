#ifndef __ST7735_SPI128x160_H
#define __ST7735_SPI128x160_H

// Copied and modified from library TFTP_eSPI/User_Setups/Setup2_ST7735.h
// to work with banggoods.com display 1.8 SPI 128x160
// include this file before TFT_eSPI.h

// See SetupX_Template.h for all options available

#define ST7735_DRIVER

#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// This makes sure colors TFT_xxxx are shown/defined correctly
#define ST7735_GREENTAB2

//#define TFT_MISO -1
#define TFT_MOSI 14 //23
#define TFT_SCLK 33 //18
#define TFT_CS    15  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
#define TFT_RST   4  // Reset pin (could connect to RST pin)

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

// #define SPI_FREQUENCY  20000000
#define SPI_FREQUENCY  27000000
// #define SPI_FREQUENCY  40000000

#define SPI_TOUCH_FREQUENCY  2500000

#endif