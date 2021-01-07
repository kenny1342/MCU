#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <config.h>

#define MAX_SRV_CLIENTS 1
#define TCP_PORT (2323)
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpServerClients[MAX_SRV_CLIENTS];

// reading buffor config
#define BUFFER_SIZE 1024
byte buff[BUFFER_SIZE];


void setup()
{
 
  Serial.begin(115200);
 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("...");
  }
 
  Serial.print("WiFi connected with IP: ");
  Serial.println(WiFi.localIP());
 

  // Start TCP listener on port TCP_PORT
  tcpServer.begin();
  tcpServer.setNoDelay(true);
  Serial.print("Ready! Use 'telnet or nc ");
  Serial.print(WiFi.localIP());
  Serial.print(' ');
  Serial.print(TCP_PORT);
  Serial.println("' to connect");

}
 

void loop()
{
  String dataString = "";
  StaticJsonDocument<250> doc;

  uint8_t i;
  //char buf[1024];
  int bytesAvail;

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

      // read JSON data from HUB
      // TODO: Read until \n and process JSON string
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


  // --------------- SEND SENSOR DATA TO HUB ---------------------

    WiFiClient client;
 
    if (!client.connect(host, port)) {
 
        Serial.println("Connection to HUB failed");
 
        delay(2000);
        return;
    }
 
    Serial.println("Connected to HUB OK!");
 
     doc.clear();
    JsonObject root = doc.to<JsonObject>();
    
    root["cmd"] = 0x45; // NEW DATA READY
    root["id"] = 0x01; // this sensor's ID
    root["firmware"] = FIRMWARE_VERSION;
    root["IP"] = WiFi.localIP().toString();
    root["port"] = TCP_PORT;
    root["uptime_sec"] = (uint32_t) millis() / 1000;
    
    JsonArray data;
    data = doc.createNestedArray("data");
    data.add(25.0001);
    data.add(4.0001);
    data.add(99.0001);

    data = doc.createNestedArray("units");
    data.add("DEGREES_C");
    data.add("DEGREES_C");
    data.add("PERCENT");

    data = doc.createNestedArray("text");
    data.add("PumpHouse Temp");
    data.add("Inlet Temp");
    data.add("PumpHouse Hum");


    serializeJson(root, dataString);

    client.print(dataString);
 
    Serial.print("TX:");
    Serial.println(dataString);
    client.stop();
 
    delay(1000);
}
