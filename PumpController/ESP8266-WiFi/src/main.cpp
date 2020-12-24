/**
 * This is code on ESP8266 WiFi module on the board, it reads a JSON encoded string from serial and makes it available via HTTP http://my.ip/json
 * We also parse out the temperature value (celsius) and display it as html on the web root (/)
 * 
 * Connect TX/RX to the controller MCU's (Uno) SW serial pins
 * 
 * Kenny Dec 19, 2020
 */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <BlynkSimpleEsp8266.h>
#include <ArduinoOTA.h>
#include <main.h>

// store long global string in flash (put the pointers to PROGMEM)
const char FIRMWARE_VERSION_LONG[] PROGMEM = "PumpController (MCU ESP8266-WiFi) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;
const char* hostname = HOSTNAME;

const char* updateHTML = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

int blynk_button_V2 = 0;

//const char auth[] = BLYNK_TOKEN;
char json_output[200];
char data_string[200];

char celsius[4];
char pressure_bar[4];
StaticJsonDocument<200> data_json;
uint32_t previousMillis = 0; 
uint32_t previousMillis_200 = 0; 
uint16_t reconnects_wifi = 0;

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

ESP8266WebServer server(80);

void setup(void) {
  pinMode(PIN_LED_1, OUTPUT);
  digitalWrite(PIN_LED_1, 0);
  pinMode(PIN_SW_RST, INPUT); // reset button

  Serial.begin(115200);

  Serial.println("");
  Serial.println("initializing...");
  Serial.println(FPSTR(FIRMWARE_VERSION_LONG));
  Serial.println("Made by Ken-Roger Andersen, 2020");
  Serial.println("");
  Serial.println(F("To reset, press button for 5+ secs while powering on"));
  Serial.println("");
  delay(1000);
  Serial.println(F("Initializing WiFi..."));

  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //exit after config instead of connecting
  wifiManager.setBreakAfterConfig(true);

  //reset settings if button pressed
  if(digitalRead(PIN_SW_RST) == LOW) {
    Serial.println(F("reset button pressed, keep pressed for 5 sec to reset all settings!"));
    delay(5000);
    if(digitalRead(PIN_SW_RST) == LOW) {
      Serial.println(F("reset button still pressed, all settings reset to default!"));
      delay(1000);
      wifiManager.resetSettings();

    } else {
      Serial.println(F("reset aborted! resuming normal bootup"));
    }
    
  }
  
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("PW:");
  Serial.println(WiFi.psk());

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("KRATECH-AP", "password")) {
    Serial.println(F("failed to connect, we should reset as see if it connects"));
    delay(3000);
    ESP.reset();
    delay(5000);
  }
 
  Serial.println("connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  ConnectBlynk();

  if (MDNS.begin(hostname)) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/json", handleJSON);

  server.on("/reset", []() {
    WiFiManager wifiManager;
    server.send(200, "text/plain", "resetting...");
    delay(3000);
    wifiManager.resetSettings();
    delay(100);
    ESP.reset();
    delay(5000);
  });

  server.onNotFound(handleNotFound);

  // ************ HTTP OTA UPDATE *************  
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updateHTML);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      WiFiUDP::stopAll();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });  

  server.begin();
  MDNS.addService("http", "tcp", 80);

  Serial.printf("HTTP server ready: http://%s.local (MDNS/Bonjour)\n", hostname);


  // ************ ARDUINO OTA CONFIG *************'
  // https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html
  Serial.println(F("Configuring OTA (port=8266)"));
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
      
}

void ConnectBlynk() {
  Serial.print(F("Connecting to Blynk servers (timeout 10 sec), token="));
  Serial.println(STR(BLYNK_TOKEN));
  Blynk.config(STR(BLYNK_TOKEN));  // in place of Blynk.begin(auth, ssid, pass);
  Blynk.connect(10000U);  // timeout set to 10 seconds and then continue without Blynk
  while (Blynk.connect() == false) {
    // Wait until connected
  }
  if(Blynk.connected()) {
    Serial.println(F("Connected to Blynk server"));
  } else {
    Serial.println(F("FAILED to connect to Blynk server!"));
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


void loop(void) {
  unsigned long currentMillis = millis();
  int wifitries = 0;

  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();
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
  if (readline(Serial.read(), data_string, 200) > 0) {
    digitalWrite(PIN_LED_1, 1);
    const char* json_input = data_string;  
    // Deserialize the JSON document (zero-copy method)
    DeserializationError error = deserializeJson(data_json, json_input);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      memcpy(json_output, data_string, sizeof(data_string[0])*200);
      Serial.print("JSON rcvd: ");
      Serial.println(json_output);    

      dtostrf(data_json["temp_c"][0].as<float>(), 3, 1, celsius);
      dtostrf(data_json["pressure_bar"][0].as<float>(), 3, 1, pressure_bar);
    }
    
    // Push new values to Blynk server
    Blynk.virtualWrite(V0, pressure_bar);
    Blynk.virtualWrite(V1, celsius);
    digitalWrite(PIN_LED_1, 0);
  }

}
