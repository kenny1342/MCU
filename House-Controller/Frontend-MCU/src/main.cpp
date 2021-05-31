/**
 * Frontend - ESP32 webserver/TFT display. Reads a JSON encoded string from serial and makes it available via HTTP + TFT.
 * 
 * Connect TX/RX to ADC-MCU (Mega2560) Serial2 pins
 * 
 * Kenny Dec 19, 2020
 */
#include <Arduino.h>

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"
#include <ST7735_SPI128x160.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_wifi.h>   // for esp_wifi_set_ps()
#include <esp_task_wdt.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <ESPmDNS.h>
#include <ESP32Ping.h>
#include <ArduinoJson.h>
#include <MD_CirQueue.h>
#include <TimeLib.h>
#include <ArduinoOTA.h>
#include <logger.h>

//#include <logo_kra-tech.h>
#include <kra-tech_128x160.c>
#include <Timemark.h>
#include <Button_KRA.h>
#include <setup.h>

#ifdef USE_WIFIMGR
#define _ESPASYNC_WIFIMGR_LOGLEVEL_    4 // Use from 0 to 4. Higher number, more debugging messages and memory usage.
#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager
#endif

#include <main.h>

char WEBIF_VERSION[6] = "N/A"; // read from file (/WEBIF_VERSION)
char ADC_VERSION[6]   = "N/A"; // filled from ADC JSON data

#ifdef USE_WIFIMGR
void WIFIconfigModeCallback (ESPAsync_WiFiManager *myWiFiManager);
DNSServer dnsServer;
#endif

//StaticJsonDocument<JSON_SIZE> JSON_DOCS[JSON_DOC_COUNT]; // = StaticJsonDocument(JSON_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)

const uint8_t QUEUE_RX_SIZE = 4;
MD_CirQueue Q_rx(QUEUE_RX_SIZE, sizeof(char)*JSON_SIZE);

char remote_data2[MAX_REMOTE_SIDS][JSON_SIZE_REMOTEPROBES];

bool shouldReboot = false;      //flag to use from web firmware update to reboot the ESP
#ifdef USE_WIFIMGR
bool shouldSaveConfig = false;  //WifiManger callback flag for saving data
#endif

const int timeZone = 1;
unsigned int localPortNTP = 55123;  // local port to listen for UDP packets
uint8_t ntp_errors = 0;
WiFiUDP ntpUDP;

uint8_t menu_page_current = 0;
LCD_state_struct LCD_state;
MENUPAGES_t MenuPages;

Button BtnUP(PIN_SW_UP, 25U, false, true); // This pin has wired pullup on TTGO T-Display board
Button BtnDOWN(PIN_SW_DOWN, 25U, true, true); // No pullup on this pin, enable internal
const uint16_t LONG_PRESS(1000);           // we define a "long press" to be 1000 milliseconds.

//TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library for TTGO T-display on-board display
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in ST7735_SPI128x160.h

char JSON_STRINGS[JSON_DOC_COUNT][JSON_SIZE] = {0};

uint32_t dataAge = 0;
uint16_t reconnects_wifi = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

Logger logger = Logger(&tft);

Timemark tm_Reboot(8*3600000); // 8 hours
Timemark tm_ClearDisplay(600000); // 600sec=10min
Timemark tm_CheckConnections(60000); 
Timemark tm_CheckDataAge(5000);
Timemark tm_PushToBlynk(2000);
Timemark tm_SerialDebug(60000);
Timemark tm_MenuReturn(30000);
Timemark tm_UpdateDisplay(1000);

const uint8_t NUM_TIMERS = 8;
enum Timers { TM_Reboot, TM_ClearDisplay, TM_CheckConnections, TM_CheckDataAge, TM_PushToBlynk, TM_SerialDebug, TM_MenuReturn, TM_UpdateDisplay };
Timemark *Timers[NUM_TIMERS] = { &tm_Reboot, &tm_ClearDisplay, &tm_CheckConnections, &tm_CheckDataAge, &tm_PushToBlynk, &tm_SerialDebug, &tm_MenuReturn, &tm_UpdateDisplay};

bool OTArunning = false;
volatile int interruptCounter;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
/**
 * Timer ISR 1ms
 */
void IRAM_ATTR onTimer() {

  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);

  if(interruptCounter == 50) {
    // every 50ms
    interruptCounter = 0;
  }
  
}


