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
#include <Wire.h>
#include <avr/wdt.h>
#include <ArduinoJson.h>
#include <MemoryFree.h>
//#include <EmonLib.h>
#include <ZMPT101B.h>
#include <olimex-mod-io.h>
#include <Timemark.h>

// store long global string in flash (put the pointers to PROGMEM)
const char string_0[] PROGMEM = "PumpController (MCU AT328p) v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;
const char* const FIRMWARE_VERSION_LONG[] PROGMEM = { string_0 };

//const int interval = 800;   // interval at which to send data (millisecs)
uint16_t timer1_counter; // preload of timer1
String dataString = "no data";



//static adc_avg ADC_temp_1;
static adc_avg ADC_waterpressure;
//SoftwareSerial Serial_Frontend(2, 3); // RX, TX
#define Serial_Frontend Serial1
#define Serial_SensorHub Serial2

StaticJsonDocument<JSON_SIZE> doc;
StaticJsonDocument<JSON_SIZE> tmp_json; // Parsing input serial data from REMOTE_SENSORS
//JsonObject json;

//EnergyMonitor EMON_K2;  // instance used for calcIrms()
//EnergyMonitor EMON_K3;  // instance used for calcIrms()

ZMPT101B voltageSensor_L_N(ADC_CH_VOLT_L_N);
ZMPT101B voltageSensor_L_PE(ADC_CH_VOLT_L_PE);
ZMPT101B voltageSensor_N_PE(ADC_CH_VOLT_N_PE);

// Structs
volatile Buzzer buzzer;
emonvrms_type EMONMAINS; // Vrms values (phases/gnd)
emondata_type EMONDATA_K2;   // Livingroom values
emondata_type EMONDATA_K3;   // Kitchen values
volatile appconfig_type APPCONFIG ;
volatile waterpump_type WATERPUMP;
volatile flags_BitField APPFLAGS;
volatile alarm_BitField_WP ALARMS_WP;     // Waterpump alarms
volatile alarm_BitField_EMON ALARMS_EMON; // Utility/EMON alarms
volatile alarm_BitField_SYS ALARMS_SYS;   // local system/MCU alarms

Timemark tm_blinkAlarm(200);
Timemark tm_blinkWarning(500);
Timemark tm_WP_seconds_counter(1000);
Timemark tm_counterWP_suspend_1sec(1000);
Timemark tm_100ms_setAlarms(100);
Timemark tm_DataTX(DATA_TX_INTERVAL);
Timemark tm_buzzer(0);
Timemark tm_DataStale_50406_1(120000); // We expect Pump House temperature updated within 2 min, if not we clear/alarm it

