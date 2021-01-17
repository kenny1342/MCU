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
#include <SPI.h>
#include <Timemark.h>


/*
HSPI and VSPI pins can be redefined or defaults used.

I like using HSPI.

The default pins for SPI on the ESP32.
HSPI
MOSI = GPIO13
MISO = GPIO12
CLK = GPIO14
CD = GPIO15

VSPI
MOSI = GPIO23
MISO = GPIO19
CLK/SCK = GPIO18
CS/SS = GPIO5
*/

Timemark tm_SendData(5000);
Timemark tm_reboot(3600000);

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

char JSON_STRINGS[NUM_COM][bufferSize] = {0};

#define NUM_FIELDS  6
byte VALUES[NUM_COM][NUM_FIELDS] = {0,0,0,0,0,0}; //ADC devid, cmd, devid, data1,data2,data3


#define SS 15

SPIClass SPI1(HSPI);
//SPIClass SPI1(VSPI);

void setup() {

  delay(500);
  
  pinMode(SS, OUTPUT); //VSPI SS set slave select pins as output

  SPI1.begin(14, 12, 13, 15); //SCLK, MISO, MOSI, SS
  //SPI1.begin();         // initialize the SPI library (Master only)
  SPI1.setClockDivider(SPI_CLOCK_DIV128);

  // SS/CS pin should be set to LOW to inform the slave that the master will send or request data. Otherwise, it is always HIGH
  digitalWrite(SS,HIGH); // set the SS pin HIGH since we didn’t start any transfer to slave


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

    tm_SendData.start();
  tm_reboot.start();

  //esp_err_t esp_wifi_set_max_tx_power(50);  //lower WiFi Power
}


/**
 * Send JSON strings to SPI 
 */
void SendData() {

  uint16_t writeDelay = 80; // delay (us) between transfer, so the slow ATmega ISR can keep up. Otherwise it reads mostly garbage chars.
  SPI.beginTransaction (SPISettings (1000000, LSBFIRST, SPI_MODE0));  
  
  digitalWrite(SS, LOW); // SS/CS pin should be set to LOW to inform the slave that the master will send or request data. Otherwise, it is always HIGH
  delay(10);
    
  SPI1.write(0x10); // ADC devid / START marker
  delayMicroseconds(writeDelay);
  SPI1.transfer('<'); //delay(10);
  delayMicroseconds(writeDelay);

  COM[DEBUG_COM]->print("SPITX-START:\n");

/*
 // send test string
 char c;
 for (const char * p = "Hello, world!\n" ; c = *p; p++) {
  SPI1.transfer (c);
  //delayMicroseconds(10000);
  delay(10);
 }
  */ 

  //char * str = {"cmd":69,"devid":50406,"firmware":"2.16","IP":"192.168.4.2","port":2323,"uptime_sec":55,"data":{"value":5,"unit":"DEG_C","name":"Pump Room","timestamp":55688},"sid":1}}
  for(int num=0; num<NUM_COM; num++) {    
    uint8_t i = 0;
    while (JSON_STRINGS[num][i] != 0x00)
    {
        SPI1.transfer(JSON_STRINGS[num][i]);        
        COM[DEBUG_COM]->write(JSON_STRINGS[num][i]);
        i++;
        delayMicroseconds(writeDelay);
    }    
  }

  SPI1.transfer(0x0F); // END    
  //delayMicroseconds(50);
  SPI1.endTransaction();
  digitalWrite(SS, HIGH); // disable Slave Select

  COM[DEBUG_COM]->print("\nSPITX-END:\n");

}

char buffer[1];
uint16_t cnt;
void loop() 
{  
  if(tm_SendData.expired()) {

/*
    SPI1.end();
    pinMode(12, OUTPUT); digitalWrite(12, 0);
    pinMode(13, OUTPUT); digitalWrite(13, 0);
    pinMode(14, OUTPUT); digitalWrite(14, 0);
    pinMode(SS, OUTPUT); digitalWrite(15, 0);
    delay(300);
    //pinMode(SS, INPUT);
    pinMode(12, INPUT);
    pinMode(13, INPUT);
    pinMode(14, INPUT);

  SPI1.begin(14, 12, 13, 15); //SCLK, MISO, MOSI, SS
  //SPI1.begin();         // initialize the SPI library (Master only)
  SPI1.setClockDivider(SPI_CLOCK_DIV128);
delay(300);
*/
    SendData();   
    cnt++;
    COM[DEBUG_COM]->printf("CNT: %u\n", cnt);
    delay(100);
  }

  if(tm_reboot.expired()) {
    COM[DEBUG_COM]->print("\n*** SCHEDULED DEBUG REBOOT....\n");
    delay(1000); 
    ESP.restart();
  }


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
            // We got TCP data (normally JSON string) from client, update the JSON_STRINGS char array with the new data, + write to UART
            COM[DEBUG_COM]->print("RXTCP-START:\n");

            while(TCPClient[num][cln].available()) {              
              TCPClient[num][cln].readBytesUntil('\n', JSON_STRINGS[num], sizeof(JSON_STRINGS[num]));
            }
            //int len = strlen(JSON_STRINGS[num]);
            //JSON_STRINGS[num][len] = 'X';
            //JSON_STRINGS[num][len+1] = '\0';

            COM[DEBUG_COM]->write(JSON_STRINGS[num]);
            COM[DEBUG_COM]->print("\nRXTCP-END\n");
            COM[num]->write(JSON_STRINGS[num]);
            COM[num]->write('\n');
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
