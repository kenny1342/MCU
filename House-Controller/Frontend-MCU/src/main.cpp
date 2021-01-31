/**
 * Frontend - ESP32 webserver/TFT display. Reads a JSON encoded string from serial and makes it available via HTTP + TFT.
 * 
 * Connect TX/RX to ADC-MCU (Mega2560) Serial2 pins
 * 
 * * TODO: replace serial link to ADC-MCU with SPI
 * 
 * Kenny Dec 19, 2020
 */
#include <Arduino.h>
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"
#include <WiFi.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <CircularBuffer.h>
#include <BlynkSimpleEsp32.h>
#include <ArduinoOTA.h>
#include <logger.h>
#include <TFT_eSPI.h>
//#include "bmp.h" // TODO: remove, free space
#include <logo_kra-tech.h>
#include <Timemark.h>
#include <Button_KRA.h>
#include <setup.h>

#define _ESPASYNC_WIFIMGR_LOGLEVEL_    4 // Use from 0 to 4. Higher number, more debugging messages and memory usage.
#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager
#include <main.h>

char WEBIF_VERSION[6] = "N/A"; // read from file (/WEBIF_VERSION)
char ADC_VERSION[6]   = "N/A"; // filled from ADC JSON data

void WIFIconfigModeCallback (ESPAsync_WiFiManager *myWiFiManager);

CircularBuffer<char*, 10> queue_rx;

bool shouldReboot = false;      //flag to use from web firmware update to reboot the ESP
bool shouldSaveConfig = false;  //WifiManger callback flag for saving data

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, CONF_DEF_NTP_SERVER, 3600, 20);

int blynk_button_V2 = 0;
uint8_t menu_page_current = 0;
LCD_state_struct LCD_state;
MENUPAGES_t MenuPages;

Button BtnUP(PIN_SW_UP, 25U, false, true); // This pin has wired pullup on TTGO T-Display board
Button BtnDOWN(PIN_SW_DOWN, 25U, true, true); // No pullup on this pin, enable internal
const uint16_t LONG_PRESS(1000);           // we define a "long press" to be 1000 milliseconds.

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

char JSON_STRINGS[JSON_DOC_COUNT][JSON_SIZE] = {0};

uint32_t dataAge = 0;
uint16_t reconnects_wifi = 0;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

Logger logger = Logger(&tft);

Timemark tm_ClearDisplay(600000); // 600sec=10min
Timemark tm_CheckConnections(120000); 
Timemark tm_CheckDataAge(5000);
Timemark tm_PushToBlynk(2000);
Timemark tm_SerialDebug(5000);
Timemark tm_MenuReturn(30000);
Timemark tm_UpdateDisplay(1000);
Timemark tm_SyncNTP(180000);

const uint8_t NUM_TIMERS = 8;
enum Timers { TM_ClearDisplay, TM_CheckConnections, TM_CheckDataAge, TM_PushToBlynk, TM_SerialDebug, TM_MenuReturn, TM_UpdateDisplay, TM_SyncNTP };
Timemark *Timers[NUM_TIMERS] = { &tm_ClearDisplay, &tm_CheckConnections, &tm_CheckDataAge, &tm_PushToBlynk, &tm_SerialDebug, &tm_MenuReturn, &tm_UpdateDisplay, &tm_SyncNTP };

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
  
}

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
  // Request the latest state from the server
  Blynk.syncVirtual(V2);
}

// When App button is pushed - switch the state
BLYNK_WRITE(V2) {
  blynk_button_V2 = param.asInt();
  digitalWrite(PIN_LED_1, blynk_button_V2);
}

