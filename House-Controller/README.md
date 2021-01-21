# House Controller

## Description

**House Automation system**

*Started as a Christmas project in 2020*


## Purpose
Controlling the water pump, monitoring water/utility/temperatures etc, and provide the data in several ways:

- Push to Blynk (IoT APP), see blynk.io
- HTTP (as JSON and webinterface)
- Serial (115200)


## Hardware
Project consist of several modules/boards in the prototype and is interconnected via SPI and/or serial port.\
- ADC module (ADC-MCU): Mega2560 R3 (data collection/processing, alarming)
- Webserver (Frontend-MCU): ESP32 (WiFi/networking). 
- Sensor HUB (Sensor-Hub): ESP32
- Remote probe: ESP32

Data is collected/processed by ADC-MCU, then encoded as JSON and sent to Frontend (Serial).\
ADC-MCU also handles alarms/buzzer.\

When Frontend recieves a complete, parsable JSON string, it makes the complete JSON available via HTTP (URL /json) and a simple summary page. It also pushes the values to Blynk server.\
Token used by Blynk login must be defined in main.h (as generated in the app).
Data (temperature etc) from remote probes are sent via WiFi to Sensor-HUB (in AP mode), and relayed to ADC-MCU via SPI.\

## Firmware
Written in C++ using Arduino Core SDK, AVRDude compiler (ATmega), ESP build tools


## Tools

* Visual Code
** PlatformIO extension


## Configuration

Frontend reset of WiFi settings: press the button on the board while connecting power, hold for 5+ secs and release.\
The board will now become an AP with SSID "KRATECH-AP". Connect to it, open 192.168.1.4 in the browser and configure WiFi settings.


## Future plans
- [X] Control the water pump

![Prototype](Pics/20210102_160239.jpg)

![Board pinout](PINOUT_UNO-WiFi-R3-AT328-ESP8266-CH340G.jpg)