void setup(void) {
  uint8_t cnt = 0;

  pinMode(PIN_LED_1, OUTPUT);
  digitalWrite(PIN_LED_1, 0);
  BtnUP.begin();    // This also sets pinMode
  BtnDOWN.begin();  // This also sets pinMode
  
  Serial_DATA.begin(57600, SERIAL_8N1, PIN_RXD2, PIN_TXD2);
  Serial_DATA.println("WiFi-MCU booting up...");

  Serial.begin(115200);
  Serial.println("");
  Serial.println("initializing...");
  Serial.println(FPSTR(FIRMWARE_VERSION_LONG));
  Serial.println("Made by Ken-Roger Andersen, 2020");
  logger.println("");
  logger.println(F("To reset, press button for 5+ secs while powering on"));
  logger.println("");
  if(!DEBUG) delay(1000);
  
  Q_rx.setFullOverwrite(true);

  tft.init();
  tft.setRotation(1); // 1
  tft.fillScreen(LCD_state.bgcolor);
  //tft.setTextSize(2);
  tft.setTextColor(LCD_state.fgcolor);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);

  tft.setSwapBytes(true);
  //tft.pushImage(0, 0,  240, 135, kra_tech);
  tft.pushImage(0, 0,  128, 160, kra_tech);
  if(!DEBUG) delay(5000);

  tft.fillScreen(TFT_RED);
  if(!DEBUG) delay(300);
  tft.fillScreen(TFT_BLUE);
  if(!DEBUG) delay(300);
  tft.fillScreen(TFT_GREEN);
  if(!DEBUG) delay(300);

  tft.setTextWrap(true);

  tft.setTextSize(txtsize);
  tft.fillScreen(LCD_state.bgcolor);
  tft.setCursor(0 ,0);

  tft.println("\n\n   Starting...  ");
  if(!DEBUG) delay(700);

  logger.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  logger.println(F("Starting FS (SPIFFS)..."));
  Setup::FileSystem();
  if(!DEBUG) delay(700);

  Serial.print(F("reading /WEBIF_VERSION..."));
  if(SPIFFS.exists("/WEBIF_VERSION")) {
    File f = SPIFFS.open("/WEBIF_VERSION", "r");
    if(f && f.size() > 0) {
      size_t filesize = f.size(); //the size of the file in bytes 
      if(filesize < sizeof(WEBIF_VERSION)) {
        f.read((uint8_t *)WEBIF_VERSION, sizeof(WEBIF_VERSION));  
        f.close(); 
        WEBIF_VERSION[filesize] = '\0';
        Serial.printf("OK (%s)\n", WEBIF_VERSION);
      } else {
        Serial.println(F("file too large!"));
      }
    } else {
      Serial.println(F("open failed or empty file"));
    }
  } else {
    Serial.println(F("not found!"));
  }
  

  logger.print(F("Loading configuration from /config.json..."));
  Setup::GetConfig();

  if(!DEBUG) delay(500);
  logger.println(F("Starting WiFi..."));

  //WiFi.setTxPower(WIFI_POWER_19_5dBm);

#ifdef USE_WIFIMGR
  //WiFiManager custom parameters/config
  ESPAsync_WMParameter custom_hostname("hostname", "Hostname", config.hostname, 64);
  ESPAsync_WMParameter custom_port("port", "HTTP port", config.port, 6);
  ESPAsync_WMParameter custom_ntpserver("ntpserver", "NTP server", config.ntpserver, 64);

  //Local intialization. Once setup() done, there is no need to keep it around
  ESPAsync_WiFiManager ESPAsync_wifiManager(&server, &dnsServer, "KRATECH-WEB");

  ESPAsync_wifiManager.setDebugOutput(true);
  //ESPAsync_wifiManager.setConnectTimeout(10);

  ESPAsync_wifiManager.setBreakAfterConfig(true); // exit after config

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  ESPAsync_wifiManager.setAPCallback(WIFIconfigModeCallback);

  //set config save notify callback
  ESPAsync_wifiManager.setSaveConfigCallback(saveConfigCallback);
  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,30,254), IPAddress(192,168,30,1), IPAddress(255,255,255,0));
#endif

  // reset settings if flagfile exists
  if(SPIFFS.exists("/doreset.dat")) {
    logger.println("reset flagfile /doreset.dat found, resetting WiFi and rebooting as AP...");
    SPIFFS.remove("/doreset.dat");

    delay(500);
    #ifdef USE_WIFIMGR 
    ESPAsync_wifiManager.resetSettings(); 
    #endif // esp8266, not esp32
    SaveTextToFile("restart: flagfile /doreset.dat found\n", "/messages.log", true);
    delay(5000);
    ESP.restart();

  } else {
    logger.println("reset flagfile /doreset.dat not found, continue");
  }

  BtnUP.read();

  //reset network settings if button pressed
  if(BtnUP.isPressed()) {
    logger.println(F("Reset button pressed, keep pressed for 5 sec to reset network settings!"));
    esp_task_wdt_reset();
    delay(5000);
    BtnUP.read();
    if(BtnUP.isPressed() &&  BtnUP.pressedFor(5000)) {
      
      File file = SPIFFS.open("/doreset.dat", "w"); // we check for this file on boot
      if(file.print("RESET") == 0) {
        logger.println(F("SPIFFS ERROR writing flagfile /doreset.dat"));
      } else {
        logger.println(F("wrote flagfile /doreset.dat"));
      }
      file.close();
      
      logger.println(F("Will reboot to complete reset..."));
      SaveTextToFile("restart: factory reset\n", "/messages.log", true);
      delay(5000);
      ESP.restart();
    } else {
      logger.println(F("Reset cancelled, resuming normal startup..."));
    }
    
  }

  logger.println(F("Initializing WiFi..."));
  delay(1000);
  esp_task_wdt_reset();

#ifdef USE_WIFIMGR
  // our custom parameters
  ESPAsync_wifiManager.addParameter(&custom_hostname);
  ESPAsync_wifiManager.addParameter(&custom_port);
  ESPAsync_wifiManager.addParameter(&custom_ntpserver);
  //ESPAsync_wifiManager.setAPStaticIPConfig(IPAddress(192,168,30,254), IPAddress(192,168,30,1), IPAddress(255,255,255,0));

  ESPAsync_wifiManager.autoConnect("KRATECH-AP");
  if (WiFi.status() != WL_CONNECTED) { 
    logger.println(F("Failed to connect, will restart"));
    SaveTextToFile("restart: wifi failed to connect\n");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    logger.println(F("Saving config..."));
    if(Setup::SaveConfig()) {
      logger.println("OK");

      //read updated parameters
      strcpy(config.hostname, custom_hostname.getValue());
      strcpy(config.port, custom_port.getValue());
      strcpy(config.ntpserver, custom_ntpserver.getValue());
    } 
  }

  Serial.printf("Connected! SSID: %s, key: %s\n", ESPAsync_wifiManager.getStoredWiFiSSID().c_str(), ESPAsync_wifiManager.getStoredWiFiPass().c_str());
