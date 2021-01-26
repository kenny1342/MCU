#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h> 
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "OneWireNg_CurrentPlatform.h" // Instead of stock OneWire lib, because it malfunctions when Wire is used too (on ESP32 at least)
#include <KRA_DallasTemp.h>
#include "SSD1306.h"
#include <AM2320.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
//#include <OneWire.h>
//#include <DallasTemperature.h>
#include <Timemark.h>
#include <main.h>

#ifdef USE_AM2320
  #ifdef USE_DHT
    #error Either USE_AM2302 or USE_DHT must be enabled, not both
  #endif
#endif
#ifndef USE_AM2320
  #ifndef USE_DHT
    #error One of USE_AM2302 or USE_DHT must be enabled
  #endif
#endif

#define MAX_SRV_CLIENTS 1
#define TCP_PORT (2323)
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpServerClients[MAX_SRV_CLIENTS];
#ifdef USE_AM2320
AM2320 am2320(&Wire); // AM2320 sensor attached SDA, SCL
#endif

#ifdef USE_DHT
DHT dht(PIN_DHT_SENSOR, DHTTYPE);
#endif

OneWire  ds_sensors(PIN_SENSOR_TEMP_MOTOR); 

#ifdef USE_TFT
SSD1306 display(0x3c, 4, 15);
#endif

bool OTArunning = false;

long ds_temps[NUM_DS_SENSORS];// = {0, 0}; //{0.0, 0.0};
double sid1_value = 0.0;
double sid2_value = 0.0;

Timemark tm_DataTX(5000);

// reading buffor config
//#define BUFFER_SIZE 1024
//byte buff[BUFFER_SIZE];

uint8_t ds_deviceCount = 0;
uint8_t wifitries = 0;
uint8_t hubconntries = 0;


static OneWireNg *ow = NULL;

void setup()
{
 
  Serial.begin(115200);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

  #ifdef USE_TFT
  pinMode(16,OUTPUT);
    
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
  display.init();
  display.flipScreenVertically();  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);  
  display.drawString(0, 0, " INIT... ");
  display.display();
  #endif

  Serial.printf("connecting to SSID:%s, key:%s...\n", ssid, pw);
  WiFi.begin(ssid, pw);
  while (WiFi.status() != WL_CONNECTED) {
    if(++wifitries > 10) {
      Serial.println("too many wifi conn tries, reboot");
      ESP.restart();
    }
    delay(1000);
    Serial.print(".");
  }
 
  Serial.print("\nWiFi connected with IP: ");
  Serial.println(WiFi.localIP());
 

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

  // Start TCP listener on port TCP_PORT
  tcpServer.begin();
  tcpServer.setNoDelay(true);
  Serial.print("Ready! Use 'telnet or nc ");
  Serial.print(WiFi.localIP());
  Serial.print(' ');
  Serial.print(TCP_PORT);
  Serial.println("' to connect");

#ifdef USE_AM2320
  Wire.begin(21, 22);
#endif

#ifdef USE_DHT
dht.begin();
#endif

ow = new OneWireNg_CurrentPlatform(PIN_SENSOR_TEMP_MOTOR, false);
#if (CONFIG_MAX_SRCH_FILTERS > 0)
    /* if filtering is enabled - filter to supported devices only;
       CONFIG_MAX_SRCH_FILTERS must be large enough to embrace all code ids */
    for (size_t i=0; i < ARRSZ(DSTH_CODES); i++)
        ow->searchFilterAdd(DSTH_CODES[i].code);
#endif

  //ds_sensors.begin(PIN_SENSOR_TEMP_MOTOR);
  
 if(mdns_init()!= ESP_OK){
    Serial.println("mDNS failed to start");
    return;
  }

  tm_DataTX.start();
}
 

