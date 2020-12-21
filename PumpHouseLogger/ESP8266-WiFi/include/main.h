#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#define FIRMWARE_VERSION    "0.10"
#define BLYNK_TOKEN         "YvC4EfhIoGyWjgxEumhj-txdtwooSyg4"  // Auth Token from project in the Blynk App
#define PIN_SW_RST          13
#define PIN_LED_1           14

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

void ConnectBlynk();
void ReconnectWiFi();
int readline(int readch, char *buffer, int len);
void handleRoot();
void handleJSON();
void handleNotFound();
#endif