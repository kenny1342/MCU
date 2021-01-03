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
#include "bmp.h"
#include <setup.h>

#define _ESPASYNC_WIFIMGR_LOGLEVEL_    4 // Use from 0 to 4. Higher number, more debugging messages and memory usage.
#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager
#include <main.h>


// store long global string in flash (put the pointers to PROGMEM)
const char FIRMWARE_VERSION_LONG[] PROGMEM = "PumpController (MCU ESP32-WiFi) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;

const char* _def_hostname = HOSTNAME;
const char* _def_port = PORT;

char data_string[JSON_SIZE];
bool shouldReboot = false;      //flag to use from web firmware update to reboot the ESP
bool shouldSaveConfig = false;  //WifiManger callback flag for saving data

int blynk_button_V2 = 0;
//char logline[160] = "";
uint8_t lcd_page_curr = 0;
LCD_state_struct LCD_state;

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

//volatile ISRFLAGS_struct ISRFLAGS;
volatile int interruptCounter;
//int totalInterruptCounter;
  //static uint16_t t2_overflow_cnt;//, t2_overflow_cnt_500ms;
Intervals_struct intervals;


hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
/**
 * Timer ISR 10ms
 */
void IRAM_ATTR onTimer() {

  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++; // will rollover, that's fine
  //intervals._1min = 1;
  //intervals.allBits = 0x00; // reset all
  //TODO: optimize (remember to declare intervals as static), modulus is quite slow, but it is easy to read and ok for now
  intervals._1min = !(interruptCounter % 6000);
  intervals._1sec = !(interruptCounter % 100);
  intervals._500ms = !(interruptCounter % 50);
  intervals._200ms = !(interruptCounter % 20);
  intervals._100ms = !(interruptCounter % 10);

  portEXIT_CRITICAL_ISR(&timerMux);
  
  //if(intervals._1min) ISRFLAGS.lcd_clear = true;
}
void setup(void) {
  
  pinMode(PIN_LED_1, OUTPUT);
  digitalWrite(PIN_LED_1, 0);
  pinMode(PIN_SW_UP, INPUT); // reset + menu page button
  pinMode(PIN_SW_DOWN, INPUT); // menu page button

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
  tft.pushImage(0, 0,  240, 135, ttgo);
  delay(2000);

  tft.fillScreen(TFT_RED);
  delay(300);
  tft.fillScreen(TFT_BLUE);
  delay(300);
  tft.fillScreen(TFT_GREEN);
  delay(300);

  tft.setTextWrap(LCD_state.textwrap);

  tft.setTextSize(3);
  tft.fillScreen(LCD_state.bgcolor);
  tft.setCursor(0 ,0);

  tft.println("\n\n   Bootup...  ");
  tft.setTextSize(txtsize);
  delay(700);

  logger.println(F("Starting 1min timer ISR..."));
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true); // 1000us=1ms, 1000000us=1s
  //timerAlarmWrite(timer, 6000000, true); // 6000000us=60sec
  timerAlarmEnable(timer);


  logger.println(F("Starting FS (SPIFFS)..."));
  Setup::FileSystem();

  delay(700);
  logger.print(F("Loading configuration from /config.json..."));
  Setup::GetConfig();

  delay(700);
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

  //WiFiManager custom parameters/config
  ESPAsync_WMParameter custom_hostname("hostname", "Hostname", config.hostname, 64);
  ESPAsync_WMParameter custom_port("port", "HTTP port", config.port, 6);

  //Local intialization. Once setup() done, there is no need to keep it around
  ESPAsync_WiFiManager ESPAsync_wifiManager(&server, &dnsServer, "KRATECH-WEB");

  ESPAsync_wifiManager.setDebugOutput(true);
  //ESPAsync_wifiManager.setConnectTimeout(10);

  ESPAsync_wifiManager.setBreakAfterConfig(true); // exit after config

  // reset settings if flagfile exists
  if(SPIFFS.exists("/doreset.dat")) {
    logger.println("reset flagfile /doreset.dat found, resetting WiFi and rebooting as AP...");
    SPIFFS.remove("/doreset.dat");

    delay(500);
    ESPAsync_wifiManager.resetSettings();  // esp8266, not esp32
    delay(500);
    delay(5000);
    ESP.restart();

  } else {
    logger.println("reset flagfile /doreset.dat not found, continue");
  }

  //reset settings if button pressed
  if(digitalRead(PIN_SW_UP) == LOW) {
    logger.println(F("reset button pressed, keep pressed for 5 sec to reset all settings!"));
    delay(5000);
    if(digitalRead(PIN_SW_UP) == LOW) {
      logger.println(F("will reboot and reset settings"));
      delay(500);
      //ESP.restart();
    } else {
      logger.println(F("reset aborted! resuming startup..."));
    }
    
  }

  //set config save notify callback
  ESPAsync_wifiManager.setSaveConfigCallback(saveConfigCallback);
  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,30,254), IPAddress(192,168,30,1), IPAddress(255,255,255,0));

  // our custom parameters
  ESPAsync_wifiManager.addParameter(&custom_hostname);
  ESPAsync_wifiManager.addParameter(&custom_port);

  //ESPAsync_wifiManager.setAPStaticIPConfig(IPAddress(192,168,30,254), IPAddress(192,168,30,1), IPAddress(255,255,255,0));
  ESPAsync_wifiManager.autoConnect("KRATECH-AP");
  if (WiFi.status() != WL_CONNECTED) { 
    logger.println(F("failed to connect, we should reset as see if it connects"));
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


  //Serial.printf("Connected! SSID: %s, key: %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
  Serial.print("Connected! SSID: "); Serial.print(WiFi.SSID().c_str()); Serial.print(", psk "); Serial.println(WiFi.psk().c_str());
  //Serial.printf("Connected! SSID: %s, key: %s\n", ESPAsync_wifiManager.getStoredWiFiSSID().c_str(), ESPAsync_wifiManager.getStoredWiFiPass().c_str());
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
  
  logger.println(F("Setup completed! Waiting for RX..."));
  
  delay(2000);
  tft.fillScreen(TFT_BLACK);
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

    if(WiFi.SSID().length() < 2 || WiFi.psk().length() < 2) {
      Serial.println(F("invalid SSID/psk"));
      return;
    }
    Serial.print("Reconnecting to ");
    Serial.print(WiFi.SSID());
    WiFi.mode(WIFI_STA);  
    WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());  
    
    while (WiFi.status() != WL_CONNECTED) {  
        if(cnt++ > 6) {
          Serial.println(F("Failed, giving up!"));
          return;
        }
        delay(500);  
        Serial.print(".");
        digitalWrite(PIN_LED_1, !digitalRead(PIN_LED_1));
    }  
    Serial.println(F("Connected!"));
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
  return String();
}

