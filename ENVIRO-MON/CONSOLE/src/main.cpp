/**
 * ESP32 WiFi <-> 3x UART + SPI Bridge
 * WiFi sensors connects to this AP. Their (JSON) data is received via TCP and forwarded out on SPI and Serial.
 * No data/JSON manipulation takes place in this module, just a simple transparent bridge.
 * Currently ADC-MCU is SPI slave and listens only for SPI, Serial not used.
 */

#include <Arduino.h>

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"
#include "main.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <ArduinoOTA.h> 
#include <SPI.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Timemark.h>
#include <esp_task_wdt.h>
#include <CircularBuffer.h>
//#include <MD_CirQueue.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
//#include <SimpleMap.h>

#include <TFT_eSPI.h>
#include <logo_kra-tech.h>

//8 seconds WDT
#define WDT_TIMEOUT 8

bool OTArunning = false;
bool printDebug = true;
uint8_t client_count = 0;

uint32_t stat_bytes_rx;
uint16_t stat_conn_count;
uint16_t last_conn_count;
uint8_t reconnects_wifi;



// Ring buffer with all chars received via TCP, to be written to serial
CircularBuffer<char, 2048> queue_tx; // chars we queue (3 probes accumulates ~400 chars between each SendData every 5 sec)
//CircularBuffer<char, 2048> datastore; // chars we queue (3 probes accumulates ~400 chars between each SendData every 5 sec)
//#define QUEUE_RX_SIZE 255
#define BUF_SIZE 800
//MD_CirQueue Q_rx(BUF_SIZE, sizeof(char)); // temp storage reading tcp client
//MD_CirQueue Q_rx(QUEUE_RX_SIZE, sizeof(char)*BUF_SIZE); // temp storage reading tcp client
char buffer[BUF_SIZE] = {0}; // permanent storage, updated from Q_rx
//uint16_t buffer_pos = 0; // last position in buffer
//CircularBuffer<char, 512> Q_rx;
char data_string[BUF_SIZE] = {0};
#define MAX_REMOTE_SIDS 8
char remote_data2[MAX_REMOTE_SIDS][BUF_SIZE];
LCD_state_struct LCD_state;

