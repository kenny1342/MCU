#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "LittleFS.h" // LittleFS is declared

#if defined(ESP8266)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp8266.h>
#include <ArduinoOTA.h>
#include <setup.h>

// Helpers to read strings from build options macros
#define XSTR(x) #x
#define STR(x) XSTR(x)

#define FIRMWARE_VERSION    "1.39"
#define HOSTNAME            "esp8266-pumpctrl"
#define PORT                "80"
#ifndef BLYNK_TOKEN // this should be set via env.py (pre-build script defined in platformio.ini)
    #define BLYNK_TOKEN         STR(BLYNK_TOKEN)
#endif
#define PIN_SW_RST          13
#define PIN_LED_1           14

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

bool ConnectBlynk();
void ReconnectWiFi();
int readline(int readch, char *buffer, int len);
String HTMLProcessor(const String& var);
void saveConfigCallback ();

extern Config config;
#endif