void loop()
{
  //String dataString = "";
  char dataString[JSON_SIZE] = {0};
  int8_t part;
  StaticJsonDocument<JSON_SIZE> doc;
  uint8_t i;
  int bytesAvail;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("wifi connection gone, reboot");
    ESP.restart();
    delay(1000);
  }


  ArduinoOTA.handle();

  //check if there are any new clients
  if (tcpServer.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      //find free/disconnected spot
      if (!tcpServerClients[i] || !tcpServerClients[i].connected()) {
        if (tcpServerClients[i]) tcpServerClients[i].stop();
        tcpServerClients[i] = tcpServer.available();
        Serial.print("New client: "); Serial.print(i);
        tcpServerClients[i].printf("Welcome to Probe %s!\n", WiFi.localIP().toString().c_str());
        tcpServerClients[i].print(F("Commands: exit | reboot | dump\n"));
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
      
      if((bytesAvail = tcpServerClients[i].available()) > 0) {
        Serial.print(bytesAvail);
        Serial.println(" BYTES FROM TCP:");

        char buffer_cmd[50] = {0};
        tcpServerClients[i].readBytesUntil('\n', buffer_cmd, sizeof(buffer_cmd));
        if(strncmp(buffer_cmd, "exit", 4) == 0) {
          tcpServerClients[i].println("BYE\n");
          tcpServerClients[i].flush();
          tcpServerClients[i].stop();
        }
        if(strncmp(buffer_cmd, "reboot", 6) == 0) {
          tcpServerClients[i].println("Rebooting...\n");
          tcpServerClients[i].flush();
          delay(2000);
          ESP.restart();
        }
        if(strncmp(buffer_cmd, "dump", 4) == 0) {
#ifdef USE_AM2320          
          tcpServerClients[i].printf("AM2320: temp=%0.2f hum=%0.2f\n", am2320.cTemp, am2320.Humidity);
#endif          
          tcpServerClients[i].printf("devid: %u\n", (uint16_t)(ESP.getEfuseMac()>>32));
          tcpServerClients[i].printf("uptime: %lu\n", millis()/1000);
          tcpServerClients[i].printf("DS18b20 device count:%u\n", ds_deviceCount);

          part = ds_temps[0] % 10;
          if(part < 0) part = -part;
          tcpServerClients[i].printf("DS18b20 #0 temp=%ld.%dC\n", ds_temps[0]/1000, part);
          
          part = ds_temps[1] % 10;
          if(part < 0) part = -part;
          tcpServerClients[i].printf("DS18b20 #1 temp=%ld.%dC\n", ds_temps[1]/1000, part);
#ifdef USE_DHT          
          tcpServerClients[i].printf("DHT: temp=%0.2fC - %0.2f%%\n", sid1_value, sid2_value);
      #endif
          
          tcpServerClients[i].printf("Last TX: %s\n", dataString);
          tcpServerClients[i].flush();
        }

      }

      // read data from TCP client
      int size = 0;
      while ((size = tcpServerClients[i].available())) {
                /*size = (size >= BUFFER_SIZE ? BUFFER_SIZE : size);
                tcpServerClients[i].read(buff, size);
                Serial.write(buff, size);
                Serial.flush();*/

      }
    }
  }
