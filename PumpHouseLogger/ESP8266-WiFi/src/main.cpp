#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#ifndef STASSID
#define STASSID "kra-stb"
#define STAPSK  "escort82"
#endif
/*
#include "SoftwareSerial.h"

#ifndef D5
#if defined(ESP8266)
#define D5 (14)
#define D6 (12)
#elif defined(ESP32)
#define D5 (18)
#define D6 (19)
#endif
#endif

SoftwareSerial swSer1;
*/


const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

const int led = 13;

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello, from esp8266!");
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
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
  digitalWrite(led, 0);
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
/*
	swSer1.begin(38400, SWSERIAL_8N1, D6, D6, false, 256);
	// high speed half duplex, turn off interrupts during tx
	swSer1.enableIntTx(false);
*/

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("init... v3");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  MDNS.update();

  // just read hw serial RX patched to UNO sw serial TX and resend it on hw TX for debug
  // TODO: parse data string (temperature for now), put in a string and service it via HTTP
  if (Serial.available())
    Serial.write(Serial.read());
}
