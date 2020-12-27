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
#include <MemoryFree.h>
#include <EmonLib.h>

// store long global string in flash (put the pointers to PROGMEM)
const char string_0[] PROGMEM = "PumpController (MCU AT328p) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;
const char* const FIRMWARE_VERSION_LONG[] PROGMEM = { string_0 };

const int interval = 800;   // interval at which to send data (millisecs)
uint16_t timer1_counter; // preload of timer1

static adc_avg ADC_temp_1;
static adc_avg ADC_waterpressure;
SoftwareSerial Serial_esp8266(2, 3); // RX, TX
//static sensors_type SENSORS ;
String dataString = "no data";
StaticJsonDocument<JSON_SIZE> doc;
JsonObject json;

EnergyMonitor EMON_K2;
EnergyMonitor EMON_K3;
emonvrms_type EMONMAINS; // Vrms values (phases/gnd)
 emondata_type      EMONDATA_K2;   // Livingroom values
 emondata_type      EMONDATA_K3;   // Kitchen values
volatile appconfig_type APPCONFIG ;
volatile waterpump_type WATERPUMP;
volatile flags_BitField APPFLAGS;
volatile alarm_BitField ALARMS;// = (volatile BitField*)&ADMUX; //declare a BitField variable which is located at the address of ADMUX.

void setup()
{
  APPFLAGS.is_busy = 1;
  pinMode(ADC_CH_VOLT_N_GND, INPUT);
  pinMode(ADC_CH_WATERP, INPUT);
  pinMode(ADC_CH_VOLT_L_GND, INPUT);
  pinMode(ADC_CH_VOLT_L_N, INPUT);
  pinMode(ADC_CH_TEMP_1, INPUT);
  pinMode(ADC_CH_CT_K2, INPUT);
  pinMode(ADC_CH_CT_K3, INPUT);

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

  Serial.println(F("initializing..."));
  Serial.println( (__FlashStringHelper*)pgm_read_word(FIRMWARE_VERSION_LONG + 0)) ;
  Serial.println(F("Made by Ken-Roger Andersen, 2020"));
  Serial.println("");


  APPCONFIG.wp_max_runtime = DEF_CONF_WP_MAX_RUNTIME;
  APPCONFIG.wp_suspendtime = DEF_CONF_WP_SUSPENDTIME;
  APPCONFIG.wp_lower = DEF_CONF_WP_LOWER;
  APPCONFIG.wp_upper = DEF_CONF_WP_UPPER;
  APPCONFIG.min_temp_pumphouse = DEF_CONF_MIN_TEMP_PUMPHOUSE;

  // set the data rate for the SoftwareSerial port
  Serial_esp8266.begin(115200);
  Serial_esp8266.println(F("uno initializing..."));

  // initialize all ADC readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    ADC_temp_1.readings[thisReading] = 0;
    ADC_waterpressure.readings[thisReading] = 0;
  }

  analogReference(DEFAULT); // the default analog reference of 5 volts (on 5V Arduino boards) or 3.3 volts (on 3.3V Arduino boards)

  // Timer 1 - 0.5 Hz overflow ISR
  noInterrupts();          // disable global interrupts

  // Setup Timer1 for 500 ms (0.5 Hz) overlow ISR
  TCCR1A = 0;     // set entire TCCR1A register to 0 (no PWM)
  TCCR1B = 0;     // same for TCCR1B
  
  // Set timer1_counter to the correct value for our interrupt interval
  timer1_counter = 34286;   // preload timer 65536-16MHz/256/2Hz, 65536-16MHz/1024/0.5Hz
  
  TCNT1 = timer1_counter;   // preload timer
  // clear:   TCCR1B &= ~(1 << CS10); set:      TCCR1B |= (1 << CS10); toggle: TCCR1B ^= (1 << CS10);
  
  // BUG: 256 prescaler crashes on Uno R3 DIL board
  //TCCR1B |= (1 << CS12);    // 256 prescaler 
  // Set CS10 and CS12 bits for 1024 prescaler:  
  TCCR1B |= (1 << CS12);
  TCCR1B |= (1 << CS10);
  
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt

  // Timer 2 - gives us our 1 ms (1000 Hz) overflow ISR
  // 16 MHz clock (62.5 ns per tick) - prescaled by 128 - counter increments every 8 µs. So we count 125 of them, giving exactly 1000 µs (1 ms)
  TCCR2A = 0x00;
  TCCR2B = 0x00;
  TCCR2A = bit (WGM21) ;   // CTC mode
  OCR2A  = 124;            // count up to 125  (zero relative!!!!)
  TCNT2 = 0;      // counter to zero
  // Reset prescalers
  GTCCR = bit (PSRASY);        // reset prescaler now
  // start Timer 2
  TCCR2B =  bit (CS20) | bit (CS22) ;  // prescaler of 128
  TIMSK2 = bit (OCIE2A);   // enable Timer2 Interrupt
  interrupts(); // enable global interrupts
  
  Serial.println(F("ISR's enabled"));
  delay(500);
    
  //EMONDATA_K2.ADC_CH_CT = ADC_CH_CT_K2;
  //EMONDATA_K2.ADC_CH_CT_VGND = ADC_CH_CT_K2_VGND;
  EMON_K2.current(ADC_CH_CT_K2, DEF_EMON_ICAL_K2);
  EMON_K3.current(ADC_CH_CT_K3, DEF_EMON_ICAL_K3);

  wdt_enable(WDTO_4S);
  APPFLAGS.is_busy = 0;

}

