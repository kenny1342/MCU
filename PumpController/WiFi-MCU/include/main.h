#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>
#include <setup.h>

// Helpers to read strings from build options macros
#define XSTR(x) #x
#define STR(x) XSTR(x)

#define FIRMWARE_VERSION    "2.51"
#define HOSTNAME            "wifi-mcu"
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

bool ConnectBlynk();
void ReconnectWiFi();
int readline(int readch, char *buffer, int len);
String HTMLProcessor(const String& var);
void saveConfigCallback ();

extern Config config;
#endif