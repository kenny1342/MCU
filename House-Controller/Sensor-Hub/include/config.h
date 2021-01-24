#define OTA_HANDLER 
//#define MODE_AP // phone connects directly to ESP
#define MODE_STA

bool debug = true;

#define VERSION "5.10"
#define MAX_SRV_CLIENTS 4

#ifdef MODE_AP
const char *ssid = "KRATECH-SENSOR-HUB";  // You will connect your phone to this Access Point
const char *pw = "12345678"; // and this is the password
IPAddress ip(192, 168, 4, 1); 
IPAddress netmask(255, 255, 255, 0);
#else
const char *ssid = "service_wifi";  // You will connect your phone to this Access Point
const char *pw = "hemmelig"; // and this is the password

#endif

#define UART_BAUD1 19200            // Baudrate UART1
#define SERIAL_PARAM1 SERIAL_8N1    // Data/Parity/Stop UART1
#define SERIAL1_RXPIN 16            // receive Pin UART1 (DevkitC 25, GPIO16)
#define SERIAL1_TXPIN 17            // transmit Pin UART1 (DevkitC 27, GPIO17)
#define SERIAL1_TCP_PORT 8880       // Wifi Port UART1

#define bufferSize 512

//////////////////////////////////////////////////////////////////////////