void setup()
{

  APPFLAGS.is_busy = 1;

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_ModIO_Reset, INPUT); // need to float
  
  LED_ON(PIN_LED_RED);
  delay(300);
  LED_ON(PIN_LED_YELLOW);
  delay(300);
  LED_ON(PIN_LED_BLUE);
  delay(300);
  LED_ON(PIN_LED_WHITE);
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

  Serial.println(F("initializing..."));
  Serial.println( (__FlashStringHelper*)pgm_read_word(FIRMWARE_VERSION_LONG + 0)) ;
  Serial.println(F("Made by Ken-Roger Andersen, 2020"));
  Serial.println("");


  analogReference(DEFAULT); // the default analog reference of 5 volts (on 5V Arduino boards) or 3.3 volts (on 3.3V Arduino boards)

  #ifdef DO_VOLTAGE_CALIBRATION
  while(1) {
    Serial.println(F("read calib/zero-point voltage sensors..."));
    Serial.print(F("L_N:'")); Serial.print(voltageSensor_L_N.calibrate());
    Serial.print(F(", L_PE:'")); Serial.print(voltageSensor_L_PE.calibrate());
    Serial.print(F(", N_PE:'")); Serial.print(voltageSensor_N_PE.calibrate());
    delay(1000);
  }
  #endif

  voltageSensor_L_N.setZeroPoint(ZERO_POINT_L_N);
  voltageSensor_L_PE.setZeroPoint(ZERO_POINT_L_PE);
  voltageSensor_N_PE.setZeroPoint(ZERO_POINT_N_PE);
  voltageSensor_L_N.setSensitivity(0.0028);
  voltageSensor_L_PE.setSensitivity(0.0018);
  voltageSensor_N_PE.setSensitivity(0.0018);

  APPCONFIG.wp_max_runtime = DEF_CONF_WP_MAX_RUNTIME;
  APPCONFIG.wp_suspendtime = DEF_CONF_WP_SUSPENDTIME;
  APPCONFIG.wp_lower = DEF_CONF_WP_LOWER;
  APPCONFIG.wp_upper = DEF_CONF_WP_UPPER;
  APPCONFIG.wp_runtime_accumulator_alarm = DEF_CONF_WP_RUNTIME_ACC_ALARM;
  APPCONFIG.min_temp_pumphouse = DEF_CONF_MIN_TEMP_PUMPHOUSE;

  WATERPUMP.pressure_state = PRESSURE_OK;
  WATERPUMP.accumulator_ok = true;

  // set the data rate for the ports
  Serial_Frontend.begin(115200);
  Serial_Frontend.println(F("ADC initializing..."));
  Serial_SensorHub.begin(115200);
  
  //Serial_SensorHub.println(F("ADC initializing..."));

  // initialize all ADC readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    //ADC_temp_1.readings[thisReading] = 0;
    ADC_waterpressure.readings[thisReading] = 0;
  }

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
  
  //EMON_K2.current(ADC_CH_CT_K2, DEF_EMON_ICAL_K2);
  //EMON_K3.current(ADC_CH_CT_K3, DEF_EMON_ICAL_K3);
  
  Wire.begin(); // Initiate the Wire library
  Wire.setClock(50000L); // The NHD LCD has a bug, use 50Khz instead of default 100

  Serial.println(F("ModIO_Init..."));
  ModIO_Init();
  //ModIO_Reset();
  delay(100);

  // turn on 12v bus (system power sensors, relays etc)
  Serial.println(F("switching on 12v bus..."));
  if(!ModIO_SetRelayState(CONF_RELAY_12VBUS, 1) == MODIO_ACK_OK) ModIO_Reset();


  LED_OFF(PIN_LED_RED);
  delay(300);
  LED_OFF(PIN_LED_YELLOW);
  delay(300);
  LED_OFF(PIN_LED_BLUE);
  delay(300);
  LED_OFF(PIN_LED_WHITE);

  tm_blinkAlarm.start();
  tm_blinkWarning.start();
  tm_counterWP_suspend_1sec.start();
  tm_WP_seconds_counter.start();
  tm_100ms_setAlarms.start();
  tm_DataTX.start();

  buzzer_on(PIN_BUZZER, 200);

  wdt_enable(WDTO_4S);

  APPFLAGS.is_busy = 0;
  
  //LED_ON(PIN_LED_RED);

}

ISR (TIMER1_OVF_vect) // interrupt service routine, 0.5 Hz
{
  TCNT1 = timer1_counter;   // preload timer
  if(!ADC_waterpressure.ready || millis() < 5000) return; // just powered up, let things stabilize

}



//******************************************************************
//  Timer2 Interrupt Service is invoked by hardware Timer 2 every 1 ms = 1000 Hz
//  16Mhz / 128 / 125 = 1000 Hz

