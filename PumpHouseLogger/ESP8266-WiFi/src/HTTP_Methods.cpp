#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <main.h>

void handleRoot() {
  digitalWrite(PIN_LED_1, 1);

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
  message += "Reset WiFi settings (will reboot as AP): <a href='/reset'>/reset</a><br />\n";
  message += "Uptime (secs): ";
  message += millis()/1000;
  message += "<br />WiFi reconnects: ";
  message += reconnects_wifi;
  message += "<br /><hr />Made by Ken-Roger Andersen, Dec 2020";
  server.send(200, "text/html", message);
  digitalWrite(PIN_LED_1, 0);
}

void handleJSON() {
  digitalWrite(PIN_LED_1, 1);
  server.send(200, "application/json", json_output);
  digitalWrite(PIN_LED_1, 0);
}

void handleNotFound() {
  digitalWrite(PIN_LED_1, 1);
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
  digitalWrite(PIN_LED_1, 0);
}