void setup(void) {
  
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


  tft.init();
  tft.setRotation(3); // 1
  tft.fillScreen(LCD_state.bgcolor);
  //tft.setTextSize(2);
  tft.setTextColor(LCD_state.fgcolor);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);

  tft.setSwapBytes(true);
  //tft.pushImage(0, 0,  240, 135, ttgo);
  tft.pushImage(0, 0,  240, 135, kra_tech);
  if(!DEBUG) delay(10000);

  tft.fillScreen(TFT_RED);
  if(!DEBUG) delay(300);
  tft.fillScreen(TFT_BLUE);
  if(!DEBUG) delay(300);
  tft.fillScreen(TFT_GREEN);
  if(!DEBUG) delay(300);

  tft.setTextWrap(true);

  tft.setTextSize(3);
  tft.fillScreen(LCD_state.bgcolor);
  tft.setCursor(0 ,0);

  tft.println("\n\n   Starting...  ");
  tft.setTextSize(txtsize);
  if(!DEBUG) delay(700);

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
/*
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin("kra-stb", "escort82");
  delay(1000);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {

  } else {
    Serial.println("connection failed!");
    delay(1000);
    ESP.restart();
  }
*/

  WiFi.setTxPower(WIFI_POWER_19_5dBm);

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


  // reset settings if flagfile exists
  if(SPIFFS.exists("/doreset.dat")) {
    logger.println("reset flagfile /doreset.dat found, resetting WiFi and rebooting as AP...");
    SPIFFS.remove("/doreset.dat");

    delay(500);
    ESPAsync_wifiManager.resetSettings();  // esp8266, not esp32
    delay(5000);
    ESP.restart();

  } else {
    logger.println("reset flagfile /doreset.dat not found, continue");
  }

  BtnUP.read();

  //reset network settings if button pressed
  if(BtnUP.isPressed()) {
    logger.println(F("Reset button pressed, keep pressed for 5 sec to reset network settings!"));
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
      delay(5000);
      ESP.restart();
    } else {
      logger.println(F("Reset cancelled, resuming normal startup..."));
    }
    
  }

  logger.println(F("Connecting to WiFi..."));

  // our custom parameters
  ESPAsync_wifiManager.addParameter(&custom_hostname);
  ESPAsync_wifiManager.addParameter(&custom_port);
  ESPAsync_wifiManager.addParameter(&custom_ntpserver);

  //ESPAsync_wifiManager.setAPStaticIPConfig(IPAddress(192,168,30,254), IPAddress(192,168,30,1), IPAddress(255,255,255,0));
  ESPAsync_wifiManager.autoConnect("KRATECH-AP");
  if (WiFi.status() != WL_CONNECTED) { 
    logger.println(F("Failed to connect, will restart"));
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

  WiFi.setAutoReconnect(true);

  //Serial.printf("Connected! SSID: %s, key: %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
  Serial.printf("Connected! SSID: %s, key: %s\n", ESPAsync_wifiManager.getStoredWiFiSSID().c_str(), ESPAsync_wifiManager.getStoredWiFiPass().c_str());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  logger.print(F("Configuring OTA..."));
  Setup::OTA();
  if(!DEBUG) delay(700);
  
  logger.print(F("Starting Blynk..."));
  if(ConnectBlynk()) {
    logger.println(F("OK - Connected to Blynk server"));
  } else {
    logger.println(F("FAILED to connect to Blynk server!"));
  }

  logger.print(F("Starting MDNS..."));
  if (MDNS.begin(config.hostname)) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("FAILED"));
  }

  logger.print(F("Configuring NTP server "));
  logger.println(config.ntpserver);
  NTPClient timeClient = NTPClient(ntpUDP, config.ntpserver, 3600, 10);
  timeClient.begin();
  if(!DEBUG) delay(700);
  if(!timeClient.update()){
    logger.println(F("Failed to obtain time"));
  } else {
    logger.println(SecondsToDateTimeString(timeClient.getEpochTime(), TFMT_DATETIME));
  }  
  if(!DEBUG) delay(700);

  logger.print(F("Starting HTTP server..."));

  if(Setup::WebServer()) {
    Serial.println(F("OK"));
    MDNS.addService("http", "tcp", 80);
    Serial.printf("HTTP server ready: http://%s.local:%s (MDNS/Bonjour)\n", config.hostname, config.port);
  } else {
    Serial.println(F("FAILED"));
  }
  Serial.println(F("OK"));
  
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
  //tft.fillScreen(TFT_BLACK);
  LCD_state.clear = 1;

  timeClient.forceUpdate();
}

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

