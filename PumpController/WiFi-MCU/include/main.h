#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <setup.h>


// Helpers to read strings from build options macros
#define XSTR(x) #x
#define STR(x) XSTR(x)

#define FIRMWARE_VERSION    "2.59"
#define WEBIF_VERSION       "3.01"
#define AUTHOR_COPYRIGHT    "2020-2021"
#define AUTHOR_TEXT         ("(c) Ken-Roger Andersen " AUTHOR_COPYRIGHT  " - ken.roger@gmail.com")
#define HOSTNAME            "websrv-mcu"
#define PORT                "80"
#define JSON_SIZE           768
#ifndef BLYNK_TOKEN // this should be set via env.py (pre-build script defined in platformio.ini)
    #define BLYNK_TOKEN         STR(BLYNK_TOKEN)
#endif
#define PIN_SW_DOWN         0 //23 (IO0)
#define PIN_SW_UP           35 // 11 (IO35)
#define PIN_LED_1           37
#define PIN_RXD2 25
#define PIN_TXD2 26

#define Serial_DATA Serial2 // Serial used talking to ADC MCU/JSON data

enum MENUPAGES_t { MENU_PAGE_UTILITY_STATS = 0, MENU_PAGE_WATER_STATS = 1, MENU_PAGE_SYSTEM_STATS = 2, MENU_PAGE_ABOUT = 3, MENU_PAGE_LOGO = 4 };
#define MENU_PAGE_MAX           4

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

//ST7789 OLED display
#define txtsize 2

struct LCD_state_struct {
    bool clear = true; // flag to clear display
    //bool textwrap = true;
    uint16_t bgcolor = TFT_BLACK;
    uint16_t fgcolor = TFT_GREEN;    
} ;
/*
union ISRFLAGS_struct {
  byte allBits;
    struct {
        bool lcd_clear = false;
    };
};
*/


bool ConnectBlynk();
void ReconnectWiFi();
int readline(int readch, char *buffer, int len);
String HTMLProcessor(const String& var);
void saveConfigCallback ();
//void makeCStringSpaces(char str[], int _len);
void CheckButtons(void);
char * TimeToString(unsigned long t);

extern Config config;
#endif