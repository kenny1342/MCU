/**
 * ESP32 WiFi <-> 3x UART Bridge
 * WiFi sensor connects to this AP. Their (JSON) data is received via TCP and relayed to Frontend via serial.
 */

#include <Arduino.h>
#include "config.h"
#include <esp_wifi.h>
#include <WiFi.h>


/* BETTER/SIMPLER EXAMPLE - PORT TO ESP32!!!

#include <ESP8266WiFi.h>

//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 1
#define TCP_PORT (23)           // Choose any port you want
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpServerClients[MAX_SRV_CLIENTS];

IPAddress apIP(192, 168, 1, 1);
const char SSID[] = "KRATECH-UART-HUB";  // Choose any SSID
const char PASSWORD[] = "12345678"; // minimum 8 characters. !!!CHANGE THIS!!!

#define SerialDebug Serial1   // Debug goes out on GPIO02
#define SerialGPS   Serial    // GPS or other device connected to the ESP UART

#ifndef min
#define min(x,y)  ((x)<(y)?(x):(y))
#endif

void setup() {
  // !!! Debug output goes to GPIO02 !!!
  SerialDebug.begin(115200);
  SerialDebug.println("TCP <-> UART bridge");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(SSID, PASSWORD);

  //start UART. Be sure to set the speed to match the speed of whatever is
  //connected  to the UART.
  SerialGPS.begin(9600);

  // Start TCP listener on port TCP_PORT
  tcpServer.begin();
  tcpServer.setNoDelay(true);
  SerialDebug.print("Ready! Use 'telnet or nc ");
  SerialDebug.print(WiFi.localIP());
  SerialDebug.print(' ');
  SerialDebug.print(TCP_PORT);
  SerialDebug.println("' to connect");
}

void loop() {
  uint8_t i;
  char buf[1024];
  int bytesAvail, bytesIn;

  //check if there are any new clients
  if (tcpServer.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      //find free/disconnected spot
      if (!tcpServerClients[i] || !tcpServerClients[i].connected()) {
        if (tcpServerClients[i]) tcpServerClients[i].stop();
        tcpServerClients[i] = tcpServer.available();
        SerialDebug.print("New client: "); SerialDebug.print(i);
        continue;
      }
    }
    //no free/disconnected spot so reject
    WiFiClient tcpServerClient = tcpServer.available();
    tcpServerClient.stop();
  }

  //check clients for data
  for (i = 0; i < MAX_SRV_CLIENTS; i++) {
    if (tcpServerClients[i] && tcpServerClients[i].connected()) {
      //get data from the telnet client and push it to the UART
      while ((bytesAvail = tcpServerClients[i].available()) > 0) {
        bytesIn = tcpServerClients[i].readBytes(buf, min(sizeof(buf), bytesAvail));
        if (bytesIn > 0) {
          SerialGPS.write(buf, bytesIn);
          delay(0);
        }
      }
    }
  }

  //check UART for data
  while ((bytesAvail = SerialGPS.available()) > 0) {
    bytesIn = SerialGPS.readBytes(buf, min(sizeof(buf), bytesAvail));
    if (bytesIn > 0) {
      //push UART data to all connected telnet clients
      for (i = 0; i < MAX_SRV_CLIENTS; i++) {
        if (tcpServerClients[i] && tcpServerClients[i].connected()) {
          tcpServerClients[i].write((uint8_t*)buf, bytesIn);
          delay(0);
        }
      }
    }
  }
}
*/

#ifdef BLUETOOTH
#include <BluetoothSerial.h>
BluetoothSerial SerialBT; 
#endif

#ifdef OTA_HANDLER  
#include <ArduinoOTA.h> 

#endif // OTA_HANDLER

HardwareSerial Serial_one(1);
HardwareSerial Serial_two(2);
HardwareSerial* COM[NUM_COM] = {&Serial, &Serial_one , &Serial_two};

#define MAX_NMEA_CLIENTS 4
#ifdef PROTOCOL_TCP
#include <WiFiClient.h>
WiFiServer server_0(SERIAL0_TCP_PORT);
WiFiServer server_1(SERIAL1_TCP_PORT);
WiFiServer server_2(SERIAL2_TCP_PORT);
WiFiServer *server[NUM_COM]={&server_0,&server_1,&server_2};
WiFiClient TCPClient[NUM_COM][MAX_NMEA_CLIENTS];
#endif


uint8_t buf1[NUM_COM][bufferSize];
uint16_t i1[NUM_COM]={0,0,0};

uint8_t buf2[NUM_COM][bufferSize];
uint16_t i2[NUM_COM]={0,0,0};

uint8_t BTbuf[bufferSize];
uint16_t iBT =0;