ISR (TIMER1_OVF_vect) // interrupt service routine, 0.5 Hz
{
  //char *endptr;

  TCNT1 = timer1_counter;   // preload timer
  if(millis() < 5000) return; // just powered up, let things stabilize

  /**** Water pump state logic ****/
  //Serial.println(strtoul(SENSORS.water_pressure_bar, &endptr, 10));
/* Serial.print( SENSORS.water_pressure_bar_val);
 Serial.print(" <= ");
 Serial.print(APPCONFIG.wp_lower);
 Serial.print(" && ");
 Serial.print(ADC_waterpressure.average);
 Serial.println(" > 0");
*/
  if(!WATERPUMP.is_running) {
    if(WATERPUMP.water_pressure_bar_val <= APPCONFIG.wp_lower && ADC_waterpressure.average > 0) { // startbelow lower threshold AND valid ADC reading (if we get here before alarm ISR has updated alarms)
      if(!IS_ACTIVE_ALARMS_WP()) { // Do not start if any of these alarms are active
        if(WATERPUMP.suspend_timer == 0 || WATERPUMP.suspend_timer > APPCONFIG.wp_suspendtime) { // Do not start if still suspended after prev alarams
          WATERPUMP.is_running = 1; // START waterpump in main loop if not running
          WATERPUMP.state_age=0; //reset state age
          WATERPUMP.start_counter++; // increase the start counter 
        } else 
          Serial.println(F("SUSP: NOT starting WP"));
      } else
        Serial.println(F("ALARMS: NOT starting WP"));
    }

  } else { // is running
    if(WATERPUMP.water_pressure_bar_val >= APPCONFIG.wp_upper || IS_ACTIVE_ALARMS_WP() ) { // stop if reached upper threshold OR alarms active     
      WATERPUMP.is_running = 0; // STOP waterpump in main loop if running
      WATERPUMP.state_age=0; //reset state age
    }
  }

}



//******************************************************************
//  Timer2 Interrupt Service is invoked by hardware Timer 2 every 1 ms = 1000 Hz
//  16Mhz / 128 / 125 = 1000 Hz