#endif

#ifndef USE_WIFIMGR
  esp_task_wdt_reset();
  logger.println(F("setting wifi mode=STA..."));
  WiFi.mode(WIFI_AP_STA);
  //esp_wifi_set_ps (WIFI_PS_NONE); // turn of power saving, resolve long ping latency and slow connects
  delay(1000);
  esp_task_wdt_reset();
  WiFi.begin(DEF_WIFI_SSID, DEF_WIFI_PW);
  esp_task_wdt_reset();
  delay(2000);
  esp_task_wdt_reset();
  logger.println(F("Connecting to WiFi..."));
  cnt = 0;
  while (WiFi.status() != WL_CONNECTED) {  
    if(cnt++ > 30) {
      logger.println(F("connection failed, rebooting!"));
      SaveTextToFile("setup(): wifi connection failed, reboot\n", "/messages.log", true);
      delay(2000);
      ESP.restart();
      return;
    }
    delay(500);  
    esp_task_wdt_reset();    
    logger.print(".");
    //Serial.print(".");
  }  
/*
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {

  } else {
    Serial.println("connection failed, rebooting!");
    SaveTextToFile("restart: wifi connection failed\n", "/messages.log", true);
    delay(2000);
    ESP.restart();
    return;
  }
  */
  Serial.printf("Connected! SSID: %s, key: %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
#endif

  esp_task_wdt_reset();
  WiFi.setAutoReconnect(true);

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  logger.print(F("Configuring OTA..."));
  Setup::OTA();
  if(!DEBUG) delay(700);

  logger.print(F("Starting MDNS..."));
  if (MDNS.begin(config.hostname)) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("FAILED"));
  }

  logger.print(F("Configuring NTP server "));
  logger.println(config.ntpserver);
  
  ntpUDP.begin(localPortNTP);
  Serial.println("syncing clock with NTP...");
  setSyncProvider(getNtpTime);
  setSyncInterval(atoi(config.ntp_interval));

  if(!DEBUG) delay(700);
  esp_task_wdt_reset();

  logger.print(F("Starting HTTP server..."));

  if(Setup::WebServer()) {
    Serial.println(F("OK"));
    MDNS.addService("http", "tcp", 80);
    Serial.printf("HTTP server ready: http://%s.local:%s (MDNS/Bonjour)\n", config.hostname, config.port);
  } else {
    Serial.println(F("FAILED"));
  }
  Serial.println(F("OK"));
  esp_task_wdt_reset();
  logger.println(F("Starting timer ISR..."));
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true); // 1000us=1ms, 1000000us=1s
  timerAlarmEnable(timer);

  for(int t=0; t<NUM_TIMERS; t++){
    Timers[t]->start();
  }

  logger.println(F("Setup completed! Waiting for data..."));
  
  //delay(2000);
  tft.setTextWrap(false);
  LCD_state.clear = 1;

  SaveTextToFile("startup completed!\n", "/messages.log", true);
  esp_task_wdt_reset();
}

#ifdef USE_WIFIMGR
//gets called when WiFiManager enters configuration mode
void WIFIconfigModeCallback (ESPAsync_WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  
  char str[150];
  snprintf(str, 150, "*IN AP/CONFIG MODE *\nTo configure the\nnetwork settings:\nConnect to WiFi/SSID\n      %s      \n  and navigate to\n%s", myWiFiManager->getConfigPortalSSID().c_str(), WiFi.softAPIP().toString().c_str());
  logger.println(str);
  //delay(2000);
  // Connect to WiFi KRA-TECH and open http://192.168.255.255 in a browser to complete configuration
}
#endif

void ReconnectWiFi() {  

  reconnects_wifi++;
  uint8_t cnt = 0;
    
  Serial.print(F("Reconnecting wifi... "));
#ifdef USE_WIFIMGR
  ESPAsync_WiFiManager ESPAsync_wifiManager(&server, &dnsServer);
  Serial.print(ESPAsync_wifiManager.getStoredWiFiSSID()); Serial.print("/"); Serial.println(ESPAsync_wifiManager.getStoredWiFiPass());
  ESPAsync_wifiManager.autoConnect();
#endif
#ifndef USE_WIFIMGR
  WiFi.reconnect();
#endif

  while (WiFi.status() != WL_CONNECTED) {  
    if(cnt++ > 20) {
      Serial.println(F("Failed, giving up!"));
      return;
    }
    delay(500);  
    esp_task_wdt_reset();
    Serial.print(".");
  }  
  Serial.println(F("Reconnected OK!"));
  reconnects_wifi = 0;
} 

/**
 * Read chars from Serial until newline and put string in buffer
 * 
 */
int readline(int readch, char *buffer, int len) {
    static int pos = 0;
    int rpos;

    if (readch > 0) {
        switch (readch) {
            case '\r': // Ignore CR
                break;
            case '\n': // Return on new-line
                rpos = pos;
                pos = 0;  // Reset position index ready for next time
                return rpos;
            default:
                if (pos < len-1) {
                    buffer[pos++] = readch;
                    buffer[pos] = 0;
                }
        }
    }
    return 0;
}

