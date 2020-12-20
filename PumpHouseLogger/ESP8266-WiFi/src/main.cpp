/**
 * This is code on ESP8266 WiFi module on the board, it reads a JSON encoded string from serial and makes it available via HTTP http://my.ip/json
 * We also parse out the temperature value (celsius) and display it as html on the web root (/)
 * 
 * Connect TX/RX to the controller MCU's (Uno) SW serial pins
 * 
 * Kenny Dec 19, 2020
 */
#include <ESP8266WiFi.h>
#include <main.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <BlynkSimpleEsp8266.h>

const int sw_rst = PIN_SW_RST;
const int led_1 = PIN_LED_1;

int app_button1 = 0;

const char auth[] = BLYNK_TOKEN;
char json_output[200];
char data_string[200];

char celsius[4];
char pressure_bar[4];
StaticJsonDocument<200> data_json;
uint32_t previousMillis = 0; 
uint32_t previousMillis_200 = 0; 

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
  // Request the latest state from the server
  Blynk.syncVirtual(V2);
}

// When App button is pushed - switch the state
BLYNK_WRITE(V2) {
  app_button1 = param.asInt();
  digitalWrite(led_1, app_button1);
  //digitalWrite(led_1, !digitalRead(led_1));
}

ESP8266WebServer server(80);

void setup(void) {
  pinMode(led_1, OUTPUT);
  digitalWrite(led_1, 0);
  pinMode(sw_rst, INPUT); // reset button

  Serial.begin(115200);

  Serial.println("");
  Serial.println(F("PumpHouseLogger v1.0, Kenny 2020"));
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
  if(digitalRead(sw_rst) == LOW) {
    Serial.println(F("reset button pressed, keep pressed for 5 sec to reset all settings!"));
    delay(5000);
    if(digitalRead(sw_rst) == LOW) {
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
  //here  "AutoConnectAP" with password "password"
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

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  ConnectBlynk();

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

  server.begin();
  Serial.println("HTTP server started");
}


void ConnectBlynk() {
  Serial.print(F("Connecting to Blynk servers (timeout 10 sec), token="));
  Serial.println(auth);
  Blynk.config(auth);  // in place of Blynk.begin(auth, ssid, pass);
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

void handleRoot() {
  digitalWrite(led_1, 1);

  String message = "<html><head></head><body>\n";
  message += "<h1>PumpHouseLogger</h1><br />";
  message += "Using LM335 on ADC0<br /><br />\n";
  message += "Temperature: <b>";
  message += celsius;
  message += " &deg;C</b><br />\n";
  message += "Pressure: <b>";
  message += pressure_bar;
  message += " Bar</b><br /><br />\n";
  message += "Output as JSON: <a href='/json'>/json</a><br />\n";
  message += "Reset WiFi settings: (will reboot as AP)<a href='/reset'>/reset</a><br />\n";
  message += "Uptime (secs): ";
  message += millis()/1000;
  message += "<br /><hr />Made by Ken-Roger Andersen, Dec 2020";
/*
message += "<br />data_string: ";
message += data_string;
message += "<br />json_output: ";
message += json_output;

message += "<br />d: ";
const char *d1 = json_output;
message += d1;

message += "<br />d2: ";
const char *d2 = data_string;
message += d2;
message += "<br />String(json_output): ";
message += String(json_output);
*/
  server.send(200, "text/html", message);
  digitalWrite(led_1, 0);
}

void handleJSON() {
  digitalWrite(led_1, 1);
  server.send(200, "application/json", json_output);
  digitalWrite(led_1, 0);
}

void handleNotFound() {
  digitalWrite(led_1, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led_1, 0);
}

void loop(void) {

  server.handleClient();
  MDNS.update();
  Blynk.run();
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis_200 >= 200L) { // update Blynk every 200ms
    Blynk.virtualWrite(V2, digitalRead(led_1));
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



    Blynk.virtualWrite(V0, pressure_bar);
    Blynk.virtualWrite(V1, celsius);
    
  }

}
