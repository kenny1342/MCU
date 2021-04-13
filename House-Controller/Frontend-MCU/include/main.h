#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <setup.h>

//#define USE_WIFIMGR
#define DEF_WIFI_SSID  "service_wifi"
//#define DEF_WIFI_SSID  "MCU"
#define DEF_WIFI_PW    "hemmelig"

#define PING_TARGET "192.168.30.1"  // host/ip to ping for network/wifi status testing

// Helpers to read strings from build options macros
#define XSTR(x) #x
#define STR(x) XSTR(x)

#define DEBUG               1       // more verbose + disables all delays (logo display, pause between display messages etc) in setup()

#define FIRMWARE_VERSION    "3.28"
#define AUTHOR_COPYRIGHT    "2020-2021"
#define AUTHOR_TEXT         ("(c) Ken-Roger Andersen " AUTHOR_COPYRIGHT  " - ken.roger@gmail.com")
// store long global string in flash (put the pointers to PROGMEM)
const char FIRMWARE_VERSION_LONG[] PROGMEM = "HouseMaster (MCU ESP32-WiFi) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;

#define CONF_DEF_HOSTNAME            "websrv-mcu"
#define CONF_DEF_PORT                "80"
#define CONF_DEF_NTP_SERVER          "192.168.30.13"
#define CONF_DEF_NTP_INTERVAL        "60"  // secs

#define JSON_SIZE                   900
#define JSON_SIZE_REMOTEPROBES      250
#define MAX_REMOTE_SIDS             20 // number of remote sensor id's JSON strings to store in memory
#define MAX_AGE_SID                 3600 // seconds we keep a devid/sid probe data in memory before deleting (dead remote probe)

#ifndef BLYNK_TOKEN // this should be set via env.py (pre-build script defined in platformio.ini)
    #define BLYNK_TOKEN         STR(BLYNK_TOKEN)
#endif
#define PIN_SW_DOWN         0 //23 (IO0)
#define PIN_SW_UP           0//35 // 11 (IO35)
#define PIN_LED_1           37
#define PIN_RXD2 25
#define PIN_TXD2 26

#define JSON_DOC_ADCSYSDATA         0
#define JSON_DOC_ADCEMONDATA        1
#define JSON_DOC_ADCWATERPUMPDATA   2
#define JSON_DOC_COUNT              3

#define Serial_DATA Serial2 // Serial used talking to ADC MCU/JSON data

enum MENUPAGES_t { MENU_PAGE_UTILITY_STATS = 0, MENU_PAGE_WATER_STATS = 1, MENU_PAGE_SYSTEM_STATS = 2, MENU_PAGE_ABOUT = 3, MENU_PAGE_LOGO = 4 };
#define MENU_PAGE_MAX           4

enum TIME_STRING_FORMATS { TFMT_LONG, TFMT_DATETIME, TFMT_HOURS };

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

//ST7789 OLED display
#define txtsize 1

struct LCD_state_struct {
    bool clear = true; // flag to clear display
    //bool textwrap = true;
    uint16_t bgcolor = TFT_BLACK;
    uint16_t fgcolor = TFT_GREEN;    
} ;

#ifdef USE_BLYNK
bool ConnectBlynk();
#endif
void ReconnectWiFi();
int readline(int readch, char *buffer, int len);
String HTMLProcessor(const String& var);
void saveConfigCallback ();
void CheckConnections(void);
void UpdateDisplay(void);
void CheckButtons(void);
char * SecondsToDateTimeString(uint32_t seconds, uint8_t format);
time_t sync();
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
void SaveTextToFile(const char *text, const char *filename, bool append);

extern Config config;

#endif