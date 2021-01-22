#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <AM2320.h>
#include <OneWire.h>
#include <main.h>

#define MAX_SRV_CLIENTS 1
#define TCP_PORT (2323)
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpServerClients[MAX_SRV_CLIENTS];
AM2320 am2320_pumproom(&Wire); // AM2320 sensor attached SDA, SCL
OneWire  ds18b20_temp_motor(PIN_SENSOR_TEMP_MOTOR); 

// reading buffor config
#define BUFFER_SIZE 1024
byte buff[BUFFER_SIZE];

uint8_t wifitries = 0;
uint8_t hubconntries = 0;

void setup()
{
 
  Serial.begin(115200);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

  Serial.printf("connecting to SSID:%s, key:%s...\n", ssid, password);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    if(++wifitries > 100) {
      Serial.println("too many wifi conn tries, reboot");
      ESP.restart();
    }
    delay(1000);
    Serial.print(".");
  }
 
  Serial.print("\nWiFi connected with IP: ");
  Serial.println(WiFi.localIP());
 

  // Start TCP listener on port TCP_PORT
  tcpServer.begin();
  tcpServer.setNoDelay(true);
  Serial.print("Ready! Use 'telnet or nc ");
  Serial.print(WiFi.localIP());
  Serial.print(' ');
  Serial.print(TCP_PORT);
  Serial.println("' to connect");

  Wire.begin(21, 22);
}
 

void loop()
{
  String dataString = "";
  StaticJsonDocument<250> doc;

  uint8_t i;
  //char buf[1024];
  int bytesAvail;
  double temp_room;
  double hum_room;
  double temp_motor;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("wifi connection gone, reboot");
    ESP.restart();
    delay(1000);
  }


  //check if there are any new clients
  if (tcpServer.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      //find free/disconnected spot
      if (!tcpServerClients[i] || !tcpServerClients[i].connected()) {
        if (tcpServerClients[i]) tcpServerClients[i].stop();
        tcpServerClients[i] = tcpServer.available();
        Serial.print("New client: "); Serial.print(i);
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
      }

      // read JSON data from HUB and spit it out on UART
      int size = 0;
      while ((size = tcpServerClients[i].available())) {
                size = (size >= BUFFER_SIZE ? BUFFER_SIZE : size);
                tcpServerClients[i].read(buff, size);
                Serial.write(buff, size);
                Serial.flush();
      }

      /*
      while ((bytesAvail = tcpServerClients[i].available()) > 0) {
        bytesIn = tcpServerClients[i].readBytes(buf, min(sizeof(buf), bytesAvail));
        if (bytesIn > 0) {
          Serial.write(buf, bytesIn);
          delay(0);
        }
      }
      */
    }
  }


    //----------- Update Pump room temps -------------
    switch(am2320_pumproom.Read()) {
      case 2:
        Serial.println("CRC failed");
        temp_room = -98;
        break;
      case 1:
        Serial.println("am2320_pumproom offline");
        temp_room = -99;
        break;
      case 0:
        temp_room = am2320_pumproom.cTemp;
        hum_room = am2320_pumproom.Humidity;
        break;
    }    

    //----------- Update Pump motor temps -------------

    readDS18B20(&ds18b20_temp_motor, &temp_motor);

  // --------------- SEND SENSOR DATA TO HUB ---------------------
/*
TX1:{"cmd":69,"devid":50406,"firmware":"2.16","IP":"192.168.4.2","port":2323,"uptime_sec":61,"data":{"value":78,"unit":"DEG_C","name":"Pump Room","timestamp":61747},"sid":1}
TX2:{"cmd":69,"devid":50406,"firmware":"2.16","IP":"192.168.4.2","port":2323,"uptime_sec":61,"data":{"value":20,"unit":"RH","name":"Pump Room","timestamp":62756},"sid":2}
TX3:{"cmd":69,"devid":50406,"firmware":"2.16","IP":"192.168.4.2","port":2323,"uptime_sec":61,"data":{"value":66,"unit":"DEG_C","name":"Pump Motor","timestamp":63760},"sid":3}
*/
 
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

    data["value"] = temp_room; //(float) random(5,39);
    data["unit"] = "DEG_C";
    data["name"] = "Room";
    data["timestamp"] = millis();

    dataString.clear();
    serializeJson(root, dataString);
    SendData(dataString.c_str());
    Serial.print("TX1:");
    Serial.println(dataString);
    delay(2000);

    // ----------- HUMIDITY PUMP ROOM ------------
    root["sid"] = 0x02; // this sensor's ID

    data["value"] = hum_room; //(float) random(0,99);
    data["unit"] = "RH";
    data["desc"] = "Room";
    data["timestamp"] = millis();

    dataString.clear();
    serializeJson(root, dataString);
    SendData(dataString.c_str());
    Serial.print("TX2:");
    Serial.println(dataString);
    delay(2000);

    // ---------- TEMP PUMP MOTOR ----------------
    root["sid"] = 0x03; // this sensor's ID

    data["value"] = temp_motor; //(float) random(0,99);
    data["unit"] = "DEG_C";
    data["desc"] = "Motor";
    data["timestamp"] = millis();

    dataString.clear();
    serializeJson(root, dataString);
    SendData(dataString.c_str());
    Serial.print("TX3:");
    Serial.println(dataString);

    Serial.print("Cycle done, waiting 5 secs...\n");
    delay(5000);

}

void SendData(const char * json) {

    WiFiClient client;
 
    if (!client.connect(host, port)) {
      if(++hubconntries > 20) {
        Serial.println("too many HUB conn tries, rebooting");
        ESP.restart();
      }
        Serial.println("Connection to HUB failed");
 
        delay(2000);
        return;
    }


  client.print(json);
  
  client.stop();

  Serial.println("Data sent to HUB OK!");

  //delay(1000);
}

void readDS18B20(OneWire *ds, double * celsius) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  //float celsius, fahrenheit;
  
  if ( !ds->search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds->reset_search();
    delay(250);
    return;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  } 

  ds->reset();
  ds->select(addr);
  ds->write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds->reset();
  ds->select(addr);    
  ds->write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds->read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

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
  Serial.print("  Temperature = ");
  Serial.print(*celsius);
  Serial.print(" Celsius, ");
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