bool ConnectBlynk() {
  Serial.print(F("Connecting to Blynk servers (timeout 10 sec), token="));
  Serial.println(STR(BLYNK_TOKEN));
  Blynk.config(STR(BLYNK_TOKEN));  // in place of Blynk.begin(auth, ssid, pass);
  Blynk.connect(500);  // timeout set to 10 seconds and then continue without Blynk
  //while (Blynk.connect() == false) {
    // Wait until connected
  //}
  if(Blynk.connected()) {
    return true;
  } else {
    return false;
  }

}

void ReconnectWiFi() {  

  reconnects_wifi++;
  uint8_t cnt = 0;

  ESPAsync_WiFiManager ESPAsync_wifiManager(&server, &dnsServer);

  if(WiFi.isConnected()) {
    return;
  }
    
  Serial.print(F("Reconnecting to "));
  Serial.print(ESPAsync_wifiManager.getStoredWiFiSSID()); Serial.print("/"); Serial.println(ESPAsync_wifiManager.getStoredWiFiPass());
  ESPAsync_wifiManager.autoConnect();

  while (WiFi.status() != WL_CONNECTED) {  
    if(cnt++ > 6) {
      Serial.println(F("Failed, giving up!"));
      return;
    }
    delay(500);  
    Serial.print(".");
  }  
  Serial.println(F("Reconnected OK!"));
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
    //return data_json.getMember("firmware").as<String>();
    return String(ADC_VERSION);
  }
  else if (var == "AUTHOR_TEXT"){
    return String(AUTHOR_TEXT);
  }

  return String();
}