String HTMLProcessor(const String& var) {
  Serial.println(var);
  if (var == "UPTIMESECS"){
    return String(millis() / 1000);
  }
  else if (var == "FIRMWARE"){
    return String(FIRMWARE_VERSION);
  }
  else if (var == "FIRMWARE_LONG"){
    return String(FIRMWARE_VERSION_LONG);
  }
  else if (var == "WEBIF_VERSION"){
    return String(WEBIF_VERSION);
  }
  else if (var == "ADC_VERSION"){
    return String(ADC_VERSION);
  }
  else if (var == "AUTHOR_TEXT"){
    return String(AUTHOR_TEXT);
  }

  return String();
}

#ifdef USE_WIFIMGR
//WifiManager callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
#endif

void loop(void) {
  
  bool doSerialDebug = Timers[TM_SerialDebug]->expired();
  char _tmpbuf[JSON_SIZE];

  esp_task_wdt_reset();

  ArduinoOTA.handle();
  if(OTArunning) return;

  ws.cleanupClients();

  CheckButtons();

  if(Timers[TM_Reboot]->expired()){
    Serial.println("Rebooting...");
    SaveTextToFile("restart: [TM_Reboot]->expired\n", "/messages.log", true);
    delay(100);
    ESP.restart();
  }

  if(shouldReboot){
    Serial.println("Rebooting...");
    SaveTextToFile("restart: shouldReboot=true\n", "/messages.log", true);
    delay(100);
    ESP.restart();
  }
/*
  if(interruptCounter > 0) {
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
  }
*/

  if(Timers[TM_CheckConnections]->expired()) {
    CheckConnections();
  }

  if(Timers[TM_UpdateDisplay]->expired()) {
    UpdateDisplay();
  } // tm_UpdateDisplay.expired()

  // Read lines from serial RX buffer and add to queue until full
  //uint8_t Q_rx_idx = 0;
  while(Serial_DATA.available())
  {
    char buffer_rx[JSON_SIZE] = {0};
    if(Q_rx.isFull()) break; // leave remaining data in serial buffer until next loop() cycle

    Serial_DATA.readBytesUntil('\n', buffer_rx, sizeof(buffer_rx));
    if(strlen(buffer_rx) > 10) {
      Q_rx.push((uint8_t *)buffer_rx);
      //Serial.printf("Q_rx: added to #%u of %u \n", Q_rx_idx++, QUEUE_RX_SIZE);
    }
  }

  // Iterate over the RX queue and update JSON_DOCS, FIFO style
  while (!Q_rx.isEmpty()) {
    char data_string[JSON_SIZE] = {0};
    uint32_t cmd = 0;
    uint32_t devid = 0;
    int32_t sid = -1;

    if(Q_rx.isFull()) {
      Serial.println("ERR: Q_rx is full!!!" );
    }

    Q_rx.pop((uint8_t *) &data_string);
    
    dataAge = millis();

    if(doSerialDebug) {
      Serial.printf("RX (%s):", SecondsToDateTimeString(now(), TFMT_DATETIME));
      Serial.println(data_string); 
    } else {
      //Serial.print(".");
    }
/*
      strcpy(_tmpbuf, data_string);
      const char *token = strtok(_tmpbuf, ","); 
      uint32_t ret = 0;
      uint32_t _val = 0;      
      // Split the json string into tokens comma as delimiter, Keep parsing tokens while one of the delimiters present in input
      while (token != NULL) 
      { 
        //Serial.printf("token='%s' - cmd/devid/sid=%u/%u/%d\n", token, cmd, devid, sid);
        sscanf(token, "{\"cmd\":%u", &cmd);
        sscanf(token, "\"devid\":%u", &devid);   

        // sid can legally be 0, need some additional validation
        ret = sscanf(token, "\"sid\":%u", &_val);
        if(ret != 0) {
          sid = _val;
        }

        token = strtok(NULL, ","); 
      }
      _tmpbuf[0] = '\0';
  */    

    // TODO: remove tmp_json, instead parse out cmd/devid/sid/firmware etc directly, and strcpy data_string to JSON_STRINGS[n]

    DynamicJsonDocument tmp_json(JSON_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)
    DeserializationError error = deserializeJson(tmp_json, (const char*) data_string); // read-only input (duplication)
    //DeserializationError error = deserializeJson(tmp_json, data_string); // writeable (zero-copy method)
    /*
    if(cmd == 0 || devid == 0 || sid == -1) {
      Serial.print(data_string);
      Serial.printf("\nERR: invalid cmd/devid/sid=%u/%u/%d\n", cmd, devid, sid);
    } else {
      */
    if (error) {
        Serial.print(data_string);
        Serial.print(F("\n^JSON_ERR: "));
        Serial.println(error.f_str());
    } else {


      cmd = tmp_json["cmd"];
      if(!cmd) {
        cmd = 0;
      }
      devid = tmp_json["devid"];
      if(!devid) {
        devid = 0;
      }
      if(tmp_json["sid"].isNull()) {
        sid = -1;
      } else {
        sid = tmp_json["sid"];
      }
      //Serial.printf("rx: %s\ncmd/devid/sid %u/%u/%d\n", data_string, cmd, devid, sid);
      

      switch(cmd) {
        case 0x10: {// ADCSYSDATA          
  
          //serializeJson(tmp_json, JSON_STRINGS[JSON_DOC_ADCSYSDATA]);
          strcpy(JSON_STRINGS[JSON_DOC_ADCSYSDATA], data_string);

          const char *firmware = tmp_json["firmware"];
          if(firmware) {
            snprintf(ADC_VERSION, 6, "%s", firmware);
          }
        }
        break;
        case 0x11: // ADCEMONDATA
          //serializeJson(tmp_json, JSON_STRINGS[JSON_DOC_ADCEMONDATA]);
          strcpy(JSON_STRINGS[JSON_DOC_ADCEMONDATA], data_string);

        break;        
        case 0x12: // ADCWATERPUMPDATA
   
          //serializeJson(tmp_json, JSON_STRINGS[JSON_DOC_ADCWATERPUMPDATA]);
          strcpy(JSON_STRINGS[JSON_DOC_ADCWATERPUMPDATA], data_string);

        break;        
        case 0x45: // REMOTE_PROBE_DATA
        {
          uint32_t _devid = 0;
          int32_t _sid = -1;
          uint32_t _ts = 0;
          bool need_to_add = true;
          uint8_t slot_to_add = 0;
          
          if(!devid) {
            Serial.print(F("ERR 0x45 no devid found, ignoring!\n"));
            break;
          }
          tmp_json["ts"] = now();

          for(uint8_t i=MAX_REMOTE_SIDS-1; i>0; i--) {  

            if(remote_data2[i][0] != '\0' && strlen(remote_data2[i]) > 1) {    
              // make a copy of the array to tokenize, as we need to retain original
              strcpy(_tmpbuf, remote_data2[i]);
              char *token = strtok(_tmpbuf, ","); 
              uint32_t ret = 0;
              uint32_t _val = 0;

              // Keep parsing tokens while one of the delimiters present in input
              while (token != NULL) 
              { 
                  sscanf(token, "\"devid\":%u", &_devid);            
                  // sid can legally be 0, need some additional validation
                  ret = sscanf(token, "\"sid\":%u", &_val);
                  if(ret != 0) {
                    _sid = _val;
                  }
                  
                  sscanf(token, "\"ts\":%u", &_ts);
                  token = strtok(NULL, ","); 
              }
              //Serial.printf("0x45 #%u SSCANF devid=%u, sid=%u, ts=%u\n", i, _devid, _sid, _ts);

              if(_devid == 0 || _sid == -1 || _ts == 0) {
                Serial.printf("ERR: 0x45 #%u slot invalid devid/sid/ts (%u/%u/%u)\n", i, _devid, _sid, _ts);
                memset ( (void*)remote_data2[i], 0, sizeof(remote_data2[i]) );
                slot_to_add = i;
              } else {
                //Serial.printf("0x45 #%u looking for slot devid=%u,sid=%u\n", i, devid, sid);

                // if data too old, delete doc so slot can be reused
                if(now() - _ts > MAX_AGE_SID) {
                  Serial.printf("ERR: 0x45 #%u old (>%u sec), freeing spot\n", i, MAX_AGE_SID);
                  //remote_data2[i][0] = '\0';
                  memset ( (void*)remote_data2[i], 0, sizeof(remote_data2[i]) );
                }

                if(
                  _devid == devid &&
                  _sid == sid              
                ) {
                  //if(doSerialDebug) 
                    //Serial.printf("0x45 #%u updating slot devid/sid %u/%u...", i, _devid, _sid);

                  // TODO: fix random crashes (LoadException)
                  //Serial.printf("0x45 #%u tmp_json.size=%u, sizeof remote_data2=%u\n", i, tmp_json.size(), sizeof(remote_data2[MAX_REMOTE_SIDS]));
                  if(tmp_json.size() < sizeof(remote_data2[MAX_REMOTE_SIDS])) {
                    serializeJson(tmp_json, remote_data2[i]);  
                  } else {
                    Serial.print(F("ERR: 0x45 tmp_json too big for remote_data2, ignoring!\n"));
                  }
                  //Serial.print(F("OK\n"));

                  need_to_add = false;
                  break;
                } else {
                  
                  //Serial.printf("0x45 #%u no match, current/received slot devid=%u/%u,sid=%u/%u\n", i, _devid, devid, _sid, sid);
                }
              }
            } else {
              //Serial.printf("0x45 #%u slot empty\n", i);
              slot_to_add = i;
            }
          } // for

          bool is_added = false;
          if(need_to_add) { // devid/sid must be added to an empty position (json doc) in array
            if(tmp_json.size() < sizeof(remote_data2[MAX_REMOTE_SIDS])) {
              serializeJson(tmp_json, remote_data2[slot_to_add]);
              is_added = true;
              Serial.printf("0x45 #%u data added to buffer devid=%u,sid=%u\n", slot_to_add, devid, sid);
            } else {
              Serial.print(F("ERR 0x45 tmp_json too big for remote_data2, ignoring!\n"));
            }

            break;
          }

          // check if we ran out of space in array (more devid/sid than MAX_REMOTE_SIDS)
          if(need_to_add && !is_added) {
            Serial.print(F("ERR: 0x45 failed to add to buffer/no space, increase MAX_REMOTE_SIDS: "));
            Serial.println(MAX_REMOTE_SIDS);
          }
          
        }
        break;
        default: Serial.printf("WARN: Unknown CMD in JSON: %u\n", cmd);

      } // switch

    } // not error

  } // RX




} // loop()

