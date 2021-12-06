#include <TFT_eSPI.h>
#include <Timemark.h>
#include <ESPAsyncWebServer.h>

#define OTA_HANDLER 
//#define MODE_AP // phone connects directly to ESP
#define MODE_STA

bool debug = true;

#define VERSION         "7.04"
#define WEBIF_VERSION   "2.02"
#define AUTHOR_TEXT     "Ken-Roger Andersen (Nov, 2021)"
#define WDT_TIMEOUT 30
#define MAX_CLIENTS 8

#ifdef MODE_AP
const char *ssid = "KRATECH-SENSOR-HUB";  // You will connect your phone to this Access Point
const char *pw = "12345678"; // and this is the password
IPAddress ip(192, 168, 4, 1); 
IPAddress netmask(255, 255, 255, 0);
#else
const char *ssid = "kra-stb";  
const char *pw = "escort82"; 
#endif

#define BUF_SIZE 300                // max expected length of JSON strings recieved from sensors

const int timeZone = 2;
unsigned int localPortNTP = 55123;  // local port to listen for UDP packets
#define NTPPSERVER   "europe.pool.ntp.org"
#define MAX_AGE_SID                 3600 // seconds we keep a devid/sid probe data in memory before deleting (dead remote probe)
#define MAX_REMOTE_SIDS 12          // memory slots to hold sensor data (SID's)

#define UART_BAUD1 57600            // Baudrate UART1
#define SERIAL_PARAM1 SERIAL_8N1    // Data/Parity/Stop UART1
#define SERIAL1_RXPIN 16            // receive Pin UART1 (DevkitC 25, GPIO16)
#define SERIAL1_TXPIN 17            // transmit Pin UART1 (DevkitC 27, GPIO17)
#define SERIAL1_TCP_PORT 8880       // Wifi Port UART1

struct CLIENT_struct {
    uint16_t conntime;
    uint16_t bytes_rx;
    Timemark tm;
};

//ST7789 OLED display
#define txtsize 2

struct LCD_state_struct {
    bool clear = true; // flag to clear display
    //bool textwrap = true;
    uint16_t bgcolor = TFT_BLACK;
    uint16_t fgcolor = TFT_GREEN;    
} ;

//////////////////////////////////////////////////////////////////////////
enum TIME_STRING_FORMATS { TFMT_LONG, TFMT_DATETIME, TFMT_HOURS };

void processSensorData(char *data_string);
int getStrength(int points);
const char *FormatBytes(long long bytes, char *str);
char * SecondsToDateTimeString(uint32_t seconds, uint8_t format);

String HTMLProcessor(const String& var);

void onRequest(AsyncWebServerRequest *request);
void onNotFound(AsyncWebServerRequest *request);
void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

char * SecondsToDateTimeString(uint32_t seconds, uint8_t format);
time_t sync();
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
String listFiles(bool ishtml);
String humanReadableSize(const size_t bytes);