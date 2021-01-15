#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <AM2320.h>
#include <config.h>

#define MAX_SRV_CLIENTS 1
#define TCP_PORT (2323)
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpServerClients[MAX_SRV_CLIENTS];
AM2320 am2320_pumproom(&Wire); // AM2320 sensor attached SDA, SCL

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
float tempsens1;
float humsens1;

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


    //----------- Update Water Pump temps -------------
    switch(am2320_pumproom.Read()) {
      case 2:
        Serial.println("CRC failed");
        tempsens1 = -98;
        break;
      case 1:
        Serial.println("am2320_pumproom offline");
        tempsens1 = -99;
        break;
      case 0:
        tempsens1 = am2320_pumproom.cTemp;
        humsens1 = am2320_pumproom.Humidity;
        break;
    }    

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

    data["value"] = tempsens1; //(float) random(5,39);
    data["unit"] = "DEG_C";
    data["name"] = "Pump Room";
    data["timestamp"] = millis();

    dataString.clear();
    serializeJson(root, dataString);
    SendData(dataString.c_str());
    Serial.print("TX1:");
    Serial.println(dataString);
    delay(2000);

    // ----------- HUMIDITY PUMP ROOM ------------
    root["sid"] = 0x02; // this sensor's ID

    data["value"] = humsens1; //(float) random(0,99);
    data["unit"] = "RH";
    data["name"] = "Pump Room";
    data["timestamp"] = millis();

    dataString.clear();
    serializeJson(root, dataString);
    SendData(dataString.c_str());
    Serial.print("TX2:");
    Serial.println(dataString);
    delay(2000);

    // ---------- TEMP PUMP ROOM ----------------
    root["sid"] = 0x03; // this sensor's ID

    data["value"] = (float) random(0,99);
    data["unit"] = "DEG_C";
    data["name"] = "Pump Motor";
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


// Take measurements of the Wi-Fi strength and return the average result.
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