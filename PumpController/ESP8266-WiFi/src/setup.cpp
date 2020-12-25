#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "LittleFS.h" // LittleFS is declared

#if defined(ESP8266)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <HTTP_Methods.h>
#include <setup.h>


Config config;                         // <- global configuration object

const char *Setup::_configfile = "/config.json";

Setup::Setup()
{
    this->error = false;
}

bool Setup::GetConfig() {

    // Load defaults
    strlcpy(config.hostname, _def_hostname, sizeof(config.hostname));
    strlcpy(config.port, "80", sizeof(config.port));    


    if (LittleFS.exists("/config.json")) {
        //file exists, reading and loading
        //Serial.println("reading config file");
        File configFile = LittleFS.open(_configfile, "r");
        //File configFile = LittleFS.open("/config.json", "r");
        if (configFile) {
            //Serial.println("opened config file");
            //size_t size = configFile.size();

            StaticJsonDocument<512> doc;

            // Deserialize the JSON document
            DeserializationError error = deserializeJson(doc, configFile);
            if (error) {
                Serial.println(F("FAILED - JSON data not found, using default configuration"));
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

    File configFile = LittleFS.open("/config.json", "w");
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
    if (LittleFS.begin()){
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
    FSInfo fs_info;
    LittleFS.info(fs_info);

    Serial.println("Fetching file system info...");
    Serial.print("Total space:      ");
    Serial.print(fs_info.totalBytes);
    Serial.println("byte");
    Serial.print("Total space used: ");
    Serial.print(fs_info.usedBytes);
    Serial.println("byte");
    Serial.print("Block size:       ");
    Serial.print(fs_info.blockSize);
    Serial.println("byte");
    Serial.print("Page size:        ");
    Serial.print(fs_info.totalBytes);
    Serial.println("byte");
    Serial.print("Max open files:   ");
    Serial.println(fs_info.maxOpenFiles);
    Serial.print("Max path lenght:  ");
    Serial.println(fs_info.maxPathLength);

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

    // upload a file to /upload
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, onUpload);

  // Route for root / web page with variable parser/processor
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html", false, HTMLProcessor);
  });
  
  // File routes
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", json_output);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", celsius);
  });
  server.on("/waterpressure", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", pressure_bar);
  });
  server.on("/uptimesecs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String( millis() /1000) );
  });

  // GET command routes
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    
    AsyncWiFiManager wifiManager(&server,&dns);
    request->send(200, "text/plain", "resetting...");
    delay(1000);
    wifiManager.resetSettings();
    delay(100);
    ESP.reset();
    delay(5000);
  
  });

// Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    //const char* updateHTML = "Upload firmware.bin: <form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
    //request->send(200, "text/html", updateHTML);
    request->send(LittleFS, "/update.html", "text/html", false, HTMLProcessor);
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", shouldReboot?"<html><head><body><h1>OK</h1>stand by while rebooting... <a href='/'>Home</a></body></html>":"<html><head></head><body>FAIL</body></html>");
    response->addHeader("Connection", "close");
    request->send(response);
    
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
        
        Update.runAsync(true);
        
        // if filename includes littlefs, update the littlefs partition
        int cmd = (filename.indexOf("littlefs") >= 0) ? U_FS : U_FLASH; 
        
        if(cmd == U_FLASH) {
            Serial.printf("Firmware Update Start: %s\n", filename.c_str());
            uint32_t freespace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if(!Update.begin(freespace, cmd)) {
                Update.printError(Serial);
            }
        } else {
            Serial.printf("File System Update Start: %s\n", filename.c_str());
            size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
            close_all_fs();
            if (!Update.begin(fsSize, U_FS)){//start with max available size
            Update.printError(Serial);
            }
        }
      
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
    }
  });

  // attach filesystem root at URL /fs
  server.serveStatic("/fs", LittleFS, "/");

  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  
  server.onNotFound(onNotFound);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();

    return true;
}