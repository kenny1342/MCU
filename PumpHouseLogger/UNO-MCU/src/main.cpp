/**
 * This is code on Atmel768P (Arduino Uno) module on the board, it reads temperature from LM335 on ADC0, encodes a JSON string and sends it via SW serial to ESP8266
 * 
 * Connect the SW TX/RX pins to the ESP8266 HW serial pins
 * 
 * Kenny Dec 19, 2020
 */
#include <Arduino.h>
#include <main.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <ArduinoJson.h>

// store long global string in flash (put the pointers to PROGMEM)
const char string_0[] PROGMEM = "PumpHouseLogger (MCU AT328p) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;
const char* const FIRMWARE_VERSION_LONG[] PROGMEM = { string_0 };

SoftwareSerial Serial_esp8266(2, 3); // RX, TX
static sensors_type SENSORS ;
const int interval = 500;   // interval at which to send data (millisecs)
String dataString = "no data";
StaticJsonDocument<200> doc;
 

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

  Serial.println("initializing...");
  Serial.println( (__FlashStringHelper*)pgm_read_word(FIRMWARE_VERSION_LONG + 0)) ;
  Serial.println("Made by Ken-Roger Andersen, 2020");
  Serial.println("");
  // set the data rate for the SoftwareSerial port
  Serial_esp8266.begin(115200);
  Serial_esp8266.println("uno initializing...");

  wdt_enable(WDTO_4S);

}

void loop() // run over and over
{
  
  uint8_t x = 0;
  uint16_t tmp;
  uint32_t currentMillis = 0;
  static uint32_t previousMillis = 0;
  JsonArray data;

  wdt_reset();

  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    
    previousMillis = currentMillis;

    doc.clear();
    // Add values in the document
    doc["sensor"] = "pumphouse";
    doc["id"] = "1";
    doc["firmware"] = FIRMWARE_VERSION;

    //----------- WATER PRESSURE **********

    analogReference(DEFAULT); // the default analog reference of 5 volts (on 5V Arduino boards) or 3.3 volts (on 3.3V Arduino boards)
    SENSORS.adc_waterpump = 0;    
    for(x=0; x<ADC_SAMPLE_COUNT; x++) // get average from 'samplecnt' readings
    {
      SENSORS.adc_waterpump += analogRead(ADC_CH_WATERP); //On ATmega based boards (UNO, Nano, Mini, Mega), it takes about 100 microseconds (0.0001 s) to read an analog input, so the maximum reading rate is about 10,000 times a second.
      delay(1);
    }
    SENSORS.adc_waterpump /= ADC_SAMPLE_COUNT;

    // 10-bit ADC, Bar=(ADC*BarMAX)/(2^10). Sensor drop -0.5V/bar (1024/10, minimum sensor output)
    // Integer math instead of float, factor of 100 gives 2 decimals
    tmp = ( (SENSORS.adc_waterpump * (double)PRESSURE_SENS_MAX) * 100L) / 1024;
    tmp += (int16_t)CORR_FACTOR_PRESSURE_SENSOR;

    sprintf(SENSORS.water_pressure_bar, "%d.%d", tmp / 100, tmp % 100);
    data = doc.createNestedArray("pressure_bar");

    if(tmp < 1000) { // make sure we don't exceed 4 chars (2 decimals), else we corrupt SRAM and everything crashes
      sprintf(SENSORS.water_pressure_bar, "%d.%02u", tmp / 100, tmp % 100);
      data.add(SENSORS.water_pressure_bar);
    } else {
      data.add("O/R");
    }


    //------------- TEMPERATURE ***********
    SENSORS.adc_temp_1 = 0;
    for(x=0; x<ADC_SAMPLE_COUNT; x++) // get average from 'samplecnt' readings
    {
      SENSORS.adc_temp_1 += analogRead(ADC_CH_TEMP_1); //On ATmega based boards (UNO, Nano, Mini, Mega), it takes about 100 microseconds (0.0001 s) to read an analog input, so the maximum reading rate is about 10,000 times a second.
      delay(1);
    }
    SENSORS.adc_temp_1 /= ADC_SAMPLE_COUNT;
    // Read analog voltage and convert it to Kelvin (0.488 = 500/1024) and subtract Kelvin zer (273.15)
    tmp = ((SENSORS.adc_temp_1 * 0.488) * 10L)  - 273.15;

    data = doc.createNestedArray("temp_c");

    if(tmp < 10000) { // make sure we don't exceed 4 chars (1 decimals), else we corrupt SRAM and everything crashes
      sprintf(SENSORS.temp_pumphouse, "%02d.%u", tmp / 100, tmp % 10);
      data.add(SENSORS.temp_pumphouse);
    } else {
      data.add("O/R");
    }
  
    dataString = "";
    // Generate the minified JSON
    serializeJson(doc, dataString);
    // The above line prints:
    // {"sensor":"pumphouse","id":"1","firmware":"0.13","pressure_bar":["1.62"],"temp_c":["26.0"]}

    Serial.println(dataString);
    Serial_esp8266.println(dataString);


  }

}