/*
*/

  if(tm_DataTX.expired()) {

      readDS18B20();

    #ifdef USE_DHT  
    Serial.print(F("Trying to read DHT/AM2302 sensor..."));
    sid1_value = dht.readTemperature(false);
    sid2_value = dht.readHumidity();
    if(sid1_value == NAN || sid2_value == NAN) {
      Serial.println(F("ERR: Not found!"));
    } else {
      Serial.printf("OK! temp=%0.2fC, hum=%0.2f%%\n", sid1_value, sid2_value);
    }
  #endif

#ifdef USE_AM2320
    // ----------- AM2320 TEMP & HUM ------------
    Serial.print(F("Getting data from AM2320...\n"));

    switch(am2320.Read()) {
      case 2:
        Serial.println("CRC failed!");
        break;
      case 1:
        Serial.println("Offline!");
        break;
      case 0: // OK
        sid1_value = am2320.cTemp;
        sid2_value = am2320.Humidity;
    }
#endif

#ifdef USE_TFT
    char str[50];

    display.clear();
    display.setFont(ArialMT_Plain_10);

    if (WiFi.status() != WL_CONNECTED) {
      sprintf(str, "WiFi ERROR (%s)", WiFi.SSID().c_str());
      
    } else {
      sprintf(str, "WiFi OK (%s) %ddBm", WiFi.SSID().c_str(), getStrength(3));
    }
    display.drawString(0, 0, str);

    sprintf(str, "IP,ID: %s,%u", WiFi.localIP().toString().c_str(), (uint16_t)(ESP.getEfuseMac()>>32));
    display.drawString(0, 10, str);

    part = ds_temps[0] % 10;
    if(part < 0) part = -part;
    sprintf(str, "DS18b20 #0: %ld.%dC\n", ds_temps[0]/1000, part );
    display.drawString(0, 30, str);

    part = ds_temps[1] % 10;
    if(part < 0) part = -part;
    sprintf(str, "DS18b20 #1: %ld.%dC\n", ds_temps[1]/1000, part);
    display.drawString(0, 40, str);

    #ifdef USE_DHT
    sprintf(str, "DHT #1: %0.2fC - %02.f%%\n", sid1_value, sid2_value);
    display.drawString(0, 50, str);
    #endif

    display.display();
#endif
    // --------------- SEND SENSOR DATA TO HUB ---------------------
    Serial.printf("Listening on %s:%u\n", WiFi.localIP().toString().c_str(), TCP_PORT);

    int nrOfServices = MDNS.queryService("rs232-1", "tcp");
      
    if (nrOfServices == 0) {
      Serial.println("No mDNS rs232-1 (HUB) service found! Hub offline?");
      return;
    } else {
      Serial.print(nrOfServices, DEC);
      Serial.println(" mDNS rs232-1 (HUB) services found: ");
    }

    for (int i = 0; i < nrOfServices; i=i+1) {
      Serial.printf("%s (%s:%u)\n", MDNS.hostname(i).c_str(), MDNS.IP(i).toString().c_str(), MDNS.port(i));
    }

    uint8_t mdns_index_hub = 0; // for now just assume the first is the correct (and only)...

    doc.clear();
    JsonObject root = doc.to<JsonObject>();

    root["cmd"] = 0x45; // REMOTE_SENSOR_DATA
    root["devid"] = (uint16_t)(ESP.getEfuseMac()>>32); // this device ID
    root["firmware"] = FIRMWARE_VERSION;
    root["IP"] = WiFi.localIP().toString();
    root["port"] = TCP_PORT;
    root["uptime_sec"] = (uint32_t) millis() / 1000;
    root["rssi"] = getStrength(5);

    JsonObject data = doc.createNestedObject("data");

    // ---------- TEMP PUMP ROOM ----------------
    root["sid"] = 0x01; // this sensor's ID

    data["value"] = sid1_value;
    data["unit"] = "DEG_C";
    //data["name"] = "Room";
    data["timestamp"] = millis();

    //dataString.clear();
    memset ( (void*)dataString, 0, JSON_SIZE );
    serializeJson(root, dataString);    
    Serial.print("TX1:");
    Serial.print(dataString);
    SendData(dataString, mdns_index_hub);
    if(OTArunning) return;
    delay(800);

    root["sid"] = 0x02; // this sensor's ID

    data["value"] = sid2_value;
    data["unit"] = "RH";
    //data["desc"] = "Room";
    data["timestamp"] = millis();

    //dataString.clear();
    memset ( (void*)dataString, 0, JSON_SIZE );
    serializeJson(root, dataString);
    Serial.print("TX2:");
    Serial.print(dataString);
    SendData(dataString, mdns_index_hub);
    if(OTArunning) return;
    delay(800);

    // ---------- TEMP 1-WIRE SENSORS ----------------

    Serial.println();
    root["sid"] = 0x03; // this sensor's ID

    part = ds_temps[0] % 10;
    if(part < 0) part = -part;
    sprintf(str, "%ld.%d", ds_temps[0]/1000, part);
    data["value"] = str; // ds_temps[0]; //(float) random(0,99);
    data["unit"] = "DEG_C";
    //data["desc"] = "Motor";
    data["timestamp"] = millis();

    //dataString.clear();
    memset ( (void*)dataString, 0, JSON_SIZE );
    //dataString[0] = '\0';
    serializeJson(root, dataString);
    Serial.print("TX3:");
    Serial.print(dataString);

    SendData(dataString, mdns_index_hub);
    if(OTArunning) return;
    delay(800);

    root["sid"] = 0x04; // this sensor's ID

    part = ds_temps[1] % 10;
    if(part < 0) part = -part;
    sprintf(str, "%ld.%d", ds_temps[1]/1000, part);
    data["value"] = str; //ds_temps[1]; //(float) random(0,99);
    data["unit"] = "DEG_C";
    //data["desc"] = "Inlet";
    data["timestamp"] = millis();

    //dataString.clear();
    memset ( (void*)dataString, 0, JSON_SIZE );
    dataString[0] = '\0';
    serializeJson(root, dataString);
    Serial.print("TX4:");
    Serial.print(dataString);

    SendData(dataString, mdns_index_hub);

    Serial.print(F("All done!...\n****************************\n"));
  }



}

