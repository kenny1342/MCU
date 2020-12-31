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

#if defined(ESP8266)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <ArduinoOTA.h>

#include <main.h>

// store long global string in flash (put the pointers to PROGMEM)
const char FIRMWARE_VERSION_LONG[] PROGMEM = "PumpController (MCU ESP32-WiFi) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;

const char* _def_hostname = HOSTNAME;
const char* _def_port = PORT;

char data_string[JSON_SIZE];
bool shouldReboot = false;      //flag to use from web firmware update to reboot the ESP
bool shouldSaveConfig = false;  //WifiManger callback flag for saving data

int blynk_button_V2 = 0;


StaticJsonDocument<JSON_SIZE> data_json;
uint32_t previousMillis = 0; 
uint32_t previousMillis_200 = 0; 
uint16_t reconnects_wifi = 0;

DNSServer dns;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

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
  pinMode(PIN_SW_RST, INPUT); // reset button

  Serial_DATA.begin(115200, SERIAL_8N1, PIN_RXD2, PIN_TXD2);
  Serial_DATA.println("WiFi-MCU booting up...");

  Serial.begin(115200);

  Serial.println("");
  Serial.println("initializing...");
  Serial.println(FPSTR(FIRMWARE_VERSION_LONG));
  Serial.println("Made by Ken-Roger Andersen, 2020");
  Serial.println("");
  Serial.println(F("To reset, press button for 5+ secs while powering on"));
  Serial.println("");
  delay(1000);

  
  Serial.print(F("Starting FS (SPIFFS)..."));
  Setup::FileSystem();

  Serial.print(F("Loading configuration from /config.json..."));
  Setup::GetConfig();

  Serial.println(F("Starting WiFi..."));

/*
  //WiFiManager custom parameters/config
  AsyncWiFiManagerParameter custom_hostname("hostname", "Hostname", config.hostname, 64);
  AsyncWiFiManagerParameter custom_port("port", "HTTP port", config.port, 6);

  //Local intialization. Once setup() done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server,&dns);

  Serial.printf("SSID: %s, key: %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());

  //reset settings if button pressed
  if(digitalRead(PIN_SW_RST) == HIGH) {
    Serial.println(F("reset button pressed, keep pressed for 5 sec to reset all settings!"));
    delay(5000);
    if(digitalRead(PIN_SW_RST) == HIGH) {
      Serial.println(F("reset button still pressed, all settings reset to default!"));
      delay(1000);
      wifiManager.resetSettings();

    } else {
      Serial.println(F("reset aborted! resuming startup..."));
    }
    
  }

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,30,254), IPAddress(192,168,30,1), IPAddress(255,255,255,0));

  // our custom parameters
  wifiManager.addParameter(&custom_hostname);
  wifiManager.addParameter(&custom_port);

  wifiManager.setBreakAfterConfig(true); // exit after config

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("KRATECH-AP", "password")) {
    Serial.println(F("failed to connect, we should reset as see if it connects"));
    delay(3000);
    ESP.restart();
    delay(5000);
  }
 */

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin("kra-stb", "escort82");
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {

  } else {
    Serial.println("connection failed!");
    delay(1000);
    ESP.restart();
  }

  Serial.println("connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
/*
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println(F("Saving config..."));
    if(Setup::SaveConfig()) {
      Serial.println("OK");

      //read updated parameters
      strcpy(config.hostname, custom_hostname.getValue());
      strcpy(config.port, custom_port.getValue());
    } 
  }
*/

  Serial.print(F("Starting Blynk..."));
  if(ConnectBlynk()) {
    Serial.println(F("OK - Connected to Blynk server"));
  } else {
    Serial.println(F("FAILED to connect to Blynk server!"));
  }

  Serial.print(F("Starting MDNS..."));
  if (MDNS.begin(config.hostname)) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("FAILED"));
  }

  Serial.print(F("Starting HTTP server..."));

  if(Setup::WebServer()) {
    Serial.println(F("OK"));
    MDNS.addService("http", "tcp", 80);
    Serial.printf("HTTP server ready: http://%s.local:%s (MDNS/Bonjour)\n", config.hostname, config.port);
  } else {
    Serial.println(F("FAILED"));
  }
  Serial.println(F("OK"));
  

  Serial.println(F("Setup completed!"));
}

bool ConnectBlynk() {
  Serial.print(F("Connecting to Blynk servers (timeout 10 sec), token="));
  Serial.println(STR(BLYNK_TOKEN));
  Blynk.config(STR(BLYNK_TOKEN));  // in place of Blynk.begin(auth, ssid, pass);
  Blynk.connect(500);  // timeout set to 10 seconds and then continue without Blynk
  while (Blynk.connect() == false) {
    // Wait until connected
  }
  if(Blynk.connected()) {
    return true;
  } else {
    return false;
  }

}

void ReconnectWiFi() {  

    reconnects_wifi++;
    Serial.print("Reconnecting to ");
    Serial.print(WiFi.SSID());
    WiFi.mode(WIFI_STA);  
    WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());  
    while (WiFi.status() != WL_CONNECTED) {  
        delay(500);  
        Serial.print(".");
        digitalWrite(PIN_LED_1, !digitalRead(PIN_LED_1));
    }  
    Serial.println("Connected!");
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
  
  if(shouldReboot){
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }

  ArduinoOTA.handle();

  //MDNS.update();
  Blynk.run();

  while ((WiFi.status() != WL_CONNECTED))
  {
      ReconnectWiFi();
      wifitries++;
      delay(1000);

      if (wifitries == 5)
      {
          return;
      }
  }

  if (currentMillis - previousMillis_200 >= 200L) { // update Blynk every 200ms
    Blynk.virtualWrite(V2, digitalRead(PIN_LED_1));
    previousMillis_200 = currentMillis;
  }

  if (currentMillis - previousMillis >= 60000L) { // once a minute, check Blynk connection
    previousMillis = currentMillis;

    if(!Blynk.connected()) {
      Serial.println(F("Blynk disconnected, reconnecting..."));
      ConnectBlynk();
    }
  }

  // read line (JSON) from hw serial RX (patched to UNO sw serial TX), then store it in data var, and resend it on hw TX for debug    
  //
  if (readline(Serial_DATA.read(), data_string, JSON_SIZE) > 0) {
    digitalWrite(PIN_LED_1, 1);

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
    }

    digitalWrite(PIN_LED_1, 0);
  }

}
