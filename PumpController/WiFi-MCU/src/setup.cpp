
#include <HTTPRoutes.h>
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


  if (SPIFFS.exists(_configfile)) {
      File configFile = SPIFFS.open(_configfile, "r");
      if (configFile) {

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