void CheckConnections(void) {
  Serial.printf("%s: checking network health...\n", SecondsToDateTimeString(now(), TFMT_DATETIME));

  timeStatus_t NTPstatus  = timeStatus();
  if(NTPstatus != timeSet) {
    Serial.println(F("WARN: NTP not synced!"));
    //setSyncProvider(getNtpTime);
  }

  bool shouldReconnect = false;

  if (!WiFi.isConnected()) {
    shouldReconnect = true;
    Serial.print(F("ERR: WiFi disconnected"));
  }

  //IPAddress gw(192,168,30,1);
  bool ping_ok = Ping.ping(PING_TARGET, 1);
  Serial.printf("ping %s: %f ms\n", PING_TARGET, Ping.averageTime());

  if (!ping_ok || Ping.averageTime() > 200.0) {
    shouldReconnect = true;
    Serial.print(F("ERR: Ping failed"));
  }
  
  if (ntp_errors > 60) {
    shouldReconnect = true;
    Serial.printf("NTP errors: %u\n", ntp_errors);
  }
  
  if (shouldReconnect)
  {
    Serial.print(F("ERR: problems detected, trying reconnect #"));
    Serial.println(reconnects_wifi);
    ReconnectWiFi();
    
    if (reconnects_wifi > 20)
    {
      Serial.println(F("ERR: Too many WiFi attempts, rebooting..."));          
      SaveTextToFile("restart: to many wifi attempts\n", "/messages.log", true);
      ESP.restart();
      delay(10000);
      return;
    }
  } else {
    Serial.println(F("WiFi OK"));
  }

}