ISR (TIMER2_COMPA_vect) 
{
  //static uint16_t t2_overflow_cnt;//, t2_overflow_cnt_500ms;
  static bool start_wp_suspension; // set to 1 if we clear an previous active waterpump related alarm (one of ALARMBITS_WATERPUMP_PROTECTION)
  
  if(tm_buzzer.expired()) {
    tm_buzzer.stop();
    digitalWrite(PIN_BUZZER, 0);
  } else {
    if(tm_buzzer.running()) {
      digitalWrite(PIN_BUZZER, !digitalRead(PIN_BUZZER));
    }    
  }

  digitalWrite(LED_BUSY, !APPFLAGS.is_busy);

  if(!ADC_waterpressure.ready || millis() < 5000) return; // just powered up, let things stabilize

  // --------- Set LEDs and buzzer state -------------
  if( (IS_ACTIVE_ALARMS_WP() /*|| WATERPUMP.is_suspended*/)) { // Water pump related is critical/Alarm LED
      if(tm_blinkAlarm.expired()) {
        LED_TOGGLE(LED_ALARM);
        buzzer_on(PIN_BUZZER, 100);
      }
  } else {
    LED_OFF(LED_ALARM);
  }

  if(WATERPUMP.is_suspended) { // if WP is suspended, turn on Warning LED to indicate
    LED_ON(LED_WARNING);
    if(tm_blinkWarning.expired()) { // TODO create dedicated tm_ for this buzzer
      buzzer_on(PIN_BUZZER, 10);
    }

  } else {
    // Non-water pump/other alarms is not critical, blink Warning LED
    if( (getAlarmStatus_SYS(ALARMS_SYS.allBits) || getAlarmStatus_WP(ALARMS_WP.allBits) || getAlarmStatus_EMON(ALARMS_EMON.allBits) ) ) // Active alarm
    {  
      if(tm_blinkWarning.expired()) {
        LED_TOGGLE(LED_WARNING);
        buzzer_on(PIN_BUZZER, 50);
      }    
    } else {
      LED_OFF(LED_WARNING);
    }
  }

  if(tm_100ms_setAlarms.expired()) // every 0.1 sec (OBS! Keep under 1000 (1 sec) for the seconds-counters in alarms to work)
  {
    start_wp_suspension = 0;

    /*** SET/CLEAR ALARMS START ***/
    // All alarms should be set/cleared here, because if they are part of ALARMBITS_WATERPUMP_PROTECTION, this is where the suspension timer is handled!

    // Check free RAM (less than xxx here means erratic behaviour/weird random stuff happens!) (on a 328p at least)
    // OBS! here we have 5-6 bytes less mem free than where we print it in debug output  
    if(freeMemory() < (int)LOWMEM_LIMIT) {
      ALARMS_SYS.low_memory = 1;
    } else {
        if(ALARMS_SYS.low_memory) start_wp_suspension = 1; // was active until now, start suspension period

      ALARMS_SYS.low_memory = 0; 
    }

    if(ALARMS_SYS.low_memory) {
      Serial.print(F("LOW MEM: "));
      Serial.println(freeMemory());
    }

    // Only update alarms (for external data) if not currently reading new ADC values/updating data, we might get zero/invalid results until it's completed
    if(!APPFLAGS.is_updating) {

      /////// EMON Sensor alarms ////////////////
      ALARMS_EMON.sensor_error = 0;

      if(EMONMAINS.Vrms_L_PE < 20.0 || EMONMAINS.Vrms_N_PE < 20.0 || EMONMAINS.Vrms_L_PE > 200.0 || EMONMAINS.Vrms_N_PE > 200.0) {
        ALARMS_EMON.sensor_error = 1; 
      }
      if(EMONMAINS.Vrms_L_N < 50.0 || EMONMAINS.Vrms_L_N > 280.0) {
        ALARMS_EMON.sensor_error = 1; 
      }

      ALARMS_EMON.emon_K2 = 0;
      ALARMS_EMON.emon_K3 = 0;
      if(EMONDATA_K2.error || EMONDATA_K2.Irms > 20.0 || EMONDATA_K2.Irms < 0.0) {
          ALARMS_EMON.emon_K2 = 1;
      }
      if(EMONDATA_K3.error || EMONDATA_K3.Irms > 20.0 || EMONDATA_K3.Irms < 0.0) {
          ALARMS_EMON.emon_K3 = 1;
      }

      // Groundfaults
      ALARMS_EMON.power_groundfault = 0;
      // Jordfeil i et 230V IT nett når spenningen fase jord er  mindre en 90V eller større en 170V (130 +- 40V)	
      if(EMONMAINS.Vrms_L_PE < 90.0 || EMONMAINS.Vrms_N_PE < 90.0 || EMONMAINS.Vrms_L_PE > 170.0 || EMONMAINS.Vrms_N_PE > 170.0) {
        ALARMS_EMON.power_groundfault = 1; 
      }

      // Mains voltage outside range
      ALARMS_EMON.power_voltage = 0;
      // Forsyningskrav i Norge er nominell spenning 230V +/- 10%
      if(EMONMAINS.Vrms_L_N > 253 || EMONMAINS.Vrms_L_N < 207) {
        ALARMS_EMON.power_voltage = 1;
      }

      /////// WATER PUMP Sensor alarms ////////////////
      ALARMS_WP.sensor_error = 0;
/*
      if(ADC_temp_1.average < 350 || ADC_temp_1.average > 700) { // if sensor (LM335) is disconnected ADC gives very high readings
        ALARMS_WP.sensor_error = 1;
      } else {    
         if(ALARMS_WP.sensor_error) start_wp_suspension = 1; // was active until now, start suspension period
      }
*/

      // TODO: check data age
      if(WATERPUMP.temp_pumphouse_val < -20.0 || WATERPUMP.temp_pumphouse_val > 40.0) {
        ALARMS_WP.sensor_error = 1;
      } else {    
         if(ALARMS_WP.sensor_error) start_wp_suspension = 1; // was active until now, start suspension period
      }


      if(ADC_waterpressure.average == 0) {
        ALARMS_WP.sensor_error = 1;
      } else {    
          if(ALARMS_WP.sensor_error) start_wp_suspension = 1; // was active until now, start suspension period
      }

      /////// Water pump and related alarms ///////
      if(WATERPUMP.accumulator_ok) {
        ALARMS_WP.accumulator_low_air = 0;
      } else {
        ALARMS_WP.accumulator_low_air = 1;
      }

      if(WATERPUMP.temp_pumphouse_val < APPCONFIG.min_temp_pumphouse) {
        ALARMS_WP.temperature_pumphouse = 1;
      } else {    
          if(ALARMS_WP.temperature_pumphouse) start_wp_suspension = 1; // was active until now, start suspension period

        ALARMS_WP.temperature_pumphouse = 0;
      }
        

      if(WATERPUMP.is_running && WATERPUMP.state_age > (uint32_t)APPCONFIG.wp_max_runtime) { // have we run to long?
        ALARMS_WP.waterpump_runtime = 1;
      } else {    
        if(ALARMS_WP.waterpump_runtime) start_wp_suspension = 1; // was active until now, start suspension period

        ALARMS_WP.waterpump_runtime = 0;
      }

      /*** SET/CLEAR ALARMS DONE ***/
    } // if(!APPFLAGS.is_updating)
  } // tm_100ms_setAlarms


  // --------- Water pump suspension logic -------------  
  if(IS_ACTIVE_ALARMS_WP()) { 
    WATERPUMP.is_suspended = 0;
  } else {  // No active WP alarms, it's ok to start or continue suspension period

    if(start_wp_suspension) {// we just cleared an WP alarm, start suspension timer
      WATERPUMP.is_suspended = 1;
      WATERPUMP.suspend_timer = 0; 
      WATERPUMP.suspend_count++; // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
    } else { // see if we should increase or disable suspension timer

      if(WATERPUMP.is_suspended && WATERPUMP.suspend_timer < APPCONFIG.wp_suspendtime) { // suspension active and still not reached timeout

      } else if(WATERPUMP.suspend_timer >= APPCONFIG.wp_suspendtime)  {// Suspension period is over
        WATERPUMP.is_suspended = 0;
        WATERPUMP.suspend_timer = 0;
      }
    }
  }

  // --------- Water pump second counters -------------  
  if(tm_WP_seconds_counter.expired()) {
    WATERPUMP.state_age++; 
    WATERPUMP.pressure_state_t++;
    if(WATERPUMP.is_running) {
      WATERPUMP.total_runtime++; 
    }
    if(WATERPUMP.is_suspended) {
      WATERPUMP.suspend_timer++; 
      WATERPUMP.suspend_timer_total++;
    }
  }

  // --------- Water pump state logic -------------
  if(WATERPUMP.water_pressure_bar_val <= APPCONFIG.wp_lower && ADC_waterpressure.average > 0) {
    if(WATERPUMP.pressure_state != PRESSURE_LOW) { // Pressure State has changed!
      WATERPUMP.pressure_state_t = 0;  // Reset state time (inc in timer2)
      WATERPUMP.pressure_state = PRESSURE_LOW;
    }
  } else if(WATERPUMP.water_pressure_bar_val >= APPCONFIG.wp_upper && ADC_waterpressure.average > 0) {
    if(WATERPUMP.pressure_state != PRESSURE_OK) { // Pressure State has changed!
      WATERPUMP.pressure_state_t = 0;  // Reset state time (inc in timer2)
      WATERPUMP.pressure_state = PRESSURE_OK;
    }
  }

  if(WATERPUMP.pressure_state == PRESSURE_LOW) {
    if(WATERPUMP.is_running) {
      
    } else {
      if(WATERPUMP.pressure_state_t > 5) {  // PRESSURE_LOW for more than 5 sec (multiple readings), so data is consistent and it's OK to turn pump on
        if(WATERPUMP.state_age > 5) {  // always wait minimum 5 sec after stop before we start again, no hysterese...
          WATERPUMP.is_running = 1;
          WATERPUMP.state_age=0; //reset state age
          WATERPUMP.start_counter++; // increase the start counter 
        }

      } else {
        //not turning on yet, pressure_state_t < 5 sec
      }
    }
  } else { // PRESSURE_OK
    if(WATERPUMP.is_running) {
      // check if runtime too short, it indicates too low air pressure in accumulator tank
      if(WATERPUMP.state_age < APPCONFIG.wp_runtime_accumulator_alarm) {
        WATERPUMP.accumulator_ok = false;
      } else {
        WATERPUMP.accumulator_ok = true;
      }
      WATERPUMP.is_running = 0;
      WATERPUMP.state_age=0; //reset state age
      
      
    } else {
      
    }
  }

  /*if(WATERPUMP.total_runtime < APPCONFIG.wp_runtime_accumulator_alarm) {
    WATERPUMP.accumulator_ok = true; // no alarm until we actually have a run cycle (after bootup)
  }*/

  // FINAL SAFETY NET, OVERRIDES ALL OTHER LOGIC (this will also include max_runtime alarm)  
  if(IS_ACTIVE_ALARMS_WP() || WATERPUMP.is_suspended) {
    WATERPUMP.is_running = 0;
  }




}