//WifiManager callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void loop(void) {
  unsigned long currentMillis = millis();
  int wifitries = 0;
  double t = 0;

  Blynk.run();
  CheckButtons();

  if(shouldReboot){
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }

  if(intervals._500ms) {
      portENTER_CRITICAL(&timerMux);
      intervals._500ms = 0;
      portEXIT_CRITICAL(&timerMux);

    if(currentMillis - dataAge > 5000 && dataAge > 0L) {
            
      tft.setTextSize(3);
      tft.setCursor(0, 0);
      tft.setTextDatum(MC_DATUM);
      //tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("  NO DATA RX " );
  
    }

    // push state data to Blynk
    Blynk.virtualWrite(V2, digitalRead(PIN_LED_1));
    
  }

  if(intervals._1min) {
      portENTER_CRITICAL(&timerMux);
      intervals._1min = 0;
      portEXIT_CRITICAL(&timerMux);

      tft.fillScreen(LCD_state.bgcolor);
      //Serial.println("ISR CLEAR LCD");


    if ((WiFi.status() != WL_CONNECTED))
    {
        Serial.print(F("Trying to reconnect to WiFi - "));
        Serial.println(wifitries);
        ReconnectWiFi();
        wifitries++;

        if (wifitries == 10)
        {
          Serial.println(F("Too many failures, rebooting..."));
          ESP.restart();
          return;
        }
    } else {
      Serial.println(F("WiFi OK"));
    }

    if(!Blynk.connected()) {
      Serial.println(F("Blynk disconnected, reconnecting..."));
      ConnectBlynk();
    }
  }

  // read line (JSON) from hw serial RX (patched to UNO sw serial TX), then store it in data var, and resend it on hw TX for debug    
  //
  if (readline(Serial_DATA.read(), data_string, JSON_SIZE) > 0) {
    digitalWrite(PIN_LED_1, 1);
    
    dataAge = millis();
    Serial.print("rcvd: ");
    Serial.println(data_string); 

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

      if(lcd_page_curr == MENU_PAGE_UTILITY_STATS) {
        tft.setTextSize(txtsize);

        if(LCD_state.bgcolor != TFT_BLACK) {
          LCD_state.bgcolor = TFT_BLACK;
          tft.fillScreen(LCD_state.bgcolor);
        }

        LCD_state.fgcolor = TFT_GOLD;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        
        tft.setCursor(0, 0);
        tft.printf("    SUMMARY    \n");

        LCD_state.fgcolor = TFT_GREEN;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);        
        
        //tft.setCursor(0, TFT_LINE1);
        //tft.printf("SSID: %-14s", WiFi.SSID().c_str());

        tft.setCursor(0, TFT_LINE2);
        tft.printf("W Pump: ");
        if(data_json["WP"].getMember("is_running").as<uint8_t>() == 1) {
          tft.setTextColor(TFT_BLACK, TFT_GREEN); 
          tft.printf("   RUNNING  ");
        } else {
          tft.setTextColor(TFT_WHITE, TFT_RED); 
          tft.printf("     OFF    ");
        }
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor); 

        tft.setCursor(0, TFT_LINE3);
        tft.printf("%-16s", " ");
        
        tft.setCursor(0, TFT_LINE4);      
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
        tft.printf(" %cC", (char)247);
        // "WP: 1.11 Bar, 1.1 oC"

        tft.setCursor(0, TFT_LINE5);
        t = data_json["emon_vrms_L_N"].as<float>();
        tft.printf("V(rms) L-N:  %d.%01d V", (int)t, (int)(t*10)%10);
        
        tft.setCursor(0, TFT_LINE6);      
        t = data_json["emon_vrms_L_PE"].as<float>();
        tft.printf("V(rms) L-PE: %d.%01d V", (int)t, (int)(t*10)%10);
        
        tft.setCursor(0, TFT_LINE7);      
        t = data_json["emon_vrms_N_PE"].as<float>();
        tft.printf("V(rms) N-PE: %d.%01d V", (int)t, (int)(t*10)%10);
        // "V(rms) L-N:  250.0 V"
        
        
        JsonArray alarms = data_json.getMember("alarms");
        if(alarms.size() == 0) {
          tft.setCursor(0, TFT_LINE8);
          tft.printf("     No alarms      ");
        } else {
          tft.setTextColor(TFT_RED, TFT_WHITE);
          tft.setCursor(0, TFT_LINE8);
          tft.printf("%20s", " "); // clear line
          tft.setCursor(0, TFT_LINE8);
      
          for(uint8_t x=0; x< alarms.size(); x++) {
            const char* str = alarms[x];
            tft.printf("A:%s ", str );
          }
        }

      } else if(lcd_page_curr == MENU_PAGE_WATER_STATS) {

        tft.setTextSize(txtsize);

        if(LCD_state.bgcolor != TFT_BLACK) {
          LCD_state.bgcolor = TFT_BLACK;
          tft.fillScreen(LCD_state.bgcolor);
        }

        LCD_state.fgcolor = TFT_GOLD;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
        
        tft.setCursor(0, 0);
        tft.printf("    WATER SUPPLY    \n");

        LCD_state.fgcolor = TFT_LIGHTGREY;
        tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);


        t = data_json["pressure_bar"][0].as<float>();      
        tft.printf("Pressure: %d.%02d bar\n", (int)t, (int)(t*100)%100 );

        t = data_json["temp_c"][0].as<float>();
        tft.printf("Room temp: %d.%01d %cC\n", (int)t, (int)(t*10)%10, (char)247 );

        tft.printf("Start count: %u \n", data_json["WP"].getMember("cnt_starts").as<uint16_t>());

        const char * state = data_json["WP"].getMember("is_running").as<uint8_t>() ? "ON" : "OFF";
        /*
        if(data_json["WP"].getMember("is_running").as<uint8_t>() == 1) {
          sprintf(state, "ON");
        } else {
          sprintf(state, "OFF");
        }
        */
        tft.printf("%s: %s\n", state, TimeToString(data_json["WP"].getMember("t_state").as<uint16_t>()));

        const char * suspended = data_json["WP"].getMember("is_suspended").as<uint8_t>() ? "YES" : "NO";
        tft.printf("Susp: %s %s\n", suspended, TimeToString(data_json["WP"].getMember("t_susp").as<uint16_t>()));
        tft.printf("Susp tot: %s\n", TimeToString(data_json["WP"].getMember("t_susp_tot").as<uint16_t>()));

      } else if(lcd_page_curr == MENU_PAGE_SYSTEM_STATS) {

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


    digitalWrite(PIN_LED_1, 0);
  }

}

