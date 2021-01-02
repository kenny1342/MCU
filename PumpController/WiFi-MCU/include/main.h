#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>

#include <setup.h>

// Helpers to read strings from build options macros
#define XSTR(x) #x
#define STR(x) XSTR(x)

#define FIRMWARE_VERSION    "2.54"
#define HOSTNAME            "websrv-mcu"
#define PORT                "80"
#define JSON_SIZE           512
#ifndef BLYNK_TOKEN // this should be set via env.py (pre-build script defined in platformio.ini)
    #define BLYNK_TOKEN         STR(BLYNK_TOKEN)
#endif
#define PIN_SW_RST          23 // (IO0)
#define PIN_LED_1           37
#define PIN_RXD2 25
#define PIN_TXD2 26

#define Serial_DATA Serial2 // Serial used talking to ADC MCU/JSON data

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

//ST7789 OLED display
#define txtsize 2
#define charheight 10
//byte charwidth = 6;
#if txtsize == 2
  #define TFT_LINE1 0
  #define TFT_LINE2 1 * (4+charheight+txtsize)
  #define TFT_LINE3 2 * (4+charheight+txtsize)
  #define TFT_LINE4 3 * (4+charheight+txtsize)
  #define TFT_LINE5 4 * (4+charheight+txtsize)
  #define TFT_LINE6 5 * (4+charheight+txtsize)
  #define TFT_LINE7 6 * (4+charheight+txtsize)
  #define TFT_LINE8 7 * (4+charheight+txtsize)
#endif



bool ConnectBlynk();
void ReconnectWiFi();
int readline(int readch, char *buffer, int len);
String HTMLProcessor(const String& var);
void saveConfigCallback ();
void makeCStringSpaces(char str[], int _len);

extern Config config;
#endif