void loop() // run over and over
{
    
  //static uint32_t previousMillis = 0;
  JsonArray data;
  int pulsecount = 0;  
  uint32_t duration = 0;
  uint32_t currentMicros = 0;
  int samples[250];
  char buffer_sensorhub[JSON_SIZE] = {0};
  const char* buffer_sensorhub_ptr = buffer_sensorhub;
  //char json[JSON_SIZE];

  wdt_reset();
//char *ptr = json;
  // forward data received from Sensor-Hub (sensors connected via wifi) to the Frontend
     
    while(Serial_SensorHub.available()) {
      Serial_SensorHub.readBytesUntil('\n', buffer_sensorhub, sizeof(buffer_sensorhub));
    }
    // Extract JSON string and forward to Frontend serial
    if(strlen(buffer_sensorhub) > 0) {
      Serial.print("FWD:");
      //sscanf(buffer_sensorhub, "\\{%s\\}", json);    
      
      //Serial_Frontend.write('{');
      Serial_Frontend.write(buffer_sensorhub);
      Serial_Frontend.write('\n');
      
      //Serial.write('{');
      Serial.write(buffer_sensorhub);
      Serial.write("\n");

      // TODO: parse JSON, process data from REMOTE_PROBES 
      // Temp Pumphouse: if cmd=0x45 and devid=50406 and sid=1 read data("value") into WATERPUMP.temp_pumphouse_val
      
      

      // Deserialize the JSON document     
      tmp_json.clear();
      DeserializationError error = deserializeJson(tmp_json, buffer_sensorhub_ptr); // read-only input (duplication)
      if (error) {
      //if(false) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      } else {

        JsonVariant jsonVal;
        
        //jsonVal = tmp_json.getMember("cmd");
        //uint8_t cmd = jsonVal.as<uint8_t>();

        if(tmp_json.getMember("cmd").as<uint8_t>() == 0x45){ // REMOTE_SENSOR_DATA
          Serial.println("this is SENSOR_DATA");
          if(tmp_json.getMember("devid").as<String>() == "50406") { // Water pump probe
            Serial.println("this is devid 50406");
            if(tmp_json.getMember("sid").as<uint8_t>() == 1) { // room temp sensor (2=humidity, 3=motor temp)
            Serial.println("this is sid 1 pump room temp sensor");
              WATERPUMP.temp_pumphouse_val = tmp_json.getMember("data").getMember("value").as<float>();
              tm_DataStale_50406_1.start(); // restart "old data" timer
            }
          }
        }
      }
    }

  // ------------ RESET/CLEAR REMOTE_SENSOR DATA IF TOO OLD ---------------
  if(tm_DataStale_50406_1.expired()) {
    WATERPUMP.temp_pumphouse_val = -99.0;
  }

  if (tm_DataTX.expired()) {
    
    APPFLAGS.is_busy = 1;
    APPFLAGS.is_updating = 1;

    // Control MOD-IO board relays
    if(ModIO_GetRelayState(CONF_RELAY_WP) != WATERPUMP.is_running) // flag set in ISR, control relay if needed
      if(!ModIO_SetRelayState(CONF_RELAY_WP, WATERPUMP.is_running) == MODIO_ACK_OK) ModIO_Reset();
    
    ModIO_Update();

    //----------- Update EMON (Mains) AC Frequency data -------------

    currentMicros = micros();
    for(int c=0; c<250; c++){
      samples[c] = analogRead(ADC_CH_VOLT_L_N);
      _delay_us(10); // get a higher resolution delay
    }
    duration = micros() - currentMicros;

    for(int i=0;i<251;i++)
    {
      if(samples[i]<512 && samples[i+1]>=512) // if negative AC cycle but next is positive
      {
        while(samples[i+1] >= 512) // while next samlple is positive cycle 
        {
          pulsecount += 1;
          i+=1;
        }
        break;
      }
    }
    // Fixed sample time: it takes 28 milliseconds to take 250 samples. So, for one sample it takes 28/250 = 0.112 msec.
    // float Freq = 1000/(2*pulsecount*0.112);
    // Use actual sample time (more costly but also more accurate)
    float t = duration / 250 / 1000.0; // us / samplecount / 1000.0 us
    EMONMAINS.Freq = 1000/(2*pulsecount* t);

    //----------- Update EMON Voltage data -------------

    EMONMAINS.Vrms_L_N = voltageSensor_L_N.getVoltageAC();
    EMONMAINS.Vrms_L_PE = voltageSensor_L_PE.getVoltageAC();
    EMONMAINS.Vrms_N_PE = voltageSensor_N_PE.getVoltageAC();
    
    //----------- Update EMON Current data -------------

    EMONDATA_K2.Irms = readACCurrentValue(ADC_CH_CT_K2, 20, ADC_CH_CT_K2_VDD_CALIB); // EMON_K2.calcIrms(500);
    EMONDATA_K2.apparentPower = (EMONMAINS.Vrms_L_N * EMONDATA_K2.Irms);
    EMONDATA_K3.Irms = readACCurrentValue(ADC_CH_CT_K3, 20, ADC_CH_CT_K3_VDD_CALIB);
    EMONDATA_K3.apparentPower = (EMONMAINS.Vrms_L_N * EMONDATA_K3.Irms);

    //----------- Update WATER PRESSURE data -------------

    ADC_waterpressure.total -= ADC_waterpressure.readings[ADC_waterpressure.readIndex];
    ADC_waterpressure.readings[ADC_waterpressure.readIndex] = analogRead(ADC_CH_WATERP); //On ATmega based boards (UNO, Nano, Mini, Mega), it takes about 100 microseconds (0.0001 s) to read an analog input, so the maximum reading rate is about 10,000 times a second.
    ADC_waterpressure.total += ADC_waterpressure.readings[ADC_waterpressure.readIndex];
    ADC_waterpressure.readIndex += 1;

    if (ADC_waterpressure.readIndex >= numReadings) {
      ADC_waterpressure.readIndex = 0;
      ADC_waterpressure.ready = 1;
    }

    ADC_waterpressure.average = ADC_waterpressure.total / numReadings;

    // 10-bit ADC, Bar=(ADC*BarMAX)/(2^10). Sensor drop -0.5V/bar (1024/10, minimum sensor output)
    // Integer math instead of float, factor of 100 gives 2 decimals
    //tmp = ( ( (double)(ADC_waterpressure.average * (double)PRESSURE_SENS_MAX) / 1024L));
    //tmp += (double)CORR_FACTOR_PRESSURE_SENSOR;
    WATERPUMP.water_pressure_bar_val = ( ( (double)(ADC_waterpressure.average * (double)PRESSURE_SENS_MAX) / 1024L));
    WATERPUMP.water_pressure_bar_val += (double)CORR_FACTOR_PRESSURE_SENSOR;

    APPFLAGS.is_updating = 0;

    //------------ Create JSON doc ----------------
    LED_ON(PIN_LED_WHITE);

    doc.clear();
    JsonObject root = doc.to<JsonObject>();

    // ---------------- SEND ADCSYSDATA --------------------
    root.clear();
    root["cmd"] = 0x10; // ADCSYSDATA
    root["firmware"] = FIRMWARE_VERSION;
    root["uptime_sec"] = (uint32_t) millis() / 1000;

    data = doc.createNestedArray("alarms");

    if(getAlarmStatus_WP(ALARMS_WP.allBits) || getAlarmStatus_EMON(ALARMS_EMON.allBits) || getAlarmStatus_SYS(ALARMS_SYS.allBits)) { // any alarm bit set?
      if(ALARMS_EMON.emon_K2) data.add(F("K2"));
      if(ALARMS_EMON.emon_K3) data.add(F("K3"));
      if(ALARMS_SYS.low_memory) data.add(F("lowmem"));
      if(ALARMS_WP.waterpump_runtime) data.add(F("wpruntime"));
      if(ALARMS_WP.accumulator_low_air) data.add(F("wp_accumulator"));
      if(ALARMS_WP.temperature_pumphouse) data.add(F("temp_pumphouse"));
      if(ALARMS_WP.sensor_error) data.add(F("sensor_wp"));
      if(ALARMS_EMON.sensor_error) data.add(F("sensor_emon"));
      if(ALARMS_EMON.power_groundfault) data.add(F("groundfault"));
      if(ALARMS_EMON.power_voltage) data.add(F("emon_voltage"));
    }

    dataString = "";
    serializeJson(root, dataString);
    Serial.println(dataString);
    Serial_Frontend.println(dataString);

    // ---------------- SEND ADCEMONDATA --------------------
    root.clear();
    root["cmd"] = 0x11; // ADCEMONDATA

    doc["emon_freq"] = EMONMAINS.Freq;
    doc["emon_vrms_L_N"] = EMONMAINS.Vrms_L_N;
    doc["emon_vrms_L_PE"] = EMONMAINS.Vrms_L_PE;
    doc["emon_vrms_N_PE"] = EMONMAINS.Vrms_N_PE;

    JsonObject circuits = root.createNestedObject("circuits");
    JsonObject circuit = circuits.createNestedObject("K2");
    //json = root.createNestedObject("K2");
    circuit["I"] = EMONDATA_K2.Irms;
    circuit["P_a"] = EMONDATA_K2.apparentPower;
    circuit["PF"] = EMONDATA_K2.PF;
    
    circuit = circuits.createNestedObject("K3");
    //json = root.createNestedObject("K3");
    circuit["I"] = EMONDATA_K3.Irms;
    circuit["P_a"] = EMONDATA_K3.apparentPower;
    circuit["PF"] = EMONDATA_K3.PF;

    dataString = "";
    serializeJson(root, dataString);
    Serial.println(dataString);
    Serial_Frontend.println(dataString);

    // ---------------- SEND ADCWATERPUMPDATA --------------------
    root.clear();
    root["cmd"] = 0x12; // ADCWATERPUMPDATA

    data = doc.createNestedArray("pressure_bar");
    data.add(WATERPUMP.water_pressure_bar_val);
    data = doc.createNestedArray("temp_c"); // TODO: LEGACY, Frontend should read from REMOTE_SENSORS
    data.add(WATERPUMP.temp_pumphouse_val);
    

    JsonObject json = root.createNestedObject("WP");

    json["t_state"] = WATERPUMP.state_age;
    json["is_running"] = WATERPUMP.is_running;
    json["is_suspended"] = WATERPUMP.is_suspended;
    json["cnt_starts"] = WATERPUMP.start_counter;
    json["cnt_susp"] = WATERPUMP.suspend_count;
    json["t_susp"] = WATERPUMP.suspend_timer;
    json["t_susp_tot"] = WATERPUMP.suspend_timer_total;
    json["t_totruntime"] = WATERPUMP.total_runtime;
    json["t_press_st"] = WATERPUMP.pressure_state_t;
    json["press_st"] = WATERPUMP.pressure_state;

    dataString = "";
    serializeJson(root, dataString);
    Serial.println(dataString);
    Serial_Frontend.println(dataString);


    LED_OFF(PIN_LED_WHITE);
    APPFLAGS.is_busy = 0;
  }

}

