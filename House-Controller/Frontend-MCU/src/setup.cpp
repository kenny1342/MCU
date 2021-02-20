#include "Arduino.h"
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"

#include <WiFi.h>

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPRoutes.h>
#include <setup.h>


Config config;                         // <- global configuration object

const char *Setup::_configfile = "/config.json";

Setup::Setup()
{
    this->error = false;
}

void Setup::deleteAllCredentials(void) {

}

bool Setup::GetConfig() {

  // Load defaults
  strlcpy(config.hostname, CONF_DEF_HOSTNAME, sizeof(config.hostname));
  strlcpy(config.port, CONF_DEF_PORT, sizeof(config.port));    
  strlcpy(config.ntpserver, CONF_DEF_NTP_SERVER, sizeof(config.ntpserver));    
  strlcpy(config.ntp_interval, CONF_DEF_NTP_INTERVAL, sizeof(config.ntp_interval));


  if (SPIFFS.exists(_configfile)) {
      File configFile = SPIFFS.open(_configfile, "r");
      if (configFile) {

        DynamicJsonDocument doc(2048);

        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, configFile);
        if (error) {
            Serial.println(F("FAILED - JSON data invalid, using default configuration"));
            Serial.println(error.f_str());
        } else {
            Serial.println("OK");
        }
        // Copy values from the JsonDocument to the Config
        //config.port = doc["port"] | "80";
        if(doc["hostname"] && strlen(doc["hostname"]) > 0) {
            strlcpy(config.hostname, doc["hostname"], sizeof(config.hostname));
        }
        if(doc["port"] && strlen(doc["port"]) > 0) {
            strlcpy(config.port, doc["port"], sizeof(config.port));
        }

      } else {
          Serial.println("FAILED\ncould not open /config.json"); 
          return false;   
      }    
  } else {
      Serial.println("FAILED\n/config.json not found");
      return false;
  }

  return true;
}

bool Setup::SaveConfig() {

  File configFile = SPIFFS.open(_configfile, "w");
  if (!configFile) {
      Serial.println("FAILED");
      Serial.println("failed to open /config.json for writing");
      return false;
  }

  StaticJsonDocument<256> doc;

  // Set the values in the document
  doc["hostname"] = config.hostname;
  doc["port"] = config.port;

  // Serialize JSON to file
  if (serializeJson(doc, configFile) == 0) {
      Serial.println("FAILED");
      Serial.println(F("serializeJson() failed to write to file"));
  }

  configFile.close();
  //end save
  
  return true;
}


bool Setup::FileSystem()
{
  if (SPIFFS.begin(true)){
      Serial.println(F("Success"));
  }else{
      Serial.println(F("Failed! Rebooting in 10 sec..."));
      delay(10000);
      ESP.restart();
      return false; 
  }

  // To format all space in LittleFS
  // LittleFS.format()

  // Get all information of your LittleFS
  // TODO: move to separate function
  /*
  FSInfo fs_info;
  SPIFFS.info(fs_info);
*/
  Serial.println("Fetching file system info...");
  Serial.print("Total space:      ");
  Serial.print(SPIFFS.totalBytes());
  Serial.println("byte");
  Serial.print("Total space used: ");
  Serial.print(SPIFFS.usedBytes());
  Serial.println("byte");
  /*Serial.print("Block size:       ");
  Serial.print(fs_info.blockSize);
  Serial.println("byte");
  Serial.print("Page size:        ");
  Serial.print(fs_info.totalBytes);
  Serial.println("byte");
  Serial.print("Max open files:   ");
  Serial.println(fs_info.maxOpenFiles);
  Serial.print("Max path lenght:  ");
  Serial.println(fs_info.maxPathLength);
*/
  Serial.println("\nFS init completed");

  return true;
}

bool Setup::WebServer()
{
    
  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  // attach AsyncEventSource
  server.addHandler(&events);

  Webserver::AddRoutes();

  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.

  server.onNotFound(onNotFound);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();

  return true;
}

void Setup::OTA()
{
  ArduinoOTA.setPort(3232);
  // ArduinoOTA.setHostname("myesp32"); // defaults to esp3232-[MAC]
  ArduinoOTA
    .onStart([]() {
      String type;
      OTArunning = true;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      OTArunning = false;
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      OTArunning = false;
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request

  ArduinoOTA.begin();

}