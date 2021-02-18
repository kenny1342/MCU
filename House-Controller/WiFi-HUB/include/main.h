#include <TFT_eSPI.h>
#include <Timemark.h>

#define OTA_HANDLER 
//#define MODE_AP // phone connects directly to ESP
#define MODE_STA

bool debug = true;

#define VERSION "6.07"
#define MAX_CLIENTS 8

#ifdef MODE_AP
const char *ssid = "KRATECH-SENSOR-HUB";  // You will connect your phone to this Access Point
const char *pw = "12345678"; // and this is the password
IPAddress ip(192, 168, 4, 1); 
IPAddress netmask(255, 255, 255, 0);
#else
const char *ssid = "service_wifi";  
//const char *ssid = "MCU";  
const char *pw = "hemmelig"; 

#endif

#define UART_BAUD1 57600            // Baudrate UART1
#define SERIAL_PARAM1 SERIAL_8N1    // Data/Parity/Stop UART1
#define SERIAL1_RXPIN 16            // receive Pin UART1 (DevkitC 25, GPIO16)
#define SERIAL1_TXPIN 17            // transmit Pin UART1 (DevkitC 27, GPIO17)
#define SERIAL1_TCP_PORT 8880       // Wifi Port UART1

//#define bufferSize 512

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

int getStrength(int points);
const char *FormatBytes(long long bytes, char *str);
char * SecondsToDateTimeString(uint32_t seconds, uint8_t format);