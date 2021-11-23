#include <Arduino.h>
#include <OneWire.h>

bool debug = true;

#define HW_ESP32_OLEDSSD1306_DHT 2 // ESP32 board with 0.96" OLED (fex Lilygo LoraWan board, some devkits etc) + DHT11/DHT22/AM2302 sensor (pumproom)
#define HW_ESP32_DHT             3 // ESP32 board + DHT11/DHT22/AM2302 sensor (bedroom, outside)
#define HW_ESP32_AM2320          1 // ESP32 board minimal/devkit c + AM2320 I2C sensor (bathroom)


#define HW_MODEL HW_ESP32_DHT
//#define HW_MODEL HW_ESP32_AM2320
//#define HW_MODEL HW_ESP32_OLEDSSD1306_DHT

#if HW_MODEL == HW_ESP32_AM2320
#define USE_AM2320 // OBS - doesn't mix with OLED SPI display SSD1306
#endif

#if HW_MODEL == HW_ESP32_OLEDSSD1306_DHT
#define USE_DHT
#define USE_SSD1306
#define PIN_INTERNAL_SENSOR         13
#define DHTTYPE                     DHT22   // DHT 22  (AM2302)
#endif

#if HW_MODEL == HW_ESP32_DHT
#define USE_DHT
#define PIN_INTERNAL_SENSOR         13
#define DHTTYPE                     DHT22   // DHT 22  (AM2302)
#endif

#define MODE_STA

#define JSON_SIZE                   512
#define FIRMWARE_VERSION            "5.01"
#define PIN_DALLAS_SENSORS          25      // DS18B20 on GPIO x (a 4.7K pullup is necessary)
#define NUM_DS_SENSORS              5       // Max nr of DS18B20 we can have on the 1-wire bus

//******** WiFi connection details **********/
#ifdef MODE_AP
const char *ssid = "KRATECH-SENSOR-HUB";  // You will connect your phone to this Access Point
const char *pw = "12345678"; // and this is the password
//IPAddress ip(192, 168, 4, 1); 
//IPAddress netmask(255, 255, 255, 0);
#else
const char *ssid = "kra-stb";  // You will connect your phone to this Access Point
const char *pw = "escort82"; // and this is the password
#endif

//******** HUB connection details **********/
const uint16_t hub_port = 8880;
const char * hub_host = "SENSORHUB.local"; // mDNS name or IP
const char * hub_ip_fallback = "192.168.30.60"; // Hub IP if no mDNS found

void SendData(JsonObject &jsonObj, int8_t mdns_index);
bool readDS18B20(OneWire *ds, bool parasitePower/*, double *celsius*/);
int getStrength(int points);

void readDS18B20();
