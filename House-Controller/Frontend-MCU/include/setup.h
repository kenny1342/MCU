
#ifndef Setup_h
#define Setup_h

struct Config {
  char hostname[64];
  char port[6]; // TODO: apply to server..
  char ntpserver[64];
  char ntp_interval[4];
  char wifi_ssid[30];
  char wifi_psk[30];
  char ping_target[30];
};

class Setup {
    private:
    bool error;
    
    public:
    static const char*  _configfile;// = "/config.json";

    Setup();
    static void deleteAllCredentials(void);
    static bool GetConfig();
    static bool SaveConfig();
    static bool FileSystem();
    static bool WebServer();
    static void OTA();
};

extern bool OTArunning;

#endif