void setup() {

  delay(500);
  
  COM[0]->begin(UART_BAUD0, SERIAL_PARAM0, SERIAL0_RXPIN, SERIAL0_TXPIN);
  COM[1]->begin(UART_BAUD1, SERIAL_PARAM1, SERIAL1_RXPIN, SERIAL1_TXPIN);
  COM[2]->begin(UART_BAUD2, SERIAL_PARAM2, SERIAL2_RXPIN, SERIAL2_TXPIN);
  
  if(debug) COM[DEBUG_COM]->println("\n\nSensor-HUP WiFi serial bridge Vx.xx");
  #ifdef MODE_AP 
   if(debug) COM[DEBUG_COM]->println("Open ESP Access Point mode");
  //AP mode (phone connects directly to ESP) (no router)
  
  WiFi.mode(WIFI_AP);
   
  WiFi.softAP(ssid, pw); // configure ssid and password for softAP
  delay(2000); // VERY IMPORTANT
  WiFi.softAPConfig(ip, ip, netmask); // configure ip address for softAP

  Serial.printf("AP SSID:%s, key:%s, IP:%s\n", ssid, pw, ip.toString().c_str());
  #endif


  #ifdef MODE_STA
   if(debug) COM[DEBUG_COM]->println("Open ESP Station mode");
  // STATION mode (ESP connects to router and gets an IP)
  // Assuming phone is also connected to that router
  // from RoboRemo you must connect to the IP of the ESP
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pw);
  if(debug) COM[DEBUG_COM]->print("try to Connect to Wireless network: ");
  if(debug) COM[DEBUG_COM]->println(ssid);
  while (WiFi.status() != WL_CONNECTED) {   
    delay(500);
    if(debug) COM[DEBUG_COM]->print(".");
  }
  if(debug) COM[DEBUG_COM]->println("\nWiFi connected");
  
  #endif
#ifdef BLUETOOTH
  if(debug) COM[DEBUG_COM]->println("Open Bluetooth Server");  
  SerialBT.begin(ssid); //Bluetooth device name
 #endif
#ifdef OTA_HANDLER  
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
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
#endif // OTA_HANDLER    

  #ifdef PROTOCOL_TCP
  COM[0]->println("Starting TCP Server 1");  
  if(debug) COM[DEBUG_COM]->println("Starting TCP Server 1");  
  server[0]->begin(); // start TCP server 
  server[0]->setNoDelay(true);
  COM[1]->println("Starting TCP Server 2");
  if(debug) COM[DEBUG_COM]->println("Starting TCP Server 2");  
  server[1]->begin(); // start TCP server 
  server[1]->setNoDelay(true);
  COM[2]->println("Starting TCP Server 3");
  if(debug) COM[DEBUG_COM]->println("Starting TCP Server 3");  
  server[2]->begin(); // start TCP server   
  server[2]->setNoDelay(true);
  #endif

  //esp_err_t esp_wifi_set_max_tx_power(50);  //lower WiFi Power
}


char buffer[1];

void loop() 
{  
#ifdef OTA_HANDLER  
  ArduinoOTA.handle();
#endif // OTA_HANDLER
  
#ifdef BLUETOOTH
  // receive from Bluetooth:
  if(SerialBT.hasClient()) 
  {
    while(SerialBT.available())
    {
      BTbuf[iBT] = SerialBT.read(); // read char from client (LK8000 app)
      if(iBT <bufferSize-1) iBT++;
    }          
    for(int num= 0; num < NUM_COM ; num++)
      COM[num]->write(BTbuf,iBT); // now send to UART(num):          
    iBT = 0;
  }  
#endif  
#ifdef PROTOCOL_TCP
  for(int num= 0; num < NUM_COM ; num++)
  {
    if (server[num]->hasClient())
    {
      for(byte i = 0; i < MAX_NMEA_CLIENTS; i++){
        //find free/disconnected spot
        if (!TCPClient[num][i] || !TCPClient[num][i].connected()){
          if(TCPClient[num][i]) TCPClient[num][i].stop();
          TCPClient[num][i] = server[num]->available();
          continue;
        }
      }
      //no free/disconnected spot so reject
      WiFiClient TmpserverClient = server[num]->available();
      TmpserverClient.stop();
    }
  }
#endif
 
  for(int num= 0; num < NUM_COM ; num++)
  {
    if(COM[num] != NULL)          
    {
      for(byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++)
      {               
        if(TCPClient[num][cln]) 
        {
          if(TCPClient[num][cln].available()) {
            
            while(TCPClient[num][cln].available())
            {
              TCPClient[num][cln].readBytesUntil('\n', buffer, sizeof(buffer));
              COM[num]->write(buffer);
              COM[DEBUG_COM]->write(buffer);              
            } 
            COM[DEBUG_COM]->write('\n');

          }
        }
      }
  
      if(COM[num]->available())
      {
        while(COM[num]->available())
        {     
          buf2[num][i2[num]] = COM[num]->read(); // read char from UART(num)
          if(i2[num]<bufferSize-1) i2[num]++;
        }
        // now send to WiFi:
        for(byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++)
        {   
          if(TCPClient[num][cln])                     
            TCPClient[num][cln].write(buf2[num], i2[num]);
        }
#ifdef BLUETOOTH        
        // now send to Bluetooth:
        if(SerialBT.hasClient())      
          SerialBT.write(buf2[num], i2[num]);               
#endif  
        i2[num] = 0;
      }
    }    
  }
}
