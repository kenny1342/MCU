#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h> 
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <AM2320.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Timemark.h>
#include <main.h>

#define MAX_SRV_CLIENTS 1
#define TCP_PORT (2323)
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpServerClients[MAX_SRV_CLIENTS];
#ifdef USE_AM2320
AM2320 am2320_pumproom(&Wire); // AM2320 sensor attached SDA, SCL
#endif
//OneWire  ds18b20_temp_motor(PIN_SENSOR_TEMP_MOTOR); 

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(PIN_SENSOR_TEMP_MOTOR);	
// Pass oneWire reference to DallasTemperature library
DallasTemperature ds_sensors(&oneWire);

Timemark tm_DataTX(1000);
/*
OneWire oneWire(PIN_SENSOR_TEMP_MOTOR);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
// arrays to hold device addresses
DeviceAddress insideThermometer, outsideThermometer;
*/
//OneWire ds(PIN_SENSOR_TEMP_MOTOR); // on pin x (a 4.7K resistor is necessary)

// reading buffor config
#define BUFFER_SIZE 1024
byte buff[BUFFER_SIZE];

uint8_t ds_deviceCount = 0;
uint8_t wifitries = 0;
uint8_t hubconntries = 0;

double temp_motor = 0.0;
double temp_inlet = 0.0;

void setup()
{
 
  Serial.begin(115200);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

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

  //ds18b20_temp_motor.begin(PIN_SENSOR_TEMP_MOTOR);
  //oneWire.begin(PIN_SENSOR_TEMP_MOTOR);
  ds_sensors.begin();
  //oneWire.begin(PIN_SENSOR_TEMP_MOTOR);

 if(mdns_init()!= ESP_OK){
    Serial.println("mDNS failed to start");
    return;
  }

  tm_DataTX.start();
}
 

void loop()
{
  String dataString = "";
  StaticJsonDocument<250> doc;

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
          tcpServerClients[i].printf("AM2320: temp=%0.2f hum=%0.2f\n", am2320_pumproom.cTemp, am2320_pumproom.Humidity);
#endif          
          tcpServerClients[i].printf("DS18b20 device count:%u\n", ds_deviceCount);
          tcpServerClients[i].printf("DS18b20 #0 (motor) temp=%0.2f\n", temp_motor);
          tcpServerClients[i].printf("DS18b20 #1 (inlet) temp=%0.2f\n", temp_inlet);
          
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

#ifdef USE_AM2320
    // ----------- AM2320 TEMP & HUM ------------
    Serial.print(F("Getting data from AM2320...\n"));

    switch(am2320_pumproom.Read()) {
      case 2:
        Serial.println("CRC failed!");
        break;
      case 1:
        Serial.println("Offline!");
        break;
      case 0:

        // ---------- TEMP PUMP ROOM ----------------
        root["sid"] = 0x01; // this sensor's ID

        data["value"] = am2320_pumproom.cTemp; //(float) random(5,39);
        data["unit"] = "DEG_C";
        data["name"] = "Room";
        data["timestamp"] = millis();

        dataString.clear();
        serializeJson(root, dataString);    
        Serial.print("TX1:");
        Serial.print(dataString);
        SendData(dataString.c_str(), mdns_index_hub);
//        delay(1000);

        root["sid"] = 0x02; // this sensor's ID

        data["value"] = am2320_pumproom.Humidity; //(float) random(0,99);
        data["unit"] = "RH";
        data["desc"] = "Room";
        data["timestamp"] = millis();

        dataString.clear();
        serializeJson(root, dataString);
        Serial.print("TX2:");
        Serial.print(dataString);
        SendData(dataString.c_str(), mdns_index_hub);

//        delay(1000);

        break;
    }    

#endif

    // ---------- TEMP PUMP MOTOR ----------------
    //----------- Update Pump motor temps -------------



  Serial.print("Locating devices...");
  Serial.print("Found ");
  ds_deviceCount = ds_sensors.getDeviceCount();
  Serial.print(ds_deviceCount, DEC);
  Serial.println(" DS devices.");

  Serial.println(F("Reading data from DS18B20 devices..."));

// Send command to all the sensors for temperature conversion
  ds_sensors.requestTemperatures(); 
  
  // Display temperature from each sensor
  for (int i = 0;  i < ds_deviceCount;  i++)
  {
    
    Serial.print("Sensor ");
    Serial.print(i+1);
    Serial.print(" : ");
    float tempC = ds_sensors.getTempCByIndex(i);
    Serial.print(tempC);
    Serial.print((char)176);//shows degrees character
    Serial.print("C");  
    if(i == 0) {
      temp_motor = tempC;
    }
    if(i == 1) {
      temp_inlet = tempC;
    }
  }

    Serial.println();
    root["sid"] = 0x03; // this sensor's ID

    data["value"] = temp_motor; //(float) random(0,99);
    data["unit"] = "DEG_C";
    data["desc"] = "Motor";
    data["timestamp"] = millis();

    dataString.clear();
    serializeJson(root, dataString);
    Serial.print("TX3:");
    Serial.print(dataString);

    SendData(dataString.c_str(), mdns_index_hub);

    delay(100);

    root["sid"] = 0x04; // this sensor's ID

    data["value"] = temp_inlet; //(float) random(0,99);
    data["unit"] = "DEG_C";
    data["desc"] = "Inlet";
    data["timestamp"] = millis();

    dataString.clear();
    serializeJson(root, dataString);
    Serial.print("TX4:");
    Serial.print(dataString);

    SendData(dataString.c_str(), mdns_index_hub);

    Serial.print(F("All done!...\n****************************\n"));
  }



}

void SendData(const char * json, uint8_t mdns_index) {

    WiFiClient client;
 

    if (!client.connect(MDNS.IP(mdns_index), MDNS.port(mdns_index))) {
      if(++hubconntries > 20) {
        Serial.println("too many HUB conn tries, rebooting");
        ESP.restart();
      }
        Serial.printf("Connection to HUB (%s:%u) failed!", MDNS.IP(mdns_index).toString().c_str(), MDNS.port(mdns_index));
        
 
        delay(2000);
        return;
    }


  client.print(json);
  
  client.stop();

  Serial.println(" [OK]");

  //delay(1000);
}

bool readDS18B20(OneWire *ds, double * celsius) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  
  if ( !ds->search(addr)) {
    delay(10);
    ds->reset_search();
    delay(500);
    if ( !ds->search(addr)) {
      Serial.println("ERR: No device found!");
      ds->reset_search();
      delay(250);
      return false;
    }
  }
  
  Serial.print("ROM=");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return false;
  }
  Serial.print(", Chip=");
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.print("DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.print("DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.print("DS1822");
      type_s = 0;
      break;
    default:
      Serial.print("NOT a DS18x20 family device");
      return false;
  } 

  ds->reset();
  ds->select(addr);
  ds->write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds->reset();
  ds->select(addr);    
  ds->write(0xBE);         // Read Scratchpad

  Serial.print(", Data=");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds->read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(", CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  //Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  *celsius = (double)raw / 16.0;
  //fahrenheit = celsius * 1.8 + 32.0;
  Serial.print(", Temperature=");
  Serial.print(*celsius);
  Serial.print(" C");

  return true;
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

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}