ISR (TIMER2_COMPA_vect) 
{
  static uint16_t t2_overflow_cnt;//, t2_overflow_cnt_500ms;
  bool start_wp_suspension=0; // set to 1 if we clear an previous active waterpump related alarm (one of ALARMBITS_WATERPUMP_PROTECTION)
  intervals_type intervals;
  //char *endptr;

  digitalWrite(PIN_LED_BUSY, APPFLAGS.is_busy);

  if(millis() < 5000) return; // just powered up, let things stabilize
  

  t2_overflow_cnt++; // will rollover, that's fine
  //intervals.allBits = 0x00; // reset all
  //TODO: optimize (remember to declare intervals as static), modulus is quite slow, but it is easy to read and ok for now
  intervals._2sec = !(t2_overflow_cnt % 2000);
  intervals._1sec = !(t2_overflow_cnt % 1000);
  intervals._500ms = !(t2_overflow_cnt % 500);
  intervals._200ms = !(t2_overflow_cnt % 200);
  intervals._100ms = !(t2_overflow_cnt % 100);

  if(intervals._1sec) {
    WATERPUMP.state_age++; 
    if(WATERPUMP.is_running) {
      WATERPUMP.total_runtime++; 
    }

    if(ALARMS.low_memory) {
      Serial.print(F("LOW MEM: "));
      Serial.println(freeMemory());
    }
  }

  // Control Alarm LED
  if(IS_ACTIVE_ALARMS_WP() || WATERPUMP.is_suspended) {
    if(WATERPUMP.is_suspended && intervals._1sec) // In suspension period, blink alarm LED every 1000ms/1 sec
    {     
      //asm ("sbi %0, %1 \n": : "I" (_SFR_IO_ADDR(PIND)), "I" (PIND5)); // use 2 cycles to toggle pin (PIND5=digital port 2 on Uno board)
      digitalWrite(PIN_LED_ALARM, !digitalRead(PIN_LED_ALARM));
    }
    else if(getAlarmStatus(ALARMS.allBits) && intervals._200ms) // Active alarm, blink alarm LED every 200ms/0.2 sec
    {      
      //asm ("sbi %0, %1 \n": : "I" (_SFR_IO_ADDR(PIND)), "I" (PIND5)); // use 2 cycles to toggle pin
      digitalWrite(PIN_LED_ALARM, !digitalRead(PIN_LED_ALARM));
    }       
  } else {
    digitalWrite(PIN_LED_ALARM, 0); // no alarms or suspension active
  }
  
  if(intervals._100ms) // every 0.1 sec (OBS! Keep under 1000 (1 sec) for the seconds-counters in alarms to work)
  {

    /*** SET/CLEAR ALARMS START ***/
    // All alarms should be set/cleared here, because if they are part of ALARMBITS_WATERPUMP_PROTECTION, this is where the suspension timer is handled!

    // Check free RAM (less than xxx here means erratic behaviour/weird random stuff happens!) (on a 328p at least)
    // OBS! here we have 5-6 bytes less mem free than where we print it in debug output  
    if(freeMemory() < (int)LOWMEM_LIMIT) {
      ALARMS.low_memory = 1;
    } else {
        if(ALARMS.low_memory) start_wp_suspension = 1; // was active until now, start suspension period

      ALARMS.low_memory = 0; 
    }

    // Only update alarms (for external data) if not currently reading new ADC values/updating data, we might get zero/invalid results until it's completed
    if(!APPFLAGS.is_updating) {

      /////// Sensor alarms ////////////////
      ALARMS.sensor_error = 0;

      ALARMS.emon_K2 = 0;
      ALARMS.emon_K3 = 0;
      if(EMONDATA_K2.error || EMONDATA_K2.Irms > 20.0 || EMONDATA_K2.Irms < 0.0) {
          ALARMS.emon_K2 = 1;
      }
      if(EMONDATA_K3.error || EMONDATA_K3.Irms > 20.0 || EMONDATA_K3.Irms < 0.0) {
          ALARMS.emon_K3 = 1;
      }

      // Groundfaults
      ALARMS.power_groundfault = 0;
      // Jordfeil i et 230V IT nett når spenningen fase jord er  mindre en 90V eller større en 170V (130 +- 40V)	
      if(EMONMAINS.Vrms_L_GND < 90.0 || EMONMAINS.Vrms_N_GND < 90.0 || EMONMAINS.Vrms_L_GND > 170.0 || EMONMAINS.Vrms_N_GND > 170.0) {
        ALARMS.power_groundfault = 1; 
      }


      if(ADC_temp_1.average == 0 || ADC_temp_1.average > 800) { // if sensor (LM335) is disconnected ADC gives very high readings
        ALARMS.sensor_error = 1;
      } else {    
         if(ALARMS.sensor_error) start_wp_suspension = 1; // was active until now, start suspension period
      }

      if(ADC_waterpressure.average == 0) {
        ALARMS.sensor_error = 1;
      } else {    
          if(ALARMS.sensor_error) start_wp_suspension = 1; // was active until now, start suspension period
      }

      /////// Water pump and related alarms ///////
      if(WATERPUMP.temp_pumphouse_val < APPCONFIG.min_temp_pumphouse) {
        ALARMS.temperature_pumphouse = 1;
      } else {    
          if(ALARMS.temperature_pumphouse) start_wp_suspension = 1; // was active until now, start suspension period

        ALARMS.temperature_pumphouse = 0;
      }
        

      if(WATERPUMP.is_running && WATERPUMP.state_age > (uint32_t)APPCONFIG.wp_max_runtime) { // have we run to long?
        ALARMS.waterpump_runtime = 1;
      } else {    
        if(ALARMS.waterpump_runtime) start_wp_suspension = 1; // was active until now, start suspension period

        ALARMS.waterpump_runtime = 0;
      }


      // TODO: ALARMS.power_voltage
      // TODO: ALARMS.power_groundfault

      /*** SET/CLEAR ALARMS DONE ***/
    }
    //Serial.print("IS_ACTIVE_ALARMS_WP:");
    //Serial.println(IS_ACTIVE_ALARMS_WP());
    //Serial.print(", start_wp_suspension=");
    //Serial.println(start_wp_suspension);
//Serial.println(ALARMS.allBits, BIN);

    /*** Update WP suspension timer (Note: every time an ALARMBITS_WATERPUMP_PROTECTION alarm is cleared, we RESTART suspension period, this is intended design) ***/
    //if(!getAlarmStatus(ALARMBITS_WATERPUMP_PROTECTION)) { // No active WP alarms, it's ok to start or continue suspension period
    if(!IS_ACTIVE_ALARMS_WP()) { // No active WP alarms, it's ok to start or continue suspension period
    //if( !(ALARMS.low_memory || ALARMS.sensor_error || ALARMS.temperature_pumphouse || ALARMS.waterpump_runtime) ) {
      if(start_wp_suspension) {// we just cleared an WP alarm, start suspension timer from 1  
          WATERPUMP.suspend_timer = 1; 
          WATERPUMP.suspend_count++; // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
        } else { // see if we should increase or disable suspension timer
          if(WATERPUMP.suspend_timer > 0L && WATERPUMP.suspend_timer < APPCONFIG.wp_suspendtime) { // suspension active and still not reached timeout
            if(intervals._1sec) { WATERPUMP.suspend_timer++; }
          } else if(WATERPUMP.suspend_timer >= DEF_CONF_WP_SUSPENDTIME)  {// Suspension period is over
            WATERPUMP.suspend_timer = 0; // disable suspension timer
          }
        }
      }
    WATERPUMP.is_suspended = WATERPUMP.suspend_timer != 0 ? true : false; // if suspend_timer != 0, we are suspended, update flag

  }
}