//WifiManager callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void loop(void) {
  double t = 0;
  //char data_string[JSON_SIZE] = "";
  //const char* data_string_ptr = data_string;
  bool pushToBlynk = Timers[TM_PushToBlynk]->expired();

  ArduinoOTA.handle();
  Blynk.run();
  CheckButtons();

  if(shouldReboot){
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }

  if(interruptCounter > 0) {
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
  }


  if(Timers[TM_SyncNTP]->expired()) {
    timeClient.update();
  }

  if(pushToBlynk) {
    Blynk.virtualWrite(V2, digitalRead(PIN_LED_1));
  }

  if(Timers[TM_CheckConnections]->expired()) {
    if (!WiFi.isConnected())
    {
        delay(5000);
        if(!WiFi.isConnected()) {
          Serial.print(F("Trying WiFi reconnect #"));
          Serial.println(reconnects_wifi);
          ReconnectWiFi();
          
          if (reconnects_wifi == 20)
          {
            Serial.println(F("Too many failures, rebooting..."));          
            //reconnects_wifi = 0;
            ESP.restart();
            return;
          }
        }
    } else {
      Serial.println(F("WiFi OK"));
/*
      if(getLocalTime(&timeinfo)){
        Serial.println(F("NTP update OK..."));
      } else {
        Serial.println(F("NTP update FAILED"));
      }
*/
      if(!Blynk.connected()) {
        Serial.println(F("Blynk disconnected, reconnecting..."));
        ConnectBlynk();
      }
    }

  }

  if(Serial_DATA.available())
  {
        //Serial_DATA.readBytesUntil('\n', data_string, sizeof(data_string));
        char buffer_rx[JSON_SIZE] = {0};
        Serial_DATA.readBytesUntil('\n', buffer_rx, sizeof(buffer_rx));
        if(strlen(buffer_rx) > 10) {
          queue_rx.unshift(buffer_rx);
        }
  }
    
  // Iterate over the RX queue and update JSON_DOCS, FIFO style

  while (!queue_rx.isEmpty()) {
    const char *data_string = queue_rx.pop();
    bool doSerialDebug = Timers[TM_SerialDebug]->expired();
    dataAge = millis();

    if(doSerialDebug) {
      Serial.printf("RX (%s):\n", SecondsToDateTimeString(timeClient.getEpochTime(), TFMT_DATETIME));
      Serial.println(data_string); 
    } else {
      Serial.print(".");
    }
    
    
    DynamicJsonDocument tmp_json(JSON_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)
    
    //DeserializationError error = deserializeJson(tmp_json, buffer_datain); // writeable (zero-copy method)
    //DeserializationError error = deserializeJson(tmp_json, data_string_ptr); // read-only input (duplication)
    DeserializationError error = deserializeJson(tmp_json, data_string); // read-only input (duplication)
    
    if (error) {
        Serial.print("\n");
        Serial.print(data_string);
        Serial.print(F("^JSON_ERR"));
        Serial.println(error.f_str());
    } else {

      JsonVariant jsonVal;
      
      jsonVal = tmp_json.getMember("cmd");
      uint8_t cmd = jsonVal.as<uint8_t>();

      //Serial.printf("cmd=%u\n", cmd);

      switch(cmd) {
        case 0x10: // ADCSYSDATA          
  
          serializeJson(tmp_json, JSON_STRINGS[JSON_DOC_ADCSYSDATA]);

          snprintf(ADC_VERSION, 6, "%s", (const char*)tmp_json.getMember("firmware"));      
        break;
        case 0x11: // ADCEMONDATA
          serializeJson(tmp_json, JSON_STRINGS[JSON_DOC_ADCEMONDATA]);

        break;        
        case 0x12: // ADCWATERPUMPDATA
   
          serializeJson(tmp_json, JSON_STRINGS[JSON_DOC_ADCWATERPUMPDATA]);

          // TESTING - send some circuit data to Blynk
          if(pushToBlynk) {
            Blynk.virtualWrite(V0, tmp_json.getMember("pressure_bar").as<float>());
            Blynk.virtualWrite(V1, tmp_json.getMember("temp_c").as<float>());      
          }
        break;        
        case 0x45: // REMOTE_SENSOR_DATA
        break;
        default: Serial.printf("Unknown CMD in JSON: %u", cmd);

      } // switch

    } // not error

  } // RX


  //----------- Update OLED display ---------------

  if(Timers[TM_ClearDisplay]->expired() || LCD_state.clear) {
      LCD_state.clear = false;
      tft.fillScreen(LCD_state.bgcolor);
      //Serial.println("ISR or told to CLEAR LCD");
  }

  // Automatically return to first menu page after set timeout
  if(Timers[TM_MenuReturn]->expired()) {
    menu_page_current = 0;
    LCD_state.clear = true;
  }

  if(Timers[TM_UpdateDisplay]->expired()) {

    // First we parse the JSON strings into objects so we can easily access the data
    
    DynamicJsonDocument JSON_DOCS[JSON_DOC_COUNT] = DynamicJsonDocument(JSON_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)
  
    for(int x=0; x<JSON_DOC_COUNT; x++) {
      
      const char * p = JSON_STRINGS[x];
      DeserializationError error = deserializeJson(JSON_DOCS[x], p); // read-only input (duplication)
      JSON_DOCS[x].shrinkToFit();
      if (error) {
        Serial.print("\n");
        Serial.print(JSON_STRINGS[x]);
        Serial.print(F("^JSON_ERR [TFT]:"));
        Serial.println(error.f_str());

        tft.setTextSize(3);
        tft.setCursor(0, 0);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println(" INVALID DATA   \n" );
        tft.println(" JSON ERROR    \n" );
        tft.printf("%lu\n", millis() );
        delay(500);
        return;        
      }
    }

    switch(menu_page_current)
    {
      case MENU_PAGE_UTILITY_STATS:
      {
        tft.setTextSize(txtsize);

        if(LCD_state.bgcolor != TFT_BLACK) {
          LCD_state.bgcolor = TFT_BLACK;
          tft.fillScreen(LCD_state.bgcolor);
        }

        // ------- LINE 1/8: -------
        LCD_state.fgcolor = TFT_GOLD;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
        tft.setCursor(0, 0);
        tft.printf("      SUMMARY       \n");
        LCD_state.fgcolor = TFT_GREEN;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);                
        // ------- LINE 2/8: -------
        const char* status = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("WP").getMember("status").as<char*>();
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
        // ------- LINE 3/8: -------
        tft.printf("WP:" );
        tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("pressure_bar").as<float>();      
        tft.printf("%d.%02d bar, ", (int)t, (int)(t*100)%100 );
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        // ------- LINE 4/8: -------
        tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("temp_c").as<float>();
        tft.printf("%d.%01d %cC\n", (int)t, (int)(t*10)%10, (char)247 );
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        // ------- LINE 5/8: -------
        t = JSON_DOCS[JSON_DOC_ADCEMONDATA].getMember("emon_vrms_L_N").as<float>();
        tft.printf("V(rms) L-N:  ");
        tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
        tft.printf("%d.%01d V\n", (int)t, (int)(t*10)%10);
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        // ------- LINE 6/8: -------
        t = JSON_DOCS[JSON_DOC_ADCEMONDATA].getMember("emon_vrms_L_PE").as<float>();
        tft.printf("V(rms) L-PE: ");
        tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
        tft.printf("%d.%01d V\n", (int)t, (int)(t*10)%10);
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        // ------- LINE 7/8: -------
        t = JSON_DOCS[JSON_DOC_ADCEMONDATA].getMember("emon_vrms_N_PE").as<float>();
        tft.printf("V(rms) N-PE: ");
        tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
        tft.printf("%d.%01d V\n", (int)t, (int)(t*10)%10);
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        // ------- LINE 8/8: -------
        JsonObject obj_circuits = JSON_DOCS[JSON_DOC_ADCEMONDATA].getMember("circuits").getMember("1"); // 1=Main Intake
        t = obj_circuits.getMember("I").as<float>();
        uint16_t power = obj_circuits.getMember("P_a").as<uint16_t>();
        tft.printf("Power: "); // "Power: 10000W/28.9A"
        tft.setTextColor(TFT_WHITE, LCD_state.bgcolor);
        tft.printf("%uW %d.%01dA \n", power, (int)t, (int)(t*10)%10);
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);

        JsonArray alarms = JSON_DOCS[JSON_DOC_ADCSYSDATA].getMember("alarms");
        if(alarms.size() == 0) {
          //tft.printf("     No alarms      ");
          tft.printf("Last A: %s          ", JSON_DOCS[JSON_DOC_ADCSYSDATA].getMember("lastAlarm").as<char*>());
        } else {
          tft.setTextColor(TFT_RED, TFT_WHITE);
      
          for(uint8_t x=0; x< alarms.size(); x++) {
            const char* str = alarms[x];
            tft.printf("A:%s ", str );
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

        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("pressure_bar").as<float>();      
        tft.printf("Pressure: %d.%02d bar\n", (int)t, (int)(t*100)%100 );

        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("temp_c").as<float>();
        tft.printf("Room: %d.%01d %cC, %d%%\n", (int)t, (int)(t*10)%10, (char)247, JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("hum_room_pct").as<uint8_t>() );

        t = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("temp_motor_c").as<float>();
        tft.printf("Motor: %d.%01d %cC  \n", (int)t, (int)(t*10)%10, (char)247 );

        tft.printf("Starts/stops: %u  \n", JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("WP").getMember("cnt_starts").as<uint16_t>());

        const char * state = JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("WP").getMember("status").as<char *>();
        tft.printf("%s: %s\n", state, SecondsToDateTimeString(JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("WP").getMember("t_state").as<uint16_t>(), TFMT_HOURS));

        tft.printf("Susp tot: %s\n", SecondsToDateTimeString(JSON_DOCS[JSON_DOC_ADCWATERPUMPDATA].getMember("WP").getMember("t_susp_tot").as<uint16_t>(), TFMT_HOURS));

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
        tft.printf("%-14s\n", SecondsToDateTimeString(timeClient.getEpochTime(), TFMT_DATETIME));
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
        tft.printf(" ADC-MCU: v%s\n",  JSON_DOCS[JSON_DOC_ADCSYSDATA].getMember("firmware").as<String>().c_str());
        tft.printf(" Web-IF:  v%s\n", WEBIF_VERSION); //L5
        tft.printf("   (c) %s    \n", AUTHOR_COPYRIGHT ); //L6
        tft.print(F(" Ken-Roger Andersen \n") );
        tft.print(F("ken.roger@gmail.com \n") );
        
        tft.setTextWrap(true);
      break;
      case MENU_PAGE_LOGO:
        tft.pushImage(0, 0,  240, 135, kra_tech);
      break;

      default: menu_page_current = 0;
    } // switch()

    if(tm_CheckDataAge.expired()) {

      if(millis() - dataAge > 10000 && dataAge > 0L) {
              
        tft.setTextSize(3);
        tft.setCursor(0, 0);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("  NO DATA RX " );
      }    
    }    
  } // tm_UpdateDisplay.expired()
} // loop()

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