float readACCurrentValue(uint8_t pin, uint8_t ACTectionRange, double Vdd_calib)
{
  uint8_t readcount = 50;
  float ACCurrtntValue = 0;
  float peakVoltage = 0;  
  float voltageVirtualValue = 0;  //Vrms

  if(ACTectionRange == 0) {
    ACTectionRange = 20; // Default AC Current Sensor tection range (5A,10A,20A)
  }

  for (int i = 0; i < readcount; i++)
  {
    peakVoltage += analogRead(pin);   //read peak voltage
    _delay_us(10);
  }
  peakVoltage = peakVoltage / readcount;   
  voltageVirtualValue = peakVoltage * 0.707;    //change the peak voltage to the Virtual Value of voltage

  /*The circuit is amplified by 2 times, so it is divided by 2.*/
  voltageVirtualValue = (voltageVirtualValue / 1024 * Vdd_calib ) / 2;  

  ACCurrtntValue = voltageVirtualValue * ACTectionRange;

  return ACCurrtntValue;
}


/**
* check if an alarm bit is set or not
*/
bool getAlarmStatus_SYS(uint8_t Alarm) {

  return (ALARMS_SYS.allBits & Alarm);
}

bool getAlarmStatus_WP(uint8_t Alarm) {

  return (ALARMS_WP.allBits & Alarm);
}

bool getAlarmStatus_EMON(uint8_t Alarm) {

  return (ALARMS_EMON.allBits & Alarm);
}

void buzzer_on(uint8_t pin, uint16_t duration) {
    buzzer.pin = pin;
    //buzzer.duration = duration;
    //buzzer._mscnt = 0;
    //buzzer.state = 1;
    if(tm_buzzer.running()) {
      //tm_buzzer.stop();
      return;
    }
    tm_buzzer.limitMillis(duration);
    tm_buzzer.start();
}

void buzzer_off(uint8_t pin) {
    buzzer.pin = pin;
    //buzzer.state = 0;
    tm_buzzer.stop();
}


/**
 * Read chars from Serial until newline and put string in buffer
 * 
 */
int readline(int readch, char *buffer, int len) {
    static int pos = 0;
    int rpos;

    if (readch > 0) {
        switch (readch) {
            case '\r': // Ignore CR
                break;
            case '\n': // Return on new-line
                rpos = pos;
                pos = 0;  // Reset position index ready for next time
                return rpos;
            default:
                if (pos < len-1) {
                    buffer[pos++] = readch;
                    buffer[pos] = 0;
                }
        }
    }
    return 0;
}