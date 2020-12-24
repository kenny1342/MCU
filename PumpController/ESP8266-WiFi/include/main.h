#pragma once
#ifndef __MAIN_H
#define __MAIN_H

// Helpers to read strings from build options macros
#define XSTR(x) #x
#define STR(x) XSTR(x)

#define FIRMWARE_VERSION    "0.11"
#define HOSTNAME            "esp8266-pumpctrl"
#ifndef BLYNK_TOKEN // this should be set via env.py (pre-build script defined in platformio.ini)
    #define BLYNK_TOKEN         STR(BLYNK_TOKEN)
#endif
#define PIN_SW_RST          13
#define PIN_LED_1           14

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

extern ESP8266WebServer server;
extern char json_output[200];
extern char celsius[4];
extern char pressure_bar[4];
extern uint16_t reconnects_wifi;

void ConnectBlynk();
void ReconnectWiFi();
int readline(int readch, char *buffer, int len);
String HTMLProcessor(const String& var);
//extern void handleRoot();
//extern void handleJSON();
extern void handleNotFound();

#endif