AsyncWebServer webserver(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

Timemark tm_CheckWifi(5000);
Timemark tm_CheckRX(600000);
Timemark tm_SendData(1000);
Timemark tm_reboot(86400 * 1000); 
Timemark tm_ClearDisplay(300000);
Timemark tm_UpdateDisplay(200);
Timemark tm_printDebug(3000);

HardwareSerial Serial_one(1);
WiFiServer server(SERIAL1_TCP_PORT);
WiFiClient *clients[MAX_CLIENTS] = { NULL };
TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

CLIENT_struct clientdata[MAX_CLIENTS];

void setup() {
  delay(500);

  Serial.begin(115200);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial_one.begin(UART_BAUD1, SERIAL_PARAM1, SERIAL1_RXPIN, SERIAL1_TXPIN);

  Serial.println("\n\nSensor-HUP WiFi serial bridge V" VERSION);

  //Q_rx.setFullOverwrite(true);
  //Q_rx.begin();

  tft.init();
  tft.setRotation(3); // 1
  tft.fillScreen(LCD_state.bgcolor);
  //tft.setTextSize(2);
  tft.setTextColor(LCD_state.fgcolor);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);

  tft.setSwapBytes(true);
  //tft.pushImage(0, 0,  240, 135, ttgo);
  tft.pushImage(0, 0,  240, 135, kra_tech);
  delay(10000);

  tft.fillScreen(TFT_RED);
  delay(300);
  tft.fillScreen(TFT_BLUE);
  delay(300);
  tft.fillScreen(TFT_GREEN);
  delay(300);

  tft.setTextWrap(true);

  tft.setTextSize(3);
  tft.fillScreen(LCD_state.bgcolor);
  tft.setCursor(0 ,0);

  tft.println("\n\nStarting HUB");
  tft.setTextSize(txtsize);
  delay(700);

  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  Serial.println(F("Starting FS (SPIFFS)..."));
    if (SPIFFS.begin(true)){
      Serial.println(F("Success"));
  }else{
      Serial.println(F("Failed! Rebooting in 10 sec..."));
      delay(10000);
      ESP.restart();
      return; 
  }
  // To format all space in LittleFS
  // LittleFS.format()
  Serial.println("Fetching file system info...");
  Serial.print("Total space:      ");
  Serial.print(SPIFFS.totalBytes());
  Serial.println("byte");
  Serial.print("Total space used: ");
  Serial.print(SPIFFS.usedBytes());
  Serial.println("byte");
  Serial.println("\nFS init completed");



  #ifdef MODE_AP 
   if(printDebug) Serial.println("Open ESP Access Point mode");
  //AP mode (phone connects directly to ESP) (no router)

  
  WiFi.mode(WIFI_AP);
   
  WiFi.softAP(ssid, pw); // configure ssid and password for softAP
  delay(2000); // VERY IMPORTANT
  WiFi.softAPConfig(ip, ip, netmask); // configure ip address for softAP

  Serial.printf("AP SSID:%s, key:%s, IP:%s\n", ssid, pw, ip.toString().c_str());
  #endif


  #ifdef MODE_STA
   if(printDebug) Serial.println("Open ESP Station mode");
  
  WiFi.setAutoReconnect(true);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pw);
  if(printDebug) Serial.print("try to Connect to Wireless network: ");
  if(printDebug) Serial.println(ssid);
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED) {  
    esp_task_wdt_reset(); 
    delay(500);
    if(++tries > 20) break;
    if(printDebug) Serial.print(".");
    
  }
  
  Serial.print("\nWiFi connected with IP: ");
  Serial.println(WiFi.localIP());
   
  #endif

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
      esp_task_wdt_reset();
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

  server.begin();
  
   Serial.println("starting mDNS as 'SENSORHUB'");
  if(!MDNS.begin("SENSORHUB")) {
     Serial.println("Error starting mDNS");
     delay(10000);
     ESP.restart();
     return;
 }
 MDNS.addService("rs232-1", "tcp", 8880);
 
 Serial.print(F("Starting HTTP server..."));
   // attach AsyncWebSocket
  ws.onEvent(onEvent); 
  webserver.addHandler(&ws);
  // attach AsyncEventSource
  webserver.addHandler(&events);

      webserver.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, onUpload);

  // Route for root / web page with variable parser/processor
  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html", false, HTMLProcessor);
  });
  
  // File routes
  webserver.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/x-icon");
  });

  webserver.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.json", "application/json");
  });
  webserver.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  webserver.on("/jquery.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/jquery.js", "application/x-javascript");
  });
  webserver.on("/update.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/update.js", "application/x-javascript");
  });
  webserver.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.js", "application/x-javascript");
  });
  webserver.on("/nosleep.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/nosleep.min.js", "application/x-javascript");
  });

  webserver.on("/json/0x45", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("application/json");


/*
    if(remote_data2[0] == '\0') {
      response->print("NO DATA\n");
      request->send(response);
      return;
    }
*/
    response->print("[");

    //request->send(200, "application/text", buffer);
		for (uint8_t i = 0; i < MAX_REMOTE_SIDS; i++) {
      if(remote_data2[i][0] == '\0' || strlen(remote_data2[i]) < 2) {
        continue;
      } else {
        if( i > 1) {
          response->print(",\n");
        }

        response->print(remote_data2[i]);
      }
		}

    response->print("]");
    response->print("\r\n\r\n");    
    request->send(response);
    
  });


  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.

  webserver.onNotFound(onNotFound);
  webserver.onFileUpload(onUpload);
  webserver.onRequestBody(onBody);

  webserver.begin();

  tm_SendData.start();
  tm_reboot.start();
  tm_ClearDisplay.start();
  tm_UpdateDisplay.start();
  tm_CheckWifi.start();
  tm_CheckRX.start();
  tm_printDebug.start();

  tft.fillScreen(LCD_state.bgcolor);
}


