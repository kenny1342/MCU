#ifndef Setup_h
#define Setup_h

#include "Arduino.h"
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESPmDNS.h>
#include <ArduinoJson.h>


struct Config {
  char hostname[64];
  char port[6]; // TODO: apply to server..
};

class Setup {
    private:
    bool error;
    
    public:
    static const char*  _configfile;// = "/config.json";

    Setup();
    static bool GetConfig();
    static bool SaveConfig();
    static bool FileSystem();
    static bool WebServer();
};

#endif