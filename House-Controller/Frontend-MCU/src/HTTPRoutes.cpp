#include "Arduino.h"
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"

#include <WiFi.h>

//#include <ESPAsyncWebServer.h>
//#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager

#include <ESPmDNS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <HTTPRoutes.h>
//#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager

//const char* recoveryHTML = "<h1>FS RECOVERY</h1>Upload firmware.bin: <form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

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
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/x-icon");
  });
  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.json", "application/json");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.on("/jquery.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/jquery.js", "application/x-javascript");
  });
  server.on("/update.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/update.js", "application/x-javascript");
  });
  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.js", "application/x-javascript");
  });
  server.on("/nosleep.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/nosleep.min.js", "application/x-javascript");
  });
  server.on("/messages.log", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/messages.log", "text/plain");
  });

  // TEST
  server.on("/config2.json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config2.json", "application/json");
  });


  // GET command routes

  // cmd 0x10 = ADCSYSDATA (from ADC-MCU)
  server.on("/json/0x10", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", JSON_STRINGS[JSON_DOC_ADCSYSDATA]);
  });

  // cmd 0x11 = ADCEMONDATA (from ADC-MCU)
  server.on("/json/0x11", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", JSON_STRINGS[JSON_DOC_ADCEMONDATA]);
  });

  // cmd 0x11 = ADCWATERPUMPDATA (from ADC-MCU)
  server.on("/json/0x12", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", JSON_STRINGS[JSON_DOC_ADCWATERPUMPDATA]);
  });

  // cmd 0x45 = REMOTE_PROBES (return JSON array with last 0x45 json lines (content from circular buffer) from remote probes)
  server.on("/json/0x45", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("application/json");

    response->print("[");

		for (uint8_t i = 0; i < MAX_REMOTE_SIDS; i++) {
      if(remote_data2[i][0] == '\0' || strlen(remote_data2[i]) < 2) {
        continue;
      } else {
        response->print(remote_data2[i]);
        if( i != 0) {
          response->print(",\n");
        }
      }
		}

    // Print our own system data/sid0
    char sid[150] = {0};
    uint64_t chipid = ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
    sprintf(sid, "{\"cmd\":%u,\"devid\":%u,\"sid\":0,\"data\":{\"firmware\":\"%s\",\"IP\":\"%s\",\"port\":%u,\"uptime_sec\":%lu,\"rssi\":%i}}", 0x45, (uint16_t)(chipid>>32), FIRMWARE_VERSION, WiFi.localIP().toString().c_str(), 80, millis()/1000, WiFi.RSSI());
    response->print(sid);

    response->print("]");
    response->print("\r\n\r\n");
    
    //size_t cs = sizeof(remote_data) * JSON_SIZE_REMOTEPROBES;
    //response->setContentLength(cs);
    request->send(response);
    
  });

  server.on("/json/frontend", HTTP_GET, [](AsyncWebServerRequest *request){
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
    //json["localtime"] = SecondsToDateTimeString(timeClient.getEpochTime(), TFMT_DATETIME);
    //json["localtime"] 
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

  
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if(SPIFFS.exists("/update.html")) {
      request->send(SPIFFS, "/update.html", "text/html", false, HTMLProcessor);
    } else {
      // FS is formatted/update.html missing, send a Simple Firmware Update Form html so we can upload new filesystem
      request->send_P(200, "text/html", PSTR("<h1>FS RECOVERY</h1>Upload firmware.bin: <form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"));    
    }
    
  });

  // for testing/dev
  server.on("/recover", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", PSTR("<h1>FS RECOVERY</h1>Upload firmware.bin: <form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"));    
  });

  // Generic file upload form recovery/fallback (for upload of config.json)
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.printf("/upload GET\n");
      request->send_P(200, "text/html", PSTR("<body><div><form method='post' action='/upload'><input type='file'><button>Send</button></form></div></body>"));
    });  


  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", shouldReboot?"<html><head><body><h1>OK</h1>stand by while rebooting... <a href='/'>Home</a></body></html>":"<html><head></head><body>FAIL</body></html>");
    response->addHeader("Connection", "close");
    request->send(response);
    delay(2000);
    ESP.restart();
    
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
        
        //Update.runAsync(true);
        
        // if filename includes spiffs, update the spiffs partition
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
  Serial.printf("onNotFound: HTTP/404 - %s,%s\n", request->url().c_str(), request->client()->remoteIP().toString().c_str());
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
  Serial.println(F("onBody"));
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
  Serial.println(F("onUpload"));



    bool is_config_json = (filename.indexOf("config.json") >= 0); 
    if(!index){      
      Serial.printf("UploadStart: %s\n",filename.c_str());
      // open the file on first call and store the file handle in the request object
      request->_tempFile = SPIFFS.open("/"+filename, "w");
    }
    if(len) {
      // stream the incoming chunk to the opened file
      request->_tempFile.write(data,len);
    }
    if(final){
      Serial.printf("UploadEnd: %s,size:%u\n", filename.c_str(), (index+len));
      // close the file handle as the upload is now done
      request->_tempFile.close();
      //request->redirect("/");
      
      if(is_config_json) {
        Serial.printf("is_config_json=true, close conn\n");
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", is_config_json?"<html><head><body><h1>Configuration uploaded OK</h1>stand by while rebooting... <a href='/'>Home</a></body></html>":"<html><head></head><body>FAIL</body></html>");
        response->addHeader("Connection", "close");
        request->send(response);
        
        delay(2000);
        //ESP.restart();
      } else {
        delay(2000);
        //request->redirect("/");
      }
    }


}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  //Handle WebSocket event
  Serial.println(F("onEvent"));
}