void SendData(const char * json, uint8_t mdns_index) {

    WiFiClient client;
 
    if (!client.connect(MDNS.IP(mdns_index), MDNS.port(mdns_index))) {
      if(++hubconntries > 20) {
        Serial.println(F("too many HUB conn tries, rebooting"));
        ESP.restart();
      }
        Serial.printf("Connection to HUB (%s:%u) failed!", MDNS.IP(mdns_index).toString().c_str(), MDNS.port(mdns_index));
        delay(2000);
        return;
    }


  client.print(json);
  
  client.stop();

  Serial.println(F(" [OK]"));
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


void readDS18B20() {
    OneWireNg::Id id;
    OneWireNg::ErrorCode ec;

    ow->searchReset();
    ds_deviceCount = 0;
    do
    {
        ec = ow->search(id);
        if (!(ec == OneWireNg::EC_MORE || ec == OneWireNg::EC_DONE))
            break;

        if (!KRA_DallasTemp::printId(id))
            continue;

        /* start temperature conversion */
        ow->addressSingle(id);
        ow->writeByte(CMD_CONVERT_T);

#ifdef PARASITE_POWER
        /* power the bus until the next activity on it */
        ow->powerBus(true);
#endif
        delay(750);

        uint8_t touchScrpd[] = {
            CMD_READ_SCRATCHPAD,
            /* the read scratchpad will be placed here (9 bytes) */
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        };

        ow->addressSingle(id);
        ow->touchBytes(touchScrpd, sizeof(touchScrpd));
        uint8_t *scrpd = &touchScrpd[1];  /* scratchpad data */

        Serial.print("  Scratchpad:");
        for (size_t i = 0; i < sizeof(touchScrpd)-1; i++) {
            Serial.print(!i ? ' ' : ':');
            Serial.print(scrpd[i], HEX);
        }
        Serial.println();

        if (OneWireNg::crc8(scrpd, 8) != scrpd[8]) {
            Serial.println("  Invalid CRC!");
            continue;
        }

        long temp = ((long)(int8_t)scrpd[1] << 8) | scrpd[0];

        if (id[0] != DS18S20) {
            unsigned res = (scrpd[4] >> 5) & 3;
            temp = (temp >> (3-res)) << (3-res);  /* zeroed undefined bits */
            temp = (temp*1000)/16;
        } else
        if (scrpd[7]) {
            temp = 1000*(temp >> 1) - 250;
            temp += 1000*(scrpd[7] - scrpd[6]) / scrpd[7];
        } else {
            /* shall never happen */
            temp = (temp*1000)/2;
            Serial.println("  Zeroed COUNT_PER_C detected!");
        }

        Serial.print("  Temp: ");
        if (temp < 0) {
            //temp = -temp;
            Serial.print('-');
        }
        Serial.print(temp / 1000);
        Serial.print('.');
        Serial.print(temp % 1000);
        Serial.println(" C");

        ds_temps[ds_deviceCount] = temp;

        if(ds_deviceCount < NUM_DS_SENSORS) {
          ds_deviceCount++;
        }
        

    } while (ec == OneWireNg::EC_MORE);

    if ( ds_deviceCount == 0) {
      Serial.println("ERR: No device found!");
    }

}


