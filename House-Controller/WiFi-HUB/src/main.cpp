/**
 * ESP32 WiFi <-> 3x UART + SPI Bridge
 * WiFi sensors connects to this AP. Their (JSON) data is received via TCP and forwarded out on SPI and Serial.
 * No data/JSON manipulation takes place in this module, just a simple transparent bridge.
 * Currently ADC-MCU is SPI slave and listens only for SPI, Serial not used.
 */

#include <Arduino.h>
#include "config.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <ArduinoOTA.h> 
#include <SPI.h>
#include <ESPmDNS.h>
#include <Timemark.h>
#include <esp_task_wdt.h>
#include <CircularBuffer.h>
#include <TFT_eSPI.h>
#include <logo_kra-tech.h>

//3 seconds WDT
#define WDT_TIMEOUT 8

bool OTArunning = false;
uint8_t stat_q_tx;
uint8_t stat_tcp_count;

CircularBuffer<char*, 10> queue_tx;
LCD_state_struct LCD_state;

Timemark tm_SendData(5000);
Timemark tm_reboot(3600000);

HardwareSerial Serial_one(1);
WiFiServer server(SERIAL1_TCP_PORT);
WiFiClient serverClients[4];
TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

void setup() {
  delay(500);

  Serial.begin(115200);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial_one.begin(UART_BAUD1, SERIAL_PARAM1, SERIAL1_RXPIN, SERIAL1_TXPIN);

  if(debug) Serial.println("\n\nSensor-HUP WiFi serial bridge Vx.xx");


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

  tft.println("\n\n Starting HUB...  ");
  tft.setTextSize(txtsize);
  delay(700);

  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  #ifdef MODE_AP 
   if(debug) Serial.println("Open ESP Access Point mode");
  //AP mode (phone connects directly to ESP) (no router)

  
  WiFi.mode(WIFI_AP);
   
  WiFi.softAP(ssid, pw); // configure ssid and password for softAP
  delay(2000); // VERY IMPORTANT
  WiFi.softAPConfig(ip, ip, netmask); // configure ip address for softAP

  Serial.printf("AP SSID:%s, key:%s, IP:%s\n", ssid, pw, ip.toString().c_str());
  #endif


  #ifdef MODE_STA
   if(debug) Serial.println("Open ESP Station mode");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pw);
  if(debug) Serial.print("try to Connect to Wireless network: ");
  if(debug) Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {   
    delay(500);
    if(debug) Serial.print(".");
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
  server.setNoDelay(true);

   Serial.println("starting mDNS as 'SENSORHUB'");
  if(!MDNS.begin("SENSORHUB")) {
     Serial.println("Error starting mDNS");
     delay(10000);
     ESP.restart();
     return;
 }
 MDNS.addService("rs232-1", "tcp", 8880);
 
  tm_SendData.start();
  tm_reboot.start();

  //tft.println("\n\n   Ready ");
  tft.fillScreen(LCD_state.bgcolor);
  tft.setCursor(0, 0);
  tft.printf("      WIFI HUB       \n");  
}


void loop() 
{ 
  
  ArduinoOTA.handle();
  
  esp_task_wdt_reset();

  if(OTArunning) return;

  tft.setCursor(0, 30);
  tft.printf("Client conn: %u  ", stat_tcp_count);

  tft.setCursor(0, 60);
  tft.printf("RX Queue:    %u  ", stat_q_tx);
  
  if(tm_SendData.expired()) {


    tft.fillScreen(LCD_state.bgcolor);


    while (!queue_tx.isEmpty()) {
        
      char *b = queue_tx.pop();
      Serial.printf("TX:%s\n", b);
      Serial_one.print(b);
      Serial_one.write('\n');

      delay(40); // so the slower Mega can keep up...

      Serial.printf("%u/%u items in TX queue\n", queue_tx.size(), queue_tx.size() + queue_tx.available());
    }            

    Serial.printf("Listening on TCP: %s:%u\n",WiFi.localIP().toString().c_str(), SERIAL1_TCP_PORT);
  }

  if(tm_reboot.expired()) {
    Serial.print("\n*** SCHEDULED DEBUG REBOOT....\n");
    delay(1000); 
    ESP.restart();
  }

  uint8_t i;

    //check if there are any new clients
    if (server.hasClient()){
      
      for(i = 0; i < MAX_SRV_CLIENTS; i++){
        //find free/disconnected spot
        if (!serverClients[i] || !serverClients[i].connected()){
          if(serverClients[i]) serverClients[i].stop();
          serverClients[i] = server.available();
          //if (!serverClients[i]) Serial.println("available broken");
          stat_tcp_count++;
          Serial.print("New client: ");
          Serial.print(i); Serial.print(' ');
          Serial.println(serverClients[i].remoteIP());
          break;
        }
      }
      if (i >= MAX_SRV_CLIENTS) {
        //no free/disconnected spot so reject
        server.available().stop();
      }
    }
    //check clients for data
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        if(serverClients[i].available()){
          char buffer_tmp[bufferSize] = {0};
          //  memset ( (void*)buffer_tmp, 0, bufferSize );
          //  buffer_tmp[0] = {0};
          while(serverClients[i].available()) {              
            serverClients[i].readBytesUntil('\n', buffer_tmp, sizeof(buffer_tmp)-1);
          }          
          queue_tx.unshift(buffer_tmp);
          Serial.printf("RX:%s\n", buffer_tmp);
        }
      }
      else {
        if (serverClients[i]) {
          serverClients[i].stop();
        }

        if(stat_tcp_count > 0) {
          stat_tcp_count--;
        }

      }
    }

}
