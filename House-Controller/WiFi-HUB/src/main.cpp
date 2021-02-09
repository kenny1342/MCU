/**
 * ESP32 WiFi <-> 3x UART + SPI Bridge
 * WiFi sensors connects to this AP. Their (JSON) data is received via TCP and forwarded out on SPI and Serial.
 * No data/JSON manipulation takes place in this module, just a simple transparent bridge.
 * Currently ADC-MCU is SPI slave and listens only for SPI, Serial not used.
 */

#include <Arduino.h>
#include "main.h"
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
#define WDT_TIMEOUT 3

bool OTArunning = false;
uint16_t stat_q_rx;
uint16_t stat_tcp_count;
uint8_t reconnects_wifi;

// Ring buffer with all chars received via TCP, to be written to serial
CircularBuffer<char, 2048> queue_tx; // chars we queue (3 probes accumulates ~400 chars between each SendData every 5 sec)
LCD_state_struct LCD_state;

Timemark tm_CheckWifi(5000);
Timemark tm_SendData(5000);
Timemark tm_reboot(1440 * 1000); 
Timemark tm_ClearDisplay(300000);
Timemark tm_UpdateDisplay(200);

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

  if(debug) Serial.println("\n\nSensor-HUP WiFi serial bridge V" VERSION);


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
  
  WiFi.setAutoReconnect(true);
  
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
  tm_ClearDisplay.start();
  tm_UpdateDisplay.start();
  tm_CheckWifi.start();

  //tft.println("\n\n   Ready ");
  tft.fillScreen(LCD_state.bgcolor);
}


void loop() 
{ 
  
  esp_task_wdt_reset();

  ArduinoOTA.handle();

  if(OTArunning) return;

  if(tm_CheckWifi.expired()) {

    if (!WiFi.isConnected())
    {
        //MDNS.end();
        delay(5000);
        if(!WiFi.isConnected()) {
          Serial.print(F("Trying WiFi reconnect #"));
          Serial.println(reconnects_wifi);

          WiFi.reconnect();
          
          uint8_t cnt = 0;
          while (WiFi.status() != WL_CONNECTED) {  
            if(cnt++ > 6) {
              Serial.println(F("Failed, giving up!"));
              return;
            }
            delay(500);  
            Serial.print(".");
          }  
          Serial.println(F("Reconnected OK!"));
          //MDNS.begin("SENSORHUB");
          
          if (reconnects_wifi == 20)
          {
            Serial.println(F("Too many failures, rebooting..."));          
            //reconnects_wifi = 0;
            ESP.restart();
            return;
          }
        }
    } else {
      Serial.println(F("WiFi OK"));
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

    tft.printf("TCP conns: %u  \n", stat_tcp_count);
    
    tft.printf("RX->TX B: %u   \n", stat_q_rx);
    
    tft.printf("TX Queue: %u/%u \n\n", queue_tx.size(), queue_tx.size() + queue_tx.available());

    tft.printf("Uptime: %s\n", SecondsToDateTimeString(millis()/1000, TFMT_HOURS));

  }

  if(tm_SendData.expired()) {

    Serial.printf("%u/%u bytes in TX queue\n", queue_tx.size(), queue_tx.size() + queue_tx.available());

    if(!queue_tx.isEmpty()) {
      Serial.printf("TXc:");  
    }

    while (!queue_tx.isEmpty()) {
        
      char c = queue_tx.pop();
      if(c == '\n') {
        delay(400);
      }
      if(c == '\0') break;

      Serial_one.write(c);
      Serial.write(c);  
      delayMicroseconds(50); // don't stress ATMega
      //Serial.write('\n');  
      //Serial_one.write('\n');
    }            
    delay(200); 

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
          if(serverClients[i]) {
            serverClients[i].stop();
            Serial.printf("i=%u, serverClients[i].stop()\n", i);
          }
          serverClients[i] = server.available();
          //if (!serverClients[i]) Serial.println("available broken");
          stat_tcp_count++;
          Serial.printf("i=%u, New client! IP=%s\n", i, serverClients[i].remoteIP().toString().c_str());
          break;
        }
      }
      if (i >= MAX_SRV_CLIENTS) {
        //no free/disconnected spot so reject
        server.available().stop();
        Serial.printf("i=%u, server.available().stop()\n", i);
      }
    }
    //check clients for data
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        if(serverClients[i].available()){
          //char buffer_tmp[bufferSize] = {0};
          Serial.print("RX:");
          while(serverClients[i].available()) {              
            //serverClients[i].readBytesUntil('\n', buffer_tmp, sizeof(buffer_tmp)-1);
            char c = serverClients[i].read();
            queue_tx.unshift(c); // add a char to buffer
            Serial.write(c);
            stat_q_rx++;
          }          
          //queue_tx.unshift('\r');
          queue_tx.unshift('\n');          
          //queue_tx.unshift(10);
          Serial.print("\n");          
        }
      }
      else {
        if (serverClients[i]) {
          serverClients[i].stop();
        }

      }
    }

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