void UpdateDisplay(void) {
  double t = 0;
  bool showDataError = false;

  // First we parse the JSON strings into objects so we can easily access the data
  
  DynamicJsonDocument JSON_DOCS[JSON_DOC_COUNT] = DynamicJsonDocument(JSON_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)
  bool nodata = true;
  for(int x=0; x<JSON_DOC_COUNT; x++) {
    
    if(JSON_STRINGS[x][0] == '\0') {
      continue;
    }
    nodata = false;

    DeserializationError error = deserializeJson(JSON_DOCS[x], (const char*) JSON_STRINGS[x]); // read-only input (duplication)
    JSON_DOCS[x].shrinkToFit();
    if (error) {
      showDataError = true;
      Serial.print("\n");
      Serial.print(JSON_STRINGS[x]);
      Serial.print(F("\n^JSON_ERR [TFT]:"));
      Serial.println(error.f_str());
    } else {
      if(showDataError) { // clear message on display
        showDataError = false;
        LCD_state.clear = true;
      }
    }
  }

  //----------- Update OLED display ---------------

  // Automatically return to first menu page after set timeout
  if(menu_page_current != 0 && Timers[TM_MenuReturn]->expired()) {
    menu_page_current = 0;
    LCD_state.clear = true;    
  }

  if(Timers[TM_ClearDisplay]->expired() || LCD_state.clear) {
      LCD_state.clear = false;
      tft.fillScreen(LCD_state.bgcolor);
  }

  if(tm_CheckDataAge.expired()) {

    if(nodata) {
      tft.setTextSize(2);
      tft.setCursor(0, 10);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("  NO DATA RX " );
    }

    if(!nodata && (millis() - dataAge > 10000 && dataAge > 0L) ) {
      tft.setTextSize(2);
      tft.setCursor(0, 0);
      //tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("  OLD DATA  " );
    }    
  } else if(showDataError) {
    
    Serial.print("TFT INVALID DATA");
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    //tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("INVALID DATA\n" );
    //tft.println(" JSON ERROR    \n" );
    tft.printf("%lu\n", millis() );
  } else {
    switch(menu_page_current)
    {
      case MENU_PAGE_UTILITY_STATS:
      {
        tft.setTextSize(txtsize);

        if(LCD_state.bgcolor != TFT_BLACK) {
          LCD_state.bgcolor = TFT_BLACK;
          tft.fillScreen(LCD_state.bgcolor);
        }

        if(!JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].isNull()) {

        
          // ------- LINE 1/8: -------
          LCD_state.fgcolor = TFT_GOLD;
          tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
          tft.setCursor(0, 0);
          tft.printf("      SUMMARY       \n");
          LCD_state.fgcolor = TFT_GREEN;
          tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);                
          
          // ------- LINE 2/8: -------
          JsonVariant WP = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"];
          if(!WP.isNull()) {
            const char* status = WP["status"];
            if(status) {
              tft.printf("W Pump: ");
              if(strcmp(status, "RUN") == 0) {
                tft.setTextColor(TFT_WHITE, TFT_DARKGREEN); 
                tft.printf("   RUNNING  ");
              } else if(strcmp(status, "STOP") == 0) {
                tft.setTextColor(TFT_WHITE, TFT_RED); 
                tft.printf("   STOPPED  ");
              } else if(strcmp(status, "SUSPENDED") == 0) {
                tft.setTextColor(TFT_BLACK, TFT_ORANGE); 
                tft.printf("  SUSPENDED ");
              } else {
                tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
                tft.printf("  %s       ", status);
              }
              tft.printf("\n");
              tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor); 
            }
          }

          // ------- LINE 3/8: -------
          t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["pressure_bar"];
          if(t) {
            tft.printf("WP:" );
            tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
            tft.printf("%d.%02d bar, ", (int)t, (int)(t*100)%100 );
            tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
          }
          t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["temp_c"];
          if(t) {
            tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
            tft.printf("%d.%01d %cC\n", (int)t, (int)(t*10)%10, (char)247 );
            tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
          }
        }

        if(!JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].isNull()) {
          // ------- LINE 4/8: -------
          t = JSON_DOCS[JSON_DOC_ADCEMONDATA]["emon_vrms_L_N"];
          if(t) {
            tft.printf("V(rms) L-N:  ");
            tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
            tft.printf("%d.%01d V\n", (int)t, (int)(t*10)%10);
            tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
          }
          // ------- LINE 5/8: -------
          t = JSON_DOCS[JSON_DOC_ADCEMONDATA]["emon_vrms_L_PE"];
          if(t) {
            tft.printf("V(rms) L-PE: ");
            tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
            tft.printf("%d.%01d V\n", (int)t, (int)(t*10)%10);
            tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
          }
          // ------- LINE 6/8: -------
          t = JSON_DOCS[JSON_DOC_ADCEMONDATA]["emon_vrms_N_PE"];
          if(t) {
            tft.printf("V(rms) N-PE: ");
            tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
            tft.printf("%d.%01d V\n", (int)t, (int)(t*10)%10);
            tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
          }
          // ------- LINE 7/8: -------
          JsonVariant circuits = JSON_DOCS[JSON_DOC_ADCEMONDATA]["circuits"];
          if(!circuits.isNull()) {
            JsonObject circuit = circuits["1"];
            if(!circuit.isNull()) {
              uint16_t power = circuit["P_a"].as<uint16_t>();
              t = circuit["I"].as<double>();
              if(power && t) {
                tft.printf("Power: "); // "Power: 10000W/28.9A"
                tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
                tft.printf("%uW %d.%01dA \n", power, (int)t, (int)(t*10)%10);
                tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
              }
            }
          }
        }

        // ------- LINE 8/8: -------
        if(!JSON_DOCS[JSON_DOC_ADCSYSDATA].isNull()) {
          JsonArray alarms = JSON_DOCS[JSON_DOC_ADCSYSDATA]["alarms"];
          if(!alarms.isNull() && alarms.size() == 0) {
            const char *lastAlarm = JSON_DOCS[JSON_DOC_ADCSYSDATA]["lastAlarm"];
            tft.printf("Last A: %s          ", lastAlarm);
          } else {
            tft.setTextColor(TFT_RED, TFT_WHITE);
        
            for(uint8_t x=0; x< alarms.size(); x++) {
              const char* str = alarms[x];
              tft.printf("A:%s ", str );
            }
          }
        }
      }
      break;
      case MENU_PAGE_WATER_STATS:
      {
        tft.setTextSize(txtsize);

        if(LCD_state.bgcolor != TFT_BLACK) {
          LCD_state.bgcolor = TFT_BLACK;
          tft.fillScreen(LCD_state.bgcolor);
        }
        tft.setCursor(0, 0);
        LCD_state.fgcolor = TFT_GOLD;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
        
        tft.printf("    WATER SUPPLY    \n");        
        LCD_state.fgcolor = TFT_LIGHTGREY;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);

        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["pressure_bar"];
        if(t) {
          tft.printf("Pressure: %d.%02d bar\n", (int)t, (int)(t*100)%100 );
        }

        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["temp_c"];
        if(t) {
          uint8_t hum_room_pct = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["hum_room_pct"].as<uint8_t>();
          if(hum_room_pct) {
            tft.printf("Room: %d.%01d %cC, %d%%\n", (int)t, (int)(t*10)%10, (char)247, hum_room_pct );
          }          
        }

        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["temp_motor_c"];
        if(t) {
          tft.printf("Motor: %d.%01d %cC  \n", (int)t, (int)(t*10)%10, (char)247 );
        }

        if(!JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"].isNull()) {
          if(!JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"]["cnt_starts"].isNull()) {
            tft.printf("Starts/stops: %u  \n", JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"]["cnt_starts"].as<uint16_t>());
          }
        }
        
        if(!JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"].isNull()) {
          const char * state = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"]["status"].as<char *>();
          uint32_t t_val = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"]["t_state"].as<uint32_t>();
          if(state && t_val) {
            tft.printf("%s: %s\n", state, SecondsToDateTimeString(t_val, TFMT_HOURS));
          }
          t_val = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA]["WP"]["t_susp_tot"].as<uint32_t>();
          if(t_val) {
            tft.printf("Susp tot: %s\n", SecondsToDateTimeString(t_val, TFMT_HOURS));
          }
          
        }

        tft.printf("Power usage: NA Watt\n");
      }
      break;
      case MENU_PAGE_SYSTEM_STATS:
      {
        tft.setTextSize(txtsize);

        if(LCD_state.bgcolor != TFT_WHITE) {
          LCD_state.bgcolor = TFT_WHITE;
          tft.fillScreen(LCD_state.bgcolor);
        }
        
        LCD_state.fgcolor = TFT_BLACK;        
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        
        tft.setCursor(0, 0);
        tft.printf("%-14s\n", SecondsToDateTimeString(now(), TFMT_DATETIME));
        //tft.printf("%-14s\n", );
        tft.printf("SSID: %-14s\n", WiFi.SSID().c_str());
        tft.printf("IP: %-16s\n", WiFi.localIP().toString().c_str());
        tft.printf("WiFi reconnects: %u\n", reconnects_wifi);
        tft.printf("Free mem: %u B\n", ESP.getFreeHeap());
        tft.printf("CPU freq: %u Mhz\n", ESP.getCpuFreqMHz());
        tft.printf("ID: %llu\n", ESP.getEfuseMac());
        //tft.setTextWrap(false);
        //tft.printf("SDK: %s\n", ESP.getSdkVersion());
        //tft.setTextWrap(true);        
        tft.printf("Uptime: %s\n", SecondsToDateTimeString(millis()/1000, TFMT_HOURS));
      }
      break;
      case MENU_PAGE_ABOUT: 
      {
        tft.setTextSize(txtsize);
        tft.setTextWrap(false);
        if(LCD_state.bgcolor != TFT_BLACK) {
          LCD_state.bgcolor = TFT_BLACK;
          tft.fillScreen(LCD_state.bgcolor);
        }
        tft.setCursor(0, 0);
        LCD_state.fgcolor = TFT_GOLD;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
        
        tft.printf("      VERSIONS      \n");        
        LCD_state.fgcolor = TFT_ORANGE;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        tft.printf("\n");
        tft.printf(" Web-MCU: v%s\n", FIRMWARE_VERSION); //L3
        const char *adc_firmware = JSON_DOCS[JSON_DOC_ADCSYSDATA]["firmware"];
        if(adc_firmware) {
          tft.printf(" ADC-MCU: v%s\n", adc_firmware);
        }
        tft.printf(" Web-IF:  v%s\n", WEBIF_VERSION); //L5
        tft.printf("   (c) %s    \n", AUTHOR_COPYRIGHT ); //L6
        tft.print(F(" Ken-Roger Andersen \n") );
        tft.print(F("ken.roger@gmail.com \n") );
        
        tft.setTextWrap(true);
      }
      break;
      case MENU_PAGE_LOGO:
        tft.pushImage(0, 0,  240, 135, kra_tech);
      break;

      default: menu_page_current = 0;
    } // switch()
  } // if showError

}
/**
 * Check button states
 */