void loop() 
{ 
  

  esp_task_wdt_reset();

  ArduinoOTA.handle();

  if(OTArunning) return;

  if(tm_reboot.expired()) {
    Serial.print("\n*** SCHEDULED printDebug REBOOT....\n");
    delay(1000); 
    ESP.restart();
  }

  if(tm_CheckRX.expired()) {
    if(stat_conn_count > last_conn_count) {
      last_conn_count = stat_conn_count;
    } else {
      Serial.println(F("Connection counter not increased within set timespan (stuck wifi?) rebooting!"));
      ESP.restart();
      delay(10000);
    }
  }

  // Check if a new client has connected
  WiFiClient newClient = server.available();
  if (newClient) {
    if(printDebug) Serial.printf("New client: IP=%s, ", newClient.remoteIP().toString().c_str());
    stat_conn_count++;
    // Find the first unused space
    for (int i=0 ; i<MAX_CLIENTS ; ++i) {
        if (NULL == clients[i]) {
            clients[i] = new WiFiClient(newClient);
            if(printDebug) Serial.printf("added to slot #%u\n", i);
            // TODO: start a timer, disconnect client after X secs without data available
            clientdata[i].conntime = 0;
            clientdata[i].bytes_rx = 0;
            clientdata[i].tm.limitMillis(1000);
            clientdata[i].tm.start();
            break;
        }
     }
  }

  // Check whether each client slot is used and has some data
  client_count = 0;
  for (int i=0 ; i<MAX_CLIENTS ; ++i) {
    if (NULL != clients[i]) {
      client_count++;

      // Clean up stale/dead connections
      if(clientdata[i].tm.expired()) {
        clientdata[i].conntime++;
      }

      bool deleteclient = false;
      if(clientdata[i].conntime > 10 && clientdata[i].bytes_rx == 0) {
        
        deleteclient = true;
      }
      if(clientdata[i].conntime > 60) {
          deleteclient = true;
      }
      if(deleteclient) {
        Serial.printf("Client %u (%s) connected %u secs (>10/max 60) and sent %u bytes, will delete!\n", i, clients[i]->remoteIP().toString().c_str(), clientdata[i].conntime, clientdata[i].bytes_rx);          
        clients[i]->stop();
        delete clients[i];
        clients[i] = NULL;
        if(printDebug) Serial.printf("client deleted from slot #%u\n", i);
        continue;
      }
      
      // Process data from clients
      if(clients[i]->available() ) {
      

        //
        uint16_t idx = 0;
        //char data_string[BUF_SIZE] = {0};
        while(clients[i]->available()) {
          char c = clients[i]->read();
          queue_tx.unshift(c); // add a char to buffer (serial out)
          //buffer[idx] = c; // add a char to buffer (webserver out)
          //Q_rx.push((uint8_t *) &c);
          data_string[idx] = c;
          clientdata[i].bytes_rx++;
          idx++;
        }
        data_string[idx] = '\0';
        //Serial.println("pushed #1");
        //buffer[idx] = '\0';
        stat_bytes_rx += clientdata[i].bytes_rx;
        queue_tx.unshift('\n');
        
        //char c;
        //c = '\n';
        //Q_rx.push((uint8_t *) &c);

    uint32_t cmd = 0;
    uint32_t devid = 0;
    int32_t sid = -1;
    


    //Q_rx.pop((uint8_t *) &data_string);

    //data_string: {"cmd":69,"devid":50406,"sid":1,"data":{"value":26.4}}
//Serial.println("DEB 1");

    DynamicJsonDocument tmp_json(BUF_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)
    DeserializationError error = deserializeJson(tmp_json, (const char*) data_string); // read-only input (duplication)
    //DeserializationError error = deserializeJson(tmp_json, data_string); // writeable (zero-copy method)

    if (error) {
        Serial.print(data_string);
        Serial.print(F("\n^JSON_ERR: "));
        Serial.println(error.f_str());
    } else {


      cmd = tmp_json["cmd"];
      if(!cmd) {
        cmd = 0;
      }
      devid = tmp_json["devid"];
      if(!devid) {
        devid = 0;
      }
      if(tmp_json["sid"].isNull()) {
        sid = -1;
      } else {
        sid = tmp_json["sid"];
      }

    
      
      //Serial.println("DEB 2");

          uint32_t _devid = 0;
          int32_t _sid = -1;
          uint32_t _ts = 0;
          bool need_to_add = true;
          uint8_t slot_to_add = 0;
          char _tmpbuf[BUF_SIZE];

          tmp_json["ts"] = now();

//_ts = now();

          for(uint8_t i=MAX_REMOTE_SIDS-1; i>0; i--) {  

            if(remote_data2[i][0] != '\0' && strlen(remote_data2[i]) > 1) {    
              // make a copy of the array to tokenize, as we need to retain original
              strcpy(_tmpbuf, remote_data2[i]);
              char *token = strtok(_tmpbuf, ","); 
              uint32_t ret = 0;
              uint32_t _val = 0;

              // Keep parsing tokens while one of the delimiters present in input
              while (token != NULL) 
              { 
                  sscanf(token, "\"devid\":%u", &_devid);            
                  // sid can legally be 0, need some additional validation
                  ret = sscanf(token, "\"sid\":%u", &_val);
                  if(ret != 0) {
                    _sid = _val;
                  }
                  
                  sscanf(token, "\"ts\":%u", &_ts);
                  token = strtok(NULL, ","); 
              }

              if(_devid == 0 || _sid == -1 || _ts == 0) {
                Serial.printf("ERR: 0x45 #%u slot invalid devid/sid/ts (%u/%u/%u)\n", i, _devid, _sid, _ts);
                memset ( (void*)remote_data2[i], 0, sizeof(remote_data2[i]) );
                slot_to_add = i;
              } else {
                Serial.printf("0x45 #%u looking for slot devid=%u,sid=%u\n", i, _devid, _sid);

                // if data too old, delete doc so slot can be reused
                if(now() - _ts > MAX_AGE_SID) {
                  Serial.printf("ERR: 0x45 #%u old (>%u sec), freeing spot\n", i, MAX_AGE_SID);
                  //remote_data2[i][0] = '\0';
                  memset ( (void*)remote_data2[i], 0, sizeof(remote_data2[i]) );
                }

                if(
                  _devid == devid &&
                  _sid == sid              
                ) {

                  //if(doSerialDebug) 
                    //Serial.printf("0x45 #%u updating slot devid/sid %u/%u...", i, _devid, _sid);

                  // TODO: fix random crashes (LoadException)
                  //Serial.printf("0x45 #%u tmp_json.size=%u, sizeof remote_data2=%u\n", i, tmp_json.size(), sizeof(remote_data2[MAX_REMOTE_SIDS]));
                  if(tmp_json.size() < sizeof(remote_data2[MAX_REMOTE_SIDS])) {
                    serializeJson(tmp_json, remote_data2[i]);  
                  } else {
                    Serial.print(F("ERR: 0x45 tmp_json too big for remote_data2, ignoring!\n"));
                  }
                  //Serial.print(F("OK\n"));

                  need_to_add = false;
                  break;
                } else {
                  
                  //Serial.printf("0x45 #%u no match, current/received slot devid=%u/%u,sid=%u/%u\n", i, _devid, devid, _sid, sid);
                }                  
              }
            } else {
              Serial.printf("0x45 #%u slot empty\n", i);
              slot_to_add = i;
            }
          } // for

          bool is_added = false;
          if(need_to_add) { // devid/sid must be added to an empty position (json doc) in array
            if(tmp_json.size() < sizeof(remote_data2[MAX_REMOTE_SIDS])) {
              serializeJson(tmp_json, remote_data2[slot_to_add]);
              is_added = true;
              Serial.printf("0x45 #%u data added to buffer devid=%u,sid=%u\n", slot_to_add, _devid, _sid);
            } else {
              Serial.print(F("ERR 0x45 tmp_json too big for remote_data2, ignoring!\n"));
            }
          } else {
            Serial.print(F("0x45 need_to_add=false\n"));
          }

          // check if we ran out of space in array (more devid/sid than MAX_REMOTE_SIDS)
          if(need_to_add && !is_added) {
            Serial.print(F("ERR: 0x45 failed to add to buffer/no space, increase MAX_REMOTE_SIDS: "));
            Serial.println(MAX_REMOTE_SIDS);
          }
    }





        //datastore.unshift('\n');
        
        if(printDebug) Serial.printf("got %u bytes, ", clientdata[i].bytes_rx);

        clients[i]->printf("DATA RCVD OK:%u BYTES\nBYE\n\n", clientdata[i].bytes_rx);
        clients[i]->flush();      
        if(printDebug) Serial.printf("sent ACK, ");

        clients[i]->stop();
        delete clients[i];
        clients[i] = NULL;
        if(printDebug) Serial.printf("client deleted slot #%u\n", i);
      } 
    }
  }

  if(client_count >= MAX_CLIENTS) {
    Serial.printf("ERR: all client slots used (%u/%u), probably stucked connections, rebooting!\n", client_count, MAX_CLIENTS);
    ESP.restart();
    delay(1000);
  }

  if(tm_printDebug.expired()) {
    if(printDebug) Serial.printf("Client slots: ");
    for (int i=0 ; i<MAX_CLIENTS ; ++i) {
      if (NULL == clients[i] ) {
        if(printDebug) Serial.printf("%u=free ", i);
      } else {
        if(printDebug) Serial.printf("%u=%s ", i, clients[i]->remoteIP().toString().c_str());
      }
    }
    if(printDebug) Serial.printf("\nClients: (%u/%u)\n", client_count, MAX_CLIENTS);
    if(printDebug) Serial.printf("Listening on TCP: %s:%u\n",WiFi.localIP().toString().c_str(), SERIAL1_TCP_PORT);
    if(printDebug) Serial.printf("Uptime: %s\n", SecondsToDateTimeString(millis()/1000, TFMT_HOURS));


    //if (!Q_rx.isEmpty()) {
    if (buffer[0] != '\0') {
      if(printDebug) Serial.println(buffer);
      /*
      //if(Q_rx.isFull()) {
      //  Serial.println("ERR: Q_rx is full!!!" );
      //}
    
      if(printDebug) Serial.print("Q_rx:");
      for (byte i = 0; i < Q_rx.size() - 1; i++) {
          if(Q_rx[i] != '\0')
            if(printDebug) Serial.print(Q_rx[i]);
      }
      if(printDebug) Serial.print("\n");
      //if(printDebug) Serial.printf("Q_rx.pop: %s\n", data_string);
      */
    }
  
  }


  if(tm_CheckWifi.expired()) {

    if (!WiFi.isConnected())
    {
        delay(3000);
        if(!WiFi.isConnected()) {
          reconnects_wifi++;
          Serial.print(F("Trying WiFi reconnect #"));
          Serial.println(reconnects_wifi);

          WiFi.begin(ssid, pw);
          //WiFi.reconnect();
          
          uint8_t cnt = 0;
          while (WiFi.status() != WL_CONNECTED) {  
            esp_task_wdt_reset(); 
            if(cnt++ > 60) {
              Serial.println(F("Failed"));
              WiFi.disconnect();
              return;
            }
            delay(500);  
            Serial.print(".");
          }  
          Serial.println(F("Reconnected OK!"));

          if (reconnects_wifi == 30)
          {
            Serial.println(F("Too many failures, rebooting..."));          
            ESP.restart();
            return;
          }
        }
    } else {
      if(printDebug) Serial.println(F("WiFi OK"));
    }
  }


  if(tm_ClearDisplay.expired()) {
    tft.fillScreen(LCD_state.bgcolor);
  }

  if(tm_UpdateDisplay.expired()) {

    LCD_state.fgcolor = TFT_GOLD;
    LCD_state.bgcolor = TFT_BLACK;

    tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);
    tft.setTextWrap(false);
    tft.setCursor(0, 0);
    tft.setTextSize(3);
    tft.printf("  WIFI HUB \n\n");  
    tft.setTextSize(2);

    LCD_state.fgcolor = TFT_GREEN;
    LCD_state.bgcolor = TFT_BLACK;
    tft.setTextColor(LCD_state.fgcolor, LCD_state.bgcolor);

    tft.printf("TCP conns: %u  \n", stat_conn_count);

    char s[32] = "";        
    tft.printf("RX->TX: %s   \n", FormatBytes(stat_bytes_rx, s));
    
    tft.printf("TX Queue: %u/%u \n\n", queue_tx.size(), queue_tx.size() + queue_tx.available());

    tft.printf("Uptime: %s\n", SecondsToDateTimeString(millis()/1000, TFMT_HOURS));

  }

  if(tm_SendData.expired()) {
    
    // First send our local system data (sid 0)
    char sid[200] = {0};
    uint64_t chipid = ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
    const char *ptr;

    sprintf(sid, " {\"cmd\":%u,\"devid\":%u,\"sid\":0,\"data\":{\"firmware\":\"%s\",\"IP\":\"%s\",\"port\":%u,\"uptime_sec\":%lu,\"rssi\":%i}}", 0x45, (uint16_t)(chipid>>32), VERSION, WiFi.localIP().toString().c_str(), SERIAL1_TCP_PORT, millis()/1000, getStrength(5));
    ptr = sid;
    while(*ptr != '\0')
    {
      Serial_one.write(*ptr);
      if(printDebug) Serial.write(*ptr);
      delayMicroseconds(10);
      ptr++;
    }    
    Serial_one.write('\n');
    if(printDebug) Serial.write('\n');
    delay(100);

    if(!queue_tx.isEmpty()) {
      if(printDebug) Serial.printf("%u/%u bytes in TX queue, flushing to Serial...", queue_tx.size(), queue_tx.size() + queue_tx.available());
      //if(printDebug) Serial.printf("TXc:");  
    
      while (!queue_tx.isEmpty()) {
          
        char c = queue_tx.pop();
        if(c == '\n') {
          delay(400);
        }
        if(c == '\0') break;

        Serial_one.write(c);
        //if(printDebug) Serial.write(c);  
        delayMicroseconds(10); // don't stress ATMega
        //Serial.write('\n');  
        //Serial_one.write('\n');
      }  

      if(printDebug) Serial.printf("OK!\n");  
      delay(200); 
    }

  }

}