void loop() // run over and over
{
    
  //double tmp;
  //uint32_t currentMillis = 0;
  static uint32_t previousMillis = 0;
  JsonArray data;

  wdt_reset();

  //currentMillis = millis();
  if (millis() - previousMillis >= interval) {
    
    previousMillis = millis();

    // Initiate the JSON doc
    doc.clear();
    JsonObject root = doc.to<JsonObject>();
    // Add values in the document
    ///root["sensor"] = "pumphouse";
    //root["id"] = "1";
    root["firmware"] = FIRMWARE_VERSION;

    APPFLAGS.is_updating = 1;
    
    //----------- EMON **********
    // TODO: read from voltage sensors
    EMONMAINS.Vrms_L_N = zmpt101bReadVoltage(ADC_CH_VOLT_L_N);
    EMONMAINS.Vrms_L_GND = zmpt101bReadVoltage(ADC_CH_VOLT_L_GND);
    EMONMAINS.Vrms_N_GND = zmpt101bReadVoltage(ADC_CH_VOLT_N_GND);
    
    //EMONMAINS.Vrms_L_GND = 110.0;
    //EMONMAINS.Vrms_N_GND = 110.0;
    //EMONMAINS.Vrms_L_N = 240.0;
    
    //calcEMON(EMON_K2, 8);  // Calculate Irms etc
    //calcEMON(EMON_K3, 8);  // Calculate Irms etc
    EMONDATA_K2.Irms = EMON_K2.calcIrms(500);
    EMONDATA_K2.apparentPower = (EMONMAINS.Vrms_L_N * EMONDATA_K2.Irms);
    EMONDATA_K3.Irms = EMON_K3.calcIrms(500);
    EMONDATA_K3.apparentPower = (EMONMAINS.Vrms_L_N * EMONDATA_K3.Irms);

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
    //tmp = ( ( (double)(ADC_waterpressure.average * (double)PRESSURE_SENS_MAX) / 1024L));
    //tmp += (double)CORR_FACTOR_PRESSURE_SENSOR;
    WATERPUMP.water_pressure_bar_val = ( ( (double)(ADC_waterpressure.average * (double)PRESSURE_SENS_MAX) / 1024L));
    WATERPUMP.water_pressure_bar_val += (double)CORR_FACTOR_PRESSURE_SENSOR;

    data = doc.createNestedArray("pressure_bar");
    
    //dtostrf(SENSORS.water_pressure_bar_val, 3, 1, SENSORS.water_pressure_bar ); // TODO: IS THIS (CHAR VALUE) NEEDED ANYMORE?
    data.add(WATERPUMP.water_pressure_bar_val);

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
    //tmp = (double) (((5000.0 * (double)ADC_temp_1.average) / 1024L) / 10L) - 273.15;
    WATERPUMP.temp_pumphouse_val = (double) (((5000.0 * (double)ADC_temp_1.average) / 1024L) / 10L) - 273.15; // as float value
    //dtostrf(SENSORS.temp_pumphouse_val, 3, 1, SENSORS.temp_pumphouse ); // TODO: IS THIS (CHAR VALUE) NEEDED ANYMORE?
    data = doc.createNestedArray("temp_c");
    data.add(WATERPUMP.temp_pumphouse_val);


    data = doc.createNestedArray("alarms");

    if(getAlarmStatus(ALARMS.allBits)) { // any alarm bit set?
      if(ALARMS.emon_K2) data.add(F("K2"));
      if(ALARMS.emon_K3) data.add(F("K3"));
      if(ALARMS.low_memory) data.add(F("lowmem"));
      if(ALARMS.waterpump_runtime) data.add(F("wpruntime"));
      if(ALARMS.temperature_pumphouse) data.add(F("temp_pumphouse"));
      if(ALARMS.sensor_error) data.add(F("sensor"));
      if(ALARMS.power_groundfault) data.add(F("groundfault"));
      if(ALARMS.power_voltage) data.add(F("emon_voltage"));
    }
    

    APPFLAGS.is_updating = 0;
    doc["emon_vrms_L_N"] = EMONMAINS.Vrms_L_N;
    doc["emon_vrms_L_GND"] = EMONMAINS.Vrms_L_GND;
    doc["emon_vrms_N_GND"] = EMONMAINS.Vrms_N_GND;

    json = root.createNestedObject("WP");

    json["t_state"] = WATERPUMP.state_age;
    json["is_running"] = WATERPUMP.is_running;
    json["is_suspended"] = WATERPUMP.is_suspended;
    json["cnt_starts"] = WATERPUMP.start_counter;
    json["cnt_susp"] = WATERPUMP.suspend_count;
    json["t_susp"] = WATERPUMP.suspend_timer;
    json["t_totruntime"] = WATERPUMP.total_runtime;

    json = root.createNestedObject("K2");
    json["I"] = EMONDATA_K2.Irms;
    json["P_a"] = EMONDATA_K2.apparentPower;
    json["PF"] = EMONDATA_K2.PF;
    

    json = root.createNestedObject("K3");
    json["I"] = EMONDATA_K3.Irms;
    json["P_a"] = EMONDATA_K3.apparentPower;
    json["PF"] = EMONDATA_K3.PF;

    dataString = "";
    // Generate the minified JSON
    serializeJson(root, dataString);
    // Outputs something like:
    // {"sensor":"pumphouse","id":"1","firmware":"2.01","pressure_bar":[3.966797],"temp_c":[25.67813],"alarms":[],"WP":{"t_state":32,"is_running":0,"is_suspended":false,"cnt_starts":0,"cnt_susp":1,"t_susp":0,"t_totruntime":0},"K2":{"I":29.6936,"P_a":0,"P_r":0,"V":230,"PF":0},"K3":{"I":21.15884,"P_a":0,"P_r":0,"V":230,"PF":0}}

    Serial.println(dataString);
    Serial_esp8266.println(dataString);
    
    Serial.print(F("mem:"));
    Serial.println(freeMemory());

    // DEBUG
/*
    Serial.print(F("WP: ")); 
    //Serial.print (WATERPUMP.pressure_bar); 
    Serial.print (SENSORS.water_pressure_bar_val);
    //Serial.print(F(".")); 
    //Serial.print (WATERPUMP.pressure_bar%100, DEC);  
    Serial.print(F(" Bar, "));
    //ModIOGetRelayStatus(CONF_RELAY_WP) ? Serial.print(F("ON for ")) : Serial.print(F("OFF for "));
    WATERPUMP.is_running ? Serial.print(F("ON for ")) : Serial.print(F("OFF for "));
    Serial.print(WATERPUMP.state_age);
    Serial.print(F(" secs, starts:"));
    Serial.print(WATERPUMP.start_counter);
    Serial.print(F(", tot_runtime:"));
    Serial.print(WATERPUMP.total_runtime);    
    Serial.print(F(", is_susp:"));
    Serial.print(WATERPUMP.is_suspended);
    Serial.print(F(", susptmr:"));
    Serial.print(WATERPUMP.suspend_timer);
    Serial.print(F(", suspcnt:"));
    Serial.print(WATERPUMP.suspend_count); // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
    Serial.println("");
    */
  }

}

