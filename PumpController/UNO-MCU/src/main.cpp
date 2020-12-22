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
const char string_0[] PROGMEM = "PumpController (MCU AT328p) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;
const char* const FIRMWARE_VERSION_LONG[] PROGMEM = { string_0 };


static adc_avg ADC_temp_1;
static adc_avg ADC_waterpressure;
SoftwareSerial Serial_esp8266(2, 3); // RX, TX
static sensors_type SENSORS ;
const int interval = 800;   // interval at which to send data (millisecs)
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

  // initialize all ADC readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    ADC_temp_1.readings[thisReading] = 0;
    ADC_waterpressure.readings[thisReading] = 0;
  }

  analogReference(DEFAULT); // the default analog reference of 5 volts (on 5V Arduino boards) or 3.3 volts (on 3.3V Arduino boards)

  wdt_enable(WDTO_4S);

}

void loop() // run over and over
{
    
  double tmp;
  uint32_t currentMillis = 0;
  static uint32_t previousMillis = 0;
  JsonArray data;

  wdt_reset();

  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    
    previousMillis = currentMillis;

    // Initiate the JSON doc
    doc.clear();
    // Add values in the document
    doc["sensor"] = "pumphouse";
    doc["id"] = "1";
    doc["firmware"] = FIRMWARE_VERSION;

    //----------- WATER PRESSURE **********

    ADC_waterpressure.total -= ADC_waterpressure.readings[ADC_waterpressure.readIndex];
    ADC_waterpressure.readings[ADC_waterpressure.readIndex] = analogRead(ADC_CH_WATERP); //On ATmega based boards (UNO, Nano, Mini, Mega), it takes about 100 microseconds (0.0001 s) to read an analog input, so the maximum reading rate is about 10,000 times a second.
    ADC_waterpressure.total += ADC_waterpressure.readings[ADC_waterpressure.readIndex];
    ADC_waterpressure.readIndex += 1;

    if (ADC_waterpressure.readIndex >= numReadings) {
      ADC_waterpressure.readIndex = 0;
    }

    ADC_waterpressure.average = ADC_waterpressure.total / numReadings;

    // 10-bit ADC, Bar=(ADC*BarMAX)/(2^10). Sensor drop -0.5V/bar (1024/10, minimum sensor output)
    // Integer math instead of float, factor of 100 gives 2 decimals
    tmp = ( ( (double)(ADC_waterpressure.average * (double)PRESSURE_SENS_MAX) / 1024L));
    tmp += (double)CORR_FACTOR_PRESSURE_SENSOR;

    data = doc.createNestedArray("pressure_bar");
    
    dtostrf(tmp, 3, 1, SENSORS.water_pressure_bar );
    data.add(SENSORS.water_pressure_bar);

    //------------- TEMPERATURE ***********

    ADC_temp_1.total -= ADC_temp_1.readings[ADC_temp_1.readIndex];
    ADC_temp_1.readings[ADC_temp_1.readIndex] = analogRead(ADC_CH_TEMP_1); //On ATmega based boards (UNO, Nano, Mini, Mega), it takes about 100 microseconds (0.0001 s) to read an analog input, so the maximum reading rate is about 10,000 times a second.
    ADC_temp_1.total += ADC_temp_1.readings[ADC_temp_1.readIndex];
    ADC_temp_1.readIndex += 1;

    if (ADC_temp_1.readIndex >= numReadings) {
      ADC_temp_1.readIndex = 0;
    }

    ADC_temp_1.average = ADC_temp_1.total / numReadings;

    // LM335 outputs mv/Kelvin, convert ADC to millivold and subtract Kelvin zero (273.15) to get Celsius
    tmp = (double) (((5000.0 * (double)ADC_temp_1.average) / 1024L) / 10L) - 273.15;
    
    dtostrf(tmp, 3, 1, SENSORS.temp_pumphouse );
    data = doc.createNestedArray("temp_c");
    data.add(SENSORS.temp_pumphouse);

    dataString = "";
    // Generate the minified JSON
    serializeJson(doc, dataString);
    // The above line prints:
    // {"sensor":"pumphouse","id":"1","firmware":"0.13","pressure_bar":["1.62"],"temp_c":["26.0"]}

    Serial.println(dataString);
    Serial_esp8266.println(dataString);


  }

}