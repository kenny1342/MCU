#include <Arduino.h>
#include <OneWire.h>

bool debug = true;



#define MODE_STA
//#define USE_AM2320
#define USE_DHT
#define USE_TFT

#define DHTTYPE                     DHT22   // DHT 22  (AM2302)

#define JSON_SIZE                   512
#define FIRMWARE_VERSION            "4.1"
#define PIN_DALLAS_SENSORS          25      // DS18B20 on GPIO x (a 4.7K resistor is necessary)
#define NUM_DS_SENSORS              2   // nr of DS18B20 we have on the 1-wire bus
#define PIN_DHT_SENSOR              13

#ifdef MODE_AP
const char *ssid = "KRATECH-SENSOR-HUB";  // You will connect your phone to this Access Point
const char *pw = "12345678"; // and this is the password
//IPAddress ip(192, 168, 4, 1); 
//IPAddress netmask(255, 255, 255, 0);
#else
const char *ssid = "service_wifi";  // You will connect your phone to this Access Point
const char *pw = "hemmelig"; // and this is the password

#endif


const uint16_t port = 8880;
//const char * host = "192.168.4.1";
const char * host = "SENSORHUB.local"; // mDNS name

void SendData(JsonObject &jsonObj, uint8_t mdns_index);
bool readDS18B20(OneWire *ds, bool parasitePower/*, double *celsius*/);
int getStrength(int points);

void readDS18B20();
