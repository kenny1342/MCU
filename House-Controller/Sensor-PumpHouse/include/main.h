#include <Arduino.h>
#include <OneWire.h>

bool debug = true;

#define MODE_STA

#define FIRMWARE_VERSION            "4.0"
#define PIN_SENSOR_TEMP_MOTOR       25      // DS18B20 on GPIO 10 (a 4.7K resistor is necessary)

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

void SendData(const char * json, const uint8_t mdns_index);
bool readDS18B20(OneWire *ds, double *celsius);
int getStrength(int points);
void printAddress(DeviceAddress deviceAddress);