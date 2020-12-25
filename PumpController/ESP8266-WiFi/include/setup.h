#ifndef Setup_h
#define Setup_h

#include "Arduino.h"
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager


struct Config {
  char hostname[64];
  char port[6]; // TODO: apply to server..
};

class Setup {
    private:
    bool error;
    

    public:
    static const char*  _configfile;// = "/config.json";

    //Config config;
    Setup();
    static bool GetConfig();
    static bool SaveConfig();
    static bool FileSystem();
    static bool WebServer();
};


extern String HTMLProcessor(const String& var);
extern DNSServer dns;
extern AsyncWebSocket ws;
extern AsyncEventSource events;
extern AsyncWebServer server;
extern bool shouldReboot;
extern bool shouldSaveConfig;
extern char json_output[200];
extern char celsius[5];
extern char pressure_bar[5];
extern const char* _def_hostname;

#endif