/**
* Take measurements of the Wi-Fi strength and return the average result.
*/
int getStrength(int points){
    long rssi = 0;
    long averageRSSI = 0;
    
    for (int i=0;i < points;i++){
        rssi += WiFi.RSSI();
        delay(20);
    }

   averageRSSI = rssi/points;
    return averageRSSI;
}


const char *FormatBytes(long long bytes, char *str)
{
    const char *sizes[5] = { "B", "KB", "MB", "GB", "TB" };
 
    int i;
    double dblByte = bytes;
    for (i = 0; i < 5 && bytes >= 1024; i++, bytes /= 1024)
        dblByte = bytes / 1024.0;
 
    sprintf(str, "%.2f", dblByte);
 
    return strcat(strcat(str, " "), sizes[i]);
}

char * SecondsToDateTimeString(uint32_t seconds, uint8_t format)
{
  time_t curSec;
  struct tm *curDate;
  static char dateString[32];
  
  curSec = time(NULL) + seconds;
  curDate = localtime(&curSec);

  switch(format) {    
    case TFMT_LONG: strftime(dateString, sizeof(dateString), "%A, %B %d %Y %H:%M:%S", curDate); break;
    case TFMT_DATETIME: strftime(dateString, sizeof(dateString), "%Y-%m-%d %H:%M:%S", curDate); break;
    case TFMT_HOURS: 
    {
      long h = seconds / 3600;
      uint32_t t = seconds % 3600;
      int m = t / 60;
      int s = t % 60;
      sprintf(dateString, "%04ld:%02d:%02d", h, m, s);
    }
      break;

    default: strftime(dateString, sizeof(dateString), "%Y-%m-%d %H:%M:%S", curDate);
  }
  return dateString;
}


void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  //Handle WebSocket event
  Serial.println(F("onEvent"));
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

String HTMLProcessor(const String& var) {
  Serial.println(var);
  if (var == "UPTIMESECS"){
    return String(millis() / 1000);
  }
  else if (var == "VERSION"){
    return String(VERSION);
  }
  return String();
}
