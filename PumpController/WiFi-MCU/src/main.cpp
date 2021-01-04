/**
 * This is code on ESP8266 WiFi module on the board, it reads a JSON encoded string from serial and makes it available via HTTP http://my.ip/json
 * We also parse out the temperature value (celsius) and display it as html on the web root (/)
 * 
 * Connect TX/RX to the controller MCU's (Uno) SW serial pins
 * 
 * Kenny Dec 19, 2020
 */
#include <Arduino.h>
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"
#include <WiFi.h>
//#include <Time.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
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

void WIFIconfigModeCallback (ESPAsync_WiFiManager *myWiFiManager);

const char* _def_hostname = HOSTNAME;
const char* _def_port = PORT;

char data_string[JSON_SIZE];
bool shouldReboot = false;      //flag to use from web firmware update to reboot the ESP
bool shouldSaveConfig = false;  //WifiManger callback flag for saving data

int blynk_button_V2 = 0;
//char logline[160] = "";
uint8_t menu_page_current = 0;
LCD_state_struct LCD_state;
MENUPAGES_t MenuPages;

Button BtnUP(PIN_SW_UP, 25U, false, true); // This pin has wired pullup on TTGO T-Display board
Button BtnDOWN(PIN_SW_DOWN, 25U, true, true); // No pullup on this pin, enable internal
const uint16_t LONG_PRESS(1000);           // we define a "long press" to be 1000 milliseconds.

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
StaticJsonDocument<JSON_SIZE> data_json;
uint32_t previousMillis = 0; 
uint32_t previousMillis_200 = 0; 
uint32_t dataAge = 0;
uint16_t reconnects_wifi = 0;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

Logger logger = Logger(&tft);