/**
* check if an alarm bit is set or not
*/
bool getAlarmStatus(uint8_t Alarm) {

  return (ALARMS.allBits & Alarm);
}

void calcEMON(emondata_type &emon, uint8_t Number_of_Samples) {

}


long readVcc() {
  long result;

  #if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega644__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openenergymonitor.org/emon/node/2253#comment-11432
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);

  #endif


  #if defined(__AVR__)
  delay(2);                                        // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);                             // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = READVCC_CALIBRATION_CONST / result;  //1100mV*1024 ADC steps http://openenergymonitor.org/emon/node/1186
  return result;
  #elif defined(__arm__)
  return (3300);                                  //Arduino Due
  #else
  return (3300);                                  //Guess that other un-supported architectures will be running a 3.3V!
  #endif
}


double zmpt101bReadVoltage(uint8_t adc_channel) {
  double sensorValue1 = 0;
  //double sensorValue2 = 0;
  //int crosscount = 0;
  //int climb_flag = 0;
  int val[100];
  int max_v = 0;
  double VmaxD = 0;
  double VeffD = 0;
  double Veff = 0;

    for ( int i = 0; i < 100; i++ ) {
    sensorValue1 = analogRead(adc_channel);
    if (analogRead(adc_channel) > 511) {
      val[i] = sensorValue1;
    }
    else {
      val[i] = 0;
    }
    delay(1);
  }

  max_v = 0;

  for ( int i = 0; i < 100; i++ )
  {
    if ( val[i] > max_v )
    {
      max_v = val[i];
    }
    val[i] = 0;
  }
  if (max_v != 0) {


    VmaxD = max_v;
    VeffD = VmaxD / sqrt(2);
    Veff = (((VeffD - 420.76) / -90.24) * -210.2) + 210.2;
  }
  else {
    Veff = 0;
  }
  //Serial.print("Voltage: ");
  //Serial.println(Veff);
  VmaxD = 0;

  return Veff;
}