#pragma once
#ifndef __HTTP_METHODS_H
#define __HTTP_METHODS_H

#include "Arduino.h"
//#include <FS.h>                   //this needs to be first, or it all crashes and burns...

//#include <WiFi.h>
//#include <Esp.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>

//#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager
#include <main.h>

class Webserver {
    private:
    bool error;
    public:
    static const char*  _configfile;// = "/config.json";

    Webserver();
    static void AddRoutes();
};

extern String HTMLProcessor(const String& var);
//extern DNSServer dnsServer;
extern AsyncWebSocket ws;
extern AsyncEventSource events;
extern AsyncWebServer server;
extern bool shouldReboot;
extern bool shouldSaveConfig;
extern const char* _def_hostname;
//extern StaticJsonDocument<JSON_SIZE> data_json_adcemon;
//extern StaticJsonDocument<JSON_SIZE> data_json_sensors;
//extern StaticJsonDocument<JSON_SIZE> JSON_DOCS[JSON_DOC_COUNT];
extern char JSON_STRINGS[4][JSON_SIZE];

void onRequest(AsyncWebServerRequest *request);
void onNotFound(AsyncWebServerRequest *request);
void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

#endif