void CheckButtons(void) {
  BtnDOWN.read();
  BtnUP.read();
  if(BtnUP.wasReleased()) {
    tm_MenuReturn.start();
    if(menu_page_current == MENU_PAGE_MAX) {
      menu_page_current = 0;
    } else {
      menu_page_current++;
    }
    LCD_state.clear = true;
  } else if(BtnUP.pressedFor(LONG_PRESS)) {
    Serial.println("BTNUP LONG PRESS");
    delay(5000);
  }

  if(BtnDOWN.wasReleased()) {
    tm_MenuReturn.start();
    if(menu_page_current == 0) {
      menu_page_current = MENU_PAGE_MAX;
    } else {
      menu_page_current--;
    }
    LCD_state.clear = true;
  } else if(BtnDOWN.pressedFor(LONG_PRESS)) {
      Serial.println("BTNDOWN LONG PRESS");
      delay(5000);
  }

}

char * SecondsToDateTimeString(uint32_t seconds, uint8_t format)
{
  time_t curSec;
  struct tm *curDate;
  static char dateString[32];
  
  curSec = time(NULL) + seconds;
  curDate = localtime(&curSec);

  switch(format) {    
    case TFMT_LONG: strftime(dateString, sizeof(dateString), "%A, %B %d %Y %H:%M:%S", curDate); break;
    case TFMT_DATETIME: strftime(dateString, sizeof(dateString), "%Y-%m-%d %H:%M:%S", curDate); break;
    case TFMT_HOURS: 
    {
      long h = seconds / 3600;
      uint32_t t = seconds % 3600;
      int m = t / 60;
      int s = t % 60;
      sprintf(dateString, "%04ld:%02d:%02d", h, m, s);
    }
      break;

    default: strftime(dateString, sizeof(dateString), "%Y-%m-%d %H:%M:%S", curDate);
  }
  return dateString;
}

