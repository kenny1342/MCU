#include "Arduino.h"
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"

#include <WiFi.h>

//#include <ESPAsyncWebServer.h>
//#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager

#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <HTTPRoutes.h>
//#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager

Webserver::Webserver()
{
    this->error = false;
}

void Webserver::AddRoutes() {

    // upload a file to /upload
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, onUpload);

  // Route for root / web page with variable parser/processor
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html", false, HTMLProcessor);
  });
  
  // File routes
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/jquery.min.js", "application/x-javascript");
  });
  server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/main.js", "application/x-javascript");
  });

  // GET command routes

  // return data fields from json as requested in /sensordata?fieldname
  server.on("/sensordata", HTTP_GET, [](AsyncWebServerRequest *request){
    int paramsNr = request->params();
    if(paramsNr == 0) {
      request->send(200, "text/plain", "no param");
      return;
    }

    for(int i=0;i<paramsNr;i++){
    
        AsyncWebParameter* p = request->getParam(i);
        //Serial.print(p->name()); Serial.print("="); Serial.println(p->value());
      if(p->name() == "wp_t_state") {
        request->send(200, "text/plain", data_json["wp_t_state"]);
        return;    
      }

      if(data_json.getMember(p->name()).isNull() ) {
        request->send(200, "text/plain", String(p->name()) +  F(" not found in json"));
        return;    
      } else {
        request->send(200, "text/plain", data_json.getMember(p->name()) );
        return;
      }

    }    
    request->send(200, "text/plain", F("Unknown parameter"));
  });

  server.on("/json/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "application/json", json_output);
    
    //String output = "";
    char output[512] = { 0 };
    serializeJson(data_json, output);
    request->send(200, "application/json", output);
  });

  
  server.on("/json/system", HTTP_GET, [](AsyncWebServerRequest *request){
    String output;
    StaticJsonDocument<150> doc;
    JsonObject root = doc.to<JsonObject>();
    // Add vital data from our self (ESP chip)
    JsonObject json = root.createNestedObject("ESP");
    json["hostname"] = config.hostname;
    json["heap"] = ESP.getFreeHeap();
    json["freq"] = ESP.getCpuFreqMHz();
    uint64_t chipid = ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
    json["chipid"] = (uint16_t)(chipid>>32); //use High 2 bytes
    json["uptimesecs"] = millis() /1000;
    
    serializeJson(root, output);
    request->send(200, "application/json", output);    
  });
  
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    
    //ESPAsync_WiFiManager ESPAsync_wifiManager(&server,&dnsServer);
    request->send(200, "text/plain", "resetting...");
    delay(1000);
    //ESPAsync_wifiManager.resetSettings();
    File file = SPIFFS.open("/doreset.dat", "w"); // we check for this file on boot
    if(file.print("RESET") == 0) {
      Serial.println(F("SPIFFS ERROR writing flagfile /doreset.dat"));
    } else {
      Serial.println(F("wrote flagfile /doreset.dat"));
    }
    file.close();
    delay(500);
    WiFi.disconnect(true, true);  // will erase ssid/password on some platforms (not esp32)
    WiFi.begin("0","0");       // adding this effectively seems to erase the previous stored SSID/PW

    ESP.restart();
    delay(5000);
  
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "rebooting...");
    delay(1000);
    ESP.restart();
    delay(5000);  
  });

// Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    //const char* updateHTML = "Upload firmware.bin: <form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
    //request->send(200, "text/html", updateHTML);
    request->send(SPIFFS, "/update.html", "text/html", false, HTMLProcessor);
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", shouldReboot?"<html><head><body><h1>OK</h1>stand by while rebooting... <a href='/'>Home</a></body></html>":"<html><head></head><body>FAIL</body></html>");
    response->addHeader("Connection", "close");
    request->send(response);
    delay(800);
    ESP.restart();
    
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
        
        //Update.runAsync(true);
        
        // if filename includes littlefs, update the littlefs partition
        int cmd = (filename.indexOf("spiffs") >= 0) ? U_SPIFFS : U_FLASH; 
        


        if(cmd == U_FLASH) {
            Serial.printf("Firmware Update Start: %s\n", filename.c_str());
            uint32_t freespace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if(!Update.begin(freespace, cmd)) {
                Update.printError(Serial);
            }
        } else {
            Serial.printf("File System Update Start: %s\n", filename.c_str());
            //size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
            //close_all_fs();
            size_t fsSize = SPIFFS.totalBytes();
            if (!Update.begin(fsSize, cmd)){//start with max available size
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
  server.serveStatic("/fs", SPIFFS, "/");


}

void onRequest(AsyncWebServerRequest *request){
  //Handle valid Request
  Serial.printf("HTTP: %s\n", request->url().c_str());
}

void onNotFound(AsyncWebServerRequest *request){
  //Handle Unknown Request
  Serial.printf("onNotFound: HTTP/404 - %s\n", request->url().c_str());
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
  Serial.println(F("onBody"));
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
  Serial.println(F("onUpload"));
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  //Handle WebSocket event
  Serial.println(F("onEvent"));
}