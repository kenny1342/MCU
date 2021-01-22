#include <Arduino.h>
#include <OneWire.h>

bool debug = true;

#define FIRMWARE_VERSION            "4.0"
#define PIN_SENSOR_TEMP_MOTOR       34      // DS18B20 on GPIO 10 (a 4.7K resistor is necessary)

// For AP mode:
const char *ssid = "KRATECH-SENSOR-HUB";  // You will connect your phone to this Access Point
const char *password = "12345678"; // and this is the password
//IPAddress ip(192, 168, 4, 2); // From RoboRemo app, connect to this IP
//IPAddress netmask(255, 255, 255, 0);
const uint16_t port = 8880;
const char * host = "192.168.4.1";

void SendData(const char * json);
void readDS18B20(OneWire *ds, double *celsius);
int getStrength(int points);