Timemark tm_ClearDisplay(600000); // 600sec=10min
Timemark tm_CheckConnections(120000); 
Timemark tm_CheckDataAge(1000); // 1sec 
Timemark tm_PushToBlynk(500);
Timemark tm_SerialDebug(10000);

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
  
  Serial_DATA.begin(115200, SERIAL_8N1, PIN_RXD2, PIN_TXD2);
  Serial_DATA.println("WiFi-MCU booting up...");

  Serial.begin(115200);

  Serial.println("");
  Serial.println("initializing...");
  Serial.println(FPSTR(FIRMWARE_VERSION_LONG));
  Serial.println("Made by Ken-Roger Andersen, 2020");
  logger.println("");
  logger.println(F("To reset, press button for 5+ secs while powering on"));
  logger.println("");
  delay(1000);


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
  delay(10000);

  tft.fillScreen(TFT_RED);
  delay(300);
  tft.fillScreen(TFT_BLUE);
  delay(300);
  tft.fillScreen(TFT_GREEN);
  delay(300);

  tft.setTextWrap(true);

  tft.setTextSize(3);
  tft.fillScreen(LCD_state.bgcolor);
  tft.setCursor(0 ,0);

  tft.println("\n\n   Starting...  ");
  tft.setTextSize(txtsize);
  delay(700);

  logger.println(F("Starting FS (SPIFFS)..."));
  Setup::FileSystem();

  delay(700);
  logger.print(F("Loading configuration from /config.json..."));
  Setup::GetConfig();

  delay(500);
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
    } 
  }

  WiFi.setAutoReconnect(true);

  //Serial.printf("Connected! SSID: %s, key: %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
  Serial.printf("Connected! SSID: %s, key: %s\n", ESPAsync_wifiManager.getStoredWiFiSSID().c_str(), ESPAsync_wifiManager.getStoredWiFiPass().c_str());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  

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
  //timerAlarmWrite(timer, 6000000, true); // 6000000us=60sec
  timerAlarmEnable(timer);

  tm_ClearDisplay.start();
  tm_CheckConnections.start();
  tm_CheckDataAge.start();
  tm_PushToBlynk.start();
  tm_SerialDebug.start();

  logger.println(F("Setup completed! Waiting for data..."));
  
  delay(2000);
  tft.setTextWrap(false);
  tft.fillScreen(TFT_BLACK);
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
  if (var == "TEMPERATURE"){
    return String(data_json["temp_c"][0].as<String>());
  }
  else if (var == "WATERPRESSURE"){
    return String(data_json["pressure_bar"][0].as<String>() );
  }
  else if (var == "UPTIMESECS"){
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
  unsigned long currentMillis = millis();
  //int wifitries = 0;
  double t = 0;

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

  if(tm_CheckDataAge.expired()) {

    if(currentMillis - dataAge > 5000 && dataAge > 0L) {
            
      tft.setTextSize(3);
      tft.setCursor(0, 0);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("  NO DATA RX " );
    }    
  }

  if(tm_PushToBlynk.expired()) {
    Blynk.virtualWrite(V2, digitalRead(PIN_LED_1));
  }

  if(tm_ClearDisplay.expired()) {
      tft.fillScreen(LCD_state.bgcolor);
      //Serial.println("ISR CLEAR LCD");
  }

  if(tm_CheckConnections.expired()) {
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
            reconnects_wifi = 0; //TODO: comment
            ESP.restart(); //TODO: uncomment
            return;
          }
        }
    } else {
      Serial.println(F("WiFi OK"));

      if(!Blynk.connected()) {
        Serial.println(F("Blynk disconnected, reconnecting..."));
        ConnectBlynk();
      }
    }

  }

  // read line (JSON) from hw serial RX (patched to UNO sw serial TX), then store it in data var, and resend it on hw TX for debug    
  //
  if (readline(Serial_DATA.read(), data_string, JSON_SIZE) > 0) {
    bool doSerialDebug = tm_SerialDebug.expired();
    dataAge = millis();

    if(doSerialDebug) {
      Serial.print("rcvd: ");
      Serial.println(data_string); 
    } else {
      Serial.print(".");
    }

    // Deserialize the JSON document (zero-copy method)
    DeserializationError error = deserializeJson(data_json, data_string);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      // Extract some values and push to Blynk server
      JsonVariant jsonVal;
      
      jsonVal = data_json.getMember("pressure_bar");
      if(!jsonVal.isNull()) {
        Blynk.virtualWrite(V0, jsonVal[0].as<float>());
      }      
      jsonVal = data_json.getMember("temp_c");
      if(!jsonVal.isNull()) {
        Blynk.virtualWrite(V1, jsonVal[0].as<float>());
      }      
      jsonVal = data_json.getMember("K2");
      if(!jsonVal.isNull()) {
        Blynk.virtualWrite(V3, jsonVal["I"].as<float>());
        Blynk.virtualWrite(V4, jsonVal["P_a"].as<long>());
      }

      Blynk.virtualWrite(V0, data_json["pressure_bar"][0].as<float>());
      Blynk.virtualWrite(V1, data_json["temp_c"][0].as<float>());      

    } // RX


    //----------- Update OLED display ---------------
    if(LCD_state.clear) {
      LCD_state.clear = false;
      tft.fillScreen(LCD_state.bgcolor);
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

        LCD_state.fgcolor = TFT_GOLD;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        
        tft.setCursor(0, 0);
        tft.printf("       SUMMARY      \n");

        LCD_state.fgcolor = TFT_GREEN;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
        
        //tft.setCursor(0, TFT_LINE2);
        tft.printf("W Pump: ");
        if(data_json["WP"].getMember("is_running").as<uint8_t>() == 1) {
          tft.setTextColor(TFT_WHITE, TFT_GREEN); 
          tft.printf("   RUNNING  ");
        } else {
          tft.setTextColor(TFT_WHITE, TFT_RED); 
          tft.printf("     OFF    ");
        }
        tft.printf("\n");
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor); 

        tft.printf("%-16s\n", " "); // EMPTY line
        
        tft.printf("WP:" );
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        t = data_json["pressure_bar"][0].as<float>();      
        tft.printf("%d.%02d", (int)t, (int)(t*100)%100 );
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        tft.printf(" bar, ");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        t = data_json["temp_c"][0].as<float>();
        tft.printf("%d.%01d", (int)t, (int)(t*10)%10 );
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        tft.printf(" %cC\n", (char)247);
                
        t = data_json["emon_vrms_L_N"].as<float>();
        tft.printf("V(rms) L-N:  %d.%01d V\n", (int)t, (int)(t*10)%10);
                
        t = data_json["emon_vrms_L_PE"].as<float>();
        tft.printf("V(rms) L-PE: %d.%01d V\n", (int)t, (int)(t*10)%10);

        t = data_json["emon_vrms_N_PE"].as<float>();
        tft.printf("V(rms) N-PE: %d.%01d V\n", (int)t, (int)(t*10)%10);        
        
        JsonArray alarms = data_json.getMember("alarms");
        if(alarms.size() == 0) {
          tft.printf("     No alarms      ");
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

        t = data_json["pressure_bar"][0].as<float>();      
        tft.printf("Pressure: %d.%02d bar\n", (int)t, (int)(t*100)%100 );

        t = data_json["temp_c"][0].as<float>();
        tft.printf("Room temp: %d.%01d %cC\n", (int)t, (int)(t*10)%10, (char)247 );

        tft.printf("Start count: %u \n", data_json["WP"].getMember("cnt_starts").as<uint16_t>());

        const char * state = data_json["WP"].getMember("is_running").as<uint8_t>() ? "ON" : "OFF";
        tft.printf("%s: %s\n", state, TimeToString(data_json["WP"].getMember("t_state").as<uint16_t>()));

        const char * suspended = data_json["WP"].getMember("is_suspended").as<uint8_t>() ? "YES" : "NO";
        tft.printf("Susp: %s %s\n", suspended, TimeToString(data_json["WP"].getMember("t_susp").as<uint16_t>()));

        tft.printf("Susp tot: %s\n", TimeToString(data_json["WP"].getMember("t_susp_tot").as<uint16_t>()));
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
        tft.printf("SSID: %-14s", WiFi.SSID().c_str());
        tft.printf("IP: %-16s", WiFi.localIP().toString().c_str());
        tft.printf("WiFi reconnects: %u\n", reconnects_wifi);
        tft.printf("Free mem: %u B\n", ESP.getFreeHeap());
        tft.printf("CPU freq: %u Mhz\n", ESP.getCpuFreqMHz());
        tft.printf("ID: %llu\n", ESP.getEfuseMac());
        tft.setTextWrap(false);
        tft.printf("SDK: %s\n", ESP.getSdkVersion());
        tft.setTextWrap(true);
        //tft.printf("PROG: %u B\n", ESP.getSketchSize());
        tft.printf("Uptime: %s\n", TimeToString(millis()/1000));
      }
      break;
      case MENU_PAGE_ABOUT:
        tft.setTextSize(txtsize);
        tft.setTextWrap(false);
        if(LCD_state.bgcolor != TFT_DARKGREEN) {
          LCD_state.bgcolor = TFT_DARKGREEN;
          tft.fillScreen(LCD_state.bgcolor);
        }
        tft.setCursor(0, 0);
        LCD_state.fgcolor = TFT_GOLD;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
        
        tft.printf("  House-Controller  \n");        
        LCD_state.fgcolor = TFT_ORANGE;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);

        tft.printf("\nWeb-MCU version: %s\n", FIRMWARE_VERSION); //L3
        tft.printf("ADC-MCU version: %s\n",  data_json.getMember("firmware").as<String>().c_str());
        tft.printf("Web-IF version: %s\n", WEBIF_VERSION); //L5
        tft.printf("   (c) %s    \n", AUTHOR_COPYRIGHT ); //L6
        tft.print(F("Ken-Roger Andersen  \n") );
        tft.print(F("ken.roger@gmail.com \n") );
        
        tft.setTextWrap(true);
      break;
      case MENU_PAGE_LOGO:
        tft.pushImage(0, 0,  240, 135, kra_tech);
      break;

      default: menu_page_current = 0;

    }
  }

}

/**
 * Check button states
 */
void CheckButtons(void) {
  //volatile static uint32_t t_btn_up_pressed;
  //volatile static uint32_t t_btn_down_pressed;
  BtnDOWN.read();
  BtnUP.read();

  //if(digitalRead(PIN_SW_UP) == LOW) {
  //  t_btn_up_pressed++;
  //} else {


   
    //if(t_btn_up_pressed > 10) { // button was pressed > x ms, goto next menu page index
      //t_btn_up_pressed = 0;  
    if(BtnUP.wasReleased()) {

      if(menu_page_current == MENU_PAGE_MAX) {
        menu_page_current = 0;
      } else {
        menu_page_current++;
      }
      LCD_state.clear = true;
    } else if(BtnUP.pressedFor(LONG_PRESS)) {
      Serial.println("BTNUP LONG PRESS");
      //WiFi.disconnect();
      delay(5000);
    }
  //}



  if(BtnDOWN.wasReleased()) {
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

// t is time in seconds = millis()/1000;
char * TimeToString(unsigned long t)
{
 static char str[12];
 long h = t / 3600;
 t = t % 3600;
 int m = t / 60;
 int s = t % 60;
 sprintf(str, "%04ld:%02d:%02d", h, m, s);
 return str;
}