/*-------- NTP code ----------*/
//const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (ntpUDP.parsePacket() > 0) ; // discard any previously received packets
  Serial.print("Sending NTP request to ");
  // get a random server from the pool
  WiFi.hostByName(config.ntpserver, ntpServerIP);
  Serial.print(config.ntpserver);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = ntpUDP.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Received NTP reply");
      ntp_errors = 0;
      ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("ERR: No NTP reply");
  ntp_errors++;
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  ntpUDP.beginPacket(address, 123); //NTP requests are to port 123
  ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();
}

void SaveTextToFile(const char *text, const char *filename, bool append) {
  File file ;

  
  if(append) {
    file = SPIFFS.open(filename, "a");
  } else {
    file = SPIFFS.open(filename, "w");
  }

  //if(file.size() > 10000) {
  if(SPIFFS.totalBytes() - SPIFFS.usedBytes() < 100) {
    Serial.println(F("no free space, reset file"));
    if(append) {
      Serial.println(F("append=true, reopening file in write mode"));
      file.close();
      file = SPIFFS.open(filename, "w");
    }
  }

  if (!file) {
    Serial.printf("There was an error opening '%s' for writing\n", filename);
    return;
  }
  if (
    file.print( SecondsToDateTimeString(now(), TFMT_DATETIME) ) && 
    file.print(": ") &&
    file.print(text)) {
    Serial.printf("wrote '%s'\n", filename);
  } else {
    Serial.printf("ERR wroting '%s'\n", filename);
  }
  file.flush();
  file.close();
  delay(200);
}