/**
 * Check button states, act accordingly
 */
void CheckButtons(void) {
  static uint32_t t_btn_up_pressed;
  static uint32_t t_btn_down_pressed;

  if(digitalRead(PIN_SW_UP) == LOW) {
    t_btn_up_pressed++;
  } else {

    if(t_btn_up_pressed > 5) { // button was pressed > 5 ms, goto next menu page index
      
      if(lcd_page_curr == MENU_PAGE_MAX) {
        lcd_page_curr = 0;
      } else {
        lcd_page_curr++;
      }
      LCD_state.clear = true;
    }

    t_btn_up_pressed = 0;
  }

  if(digitalRead(PIN_SW_DOWN) == LOW) {
    t_btn_down_pressed++;
  } else {

    if(t_btn_down_pressed > 5) { // button was pressed > 5 ms, goto previous menu page index
      
      if(lcd_page_curr == 0) {
        lcd_page_curr = MENU_PAGE_MAX;
      } else {
        lcd_page_curr--;
      }
      LCD_state.clear = true;
    }

    t_btn_down_pressed = 0;
  }

}
/*
void makeCStringSpaces(char str[], int _len)// Simple C string function
{
  //char spaces[] = "                ";     // 16 spaces
  //int end = strlen(str);
  //strcat(str, &spaces[end]);
  char bla[_len];

memset(bla, ' ', sizeof bla - 1);
bla[sizeof bla - 1] = '\0';
}
*/
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