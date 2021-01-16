/**
 * ADC-MCU - collect data and process alarms and relays/outputs. Also send data to Frontend (ESP32 webserver+TFT display) via serial
 * 
 * Connect the SW TX/RX pins to the ESP32 HW serial pins
 * 
 * TODO: replace serial link to Frontend with SPI
 * 
 * Kenny Dec 19, 2020
 */
#include <Arduino.h>
#include <main.h>
#include <SoftwareSerial.h>
#include <Wire.h>
//#include <SoftWire.h>
#include <SPI.h>
#include <AM2320.h>
#include <avr/wdt.h>
#include <ArduinoJson.h>
#include <MemoryFree.h>

#include <ZMPT101B.h>
#include <olimex-mod-io.h>
#include <Timemark.h>
#include <KRA-Emon.h>

// store long global string in flash (put the pointers to PROGMEM)
const char string_0[] PROGMEM = "ADC-MCU v" FIRMWARE_VERSION " build " __DATE__ " " __TIME__ " from file " __FILE__ " using GCC v" __VERSION__;
const char* const FIRMWARE_VERSION_LONG[] PROGMEM = { string_0 };

uint16_t timer1_counter; // preload of timer1
String dataString = "no data";

char lastAlarm[20] = "-";

// For SPI data processing/ISR
volatile uint16_t indx;
volatile bool SPI_dataready = false;
volatile char buffer_sensorhub[JSON_SIZE];

//StaticJsonDocument<JSON_SIZE> doc;
//StaticJsonDocument<JSON_SIZE> tmp_json; // Parsing input serial data from REMOTE_SENSORS

//AM2320 am2320_pumproom(PIN_AM2320_SDA_PUMPROOM,PIN_AM2320_SCL_PUMPROOM); // AM2320 sensor attached SDA to digital PIN 5 and SCL to digital PIN 6

ZMPT101B voltageSensor_L_PE(ADC_CH_VOLT_L_PE);
ZMPT101B voltageSensor_N_PE(ADC_CH_VOLT_N_PE);

volatile KRAEMON EMON_K1 (ADC_CH_CT_K1, ADC_CH_VOLT_L_N, ADC_CH_CT_K1_MVPRAMP, "1");
volatile KRAEMON EMON_K2 (ADC_CH_CT_K2, ADC_CH_VOLT_L_N, ADC_CH_CT_K2_MVPRAMP, "2"); 
volatile KRAEMON EMON_K3 (ADC_CH_CT_K3, ADC_CH_VOLT_L_N, ADC_CH_CT_K3_MVPRAMP, "3"); 
volatile KRAEMON EMON_K13 (ADC_CH_CT_K13, ADC_CH_VOLT_L_N, ADC_CH_CT_K13_MVPRAMP, "13"); 
const uint8_t NUM_EMONS = 4;
volatile KRAEMON * KRAEMONS[NUM_EMONS] = { &EMON_K1, &EMON_K2, &EMON_K3, &EMON_K13 };


// Structs
adc_avg ADC_waterpressure;
volatile Buzzer buzzer;
emonvrms_type EMONMAINS; // Vrms values (phases/gnd)
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
Timemark tm_100ms_setAlarms(10);
Timemark tm_UpdateEMON(500);
Timemark tm_DataTX(DATA_TX_INTERVAL);
Timemark tm_buzzer(0);
Timemark tm_DataStale_50406_1(120000); // We expect Pump House temperature updated within 2 min, if not we clear/alarm it
Timemark tm_SerialDebug(5000);

const uint8_t NUM_TIMERS = 10;

Timemark *Timers[NUM_TIMERS] = { 
  &tm_blinkAlarm, 
  &tm_blinkWarning, 
  &tm_WP_seconds_counter, 
  &tm_counterWP_suspend_1sec, 
  &tm_100ms_setAlarms, 
  &tm_UpdateEMON,
  &tm_DataTX, 
  &tm_buzzer, 
  &tm_DataStale_50406_1, 
  &tm_SerialDebug 
  };
enum Timers {   TM_blinkAlarm, 
  TM_blinkWarning, 
  TM_WP_seconds_counter, 
  TM_counterWP_suspend_1sec, 
  TM_100ms_setAlarms, 
  TM_UpdateEMON,
  TM_DataTX, 
  TM_buzzer, 
  TM_DataStale_50406_1, 
  TM_SerialDebug 
};

void setup()
{
  wdt_enable(WDTO_8S);

  APPFLAGS.isSendingData = 1;

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_ModIO_Reset, INPUT); // need to float
  pinMode(PIN_MISO,OUTPUT); // // have to send on master in, *slave out*
  //pinMode(PIN_SCK,INPUT);
  //pinMode(PIN_MOSI,INPUT);
  //pinMode(PIN_SS,INPUT);

   
  SPCR |= _BV(SPE); // turn on SPI in slave mode
  SPI.attachInterrupt(); // turn on interrupt


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

  voltageSensor_L_PE.setZeroPoint(ZERO_POINT_L_PE);
  voltageSensor_N_PE.setZeroPoint(ZERO_POINT_N_PE);
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

  Serial_Frontend.begin(57600);
  Serial_Frontend.println(F("ADC initializing..."));

  noInterrupts();          // disable global interrupts
/*
  // Setup Timer1 for 500 ms (0.5 Hz) overlow ISR
  TCCR1A = 0;     // set entire TCCR1A register to 0 (no PWM)
  TCCR1B = 0;     // same for TCCR1B
  // Set timer1_counter to the correct value for our interrupt interval
  timer1_counter = 60000; //34286;   // preload timer 65536-16MHz/256/2Hz, 65536-16MHz/1024/0.5Hz
  TCNT1 = timer1_counter;   // preload timer
  
  // BUG: 256 prescaler crashes on Uno R3 DIL board
  //TCCR1B |= (1 << CS12);    // 256 prescaler 
  // Set CS10 and CS12 bits for 1024 prescaler:  
  TCCR1B |= (1 << CS12);
  TCCR1B |= (1 << CS10);
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
*/

// easy calc: http://www.8bit-era.cz/arduino-timer-interrupts-calculator.html

// TIMER 1 for interrupt frequency 10 Hz:
cli(); // stop interrupts
TCCR1A = 0; // set entire TCCR1A register to 0
TCCR1B = 0; // same for TCCR1B
TCNT1  = 0; // initialize counter value to 0
// set compare match register for 10 Hz increments
OCR1A = 24999; // = 16000000 / (64 * 10) - 1 (must be <65536)
// turn on CTC mode
TCCR1B |= (1 << WGM12);
// Set CS12, CS11 and CS10 bits for 64 prescaler
TCCR1B |= (0 << CS12) | (1 << CS11) | (1 << CS10);
// enable timer compare interrupt
TIMSK1 |= (1 << OCIE1A);
sei(); // allow interrupts

// TIMER 2 for interrupt frequency 1000 Hz:
cli(); // stop interrupts
TCCR2A = 0; // set entire TCCR2A register to 0
TCCR2B = 0; // same for TCCR2B
TCNT2  = 0; // initialize counter value to 0
// set compare match register for 1000 Hz increments
OCR2A = 249; // = 16000000 / (64 * 1000) - 1 (must be <256)
// turn on CTC mode
TCCR2B |= (1 << WGM21);
// Set CS22, CS21 and CS20 bits for 64 prescaler
TCCR2B |= (1 << CS22) | (0 << CS21) | (0 << CS20);
// enable timer compare interrupt
TIMSK2 |= (1 << OCIE2A);
sei(); // allow interrupts

/*
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
  */

  Serial.println(F("ISR's enabled"));
  delay(500);

  Wire.begin(); // Initiate the Wire library

  Serial.println(F("ModIO_Init..."));
  ModIO_Reset(); // we need to because sometimes it hangs on startup (stuck i2c)
  ModIO_Init();
  
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

  // TODO: rewrite all to pointer
  tm_blinkAlarm.start();
  tm_blinkWarning.start();
  tm_counterWP_suspend_1sec.start();
  tm_WP_seconds_counter.start();
  tm_100ms_setAlarms.start();
  tm_UpdateEMON.start();
  tm_DataTX.start();
  Timers[TM_SerialDebug]->start();

  for(int t=0; t<NUM_TIMERS; t++){
   // Timers[t]->start();
  }

  buzzer_on(PIN_BUZZER, 200);

  APPFLAGS.isSendingData = 0;
  
  Serial.println(F("Init done!"));
}

ISR (SPI_STC_vect)
{
  byte c = SPDR;
  
if(c == 0x10) { // our address
  SPDR = c;
  return;
}

  if(c == '<' || indx >= JSON_SIZE-1) { // Start marker
    indx =0;
    memset ( (void*)buffer_sensorhub, 0, JSON_SIZE );
    buffer_sensorhub[0] = 0;

    //Serial.print("SPISTART\n");
    return;
  } 

  if(c == 0x0F) { // End marker
    indx = 0;
    //Serial.write(c);
    SPI_dataready = true;
    //Serial.print("SPIEND\n");
    return;
  }

  if(c != 0x00) {
    buffer_sensorhub[indx] = c; // read byte from SPI Data Register
    indx++;
    buffer_sensorhub[indx] = '\0';
  }

}

/**
 * ISR to handle incoming data from SPI (remote probes via WiFi Hub)
 */
//ISR (TIMER1_OVF_vect) // interrupt service routine, 0.5 Hz
ISR(TIMER1_COMPA_vect)
{
  TCNT1 = timer1_counter;   // preload timer
  if(millis() < 5000) return; // just powered up, let things stabilize

  bool doSerialDebug = true;
  char SPIData[JSON_SIZE]; // local copy
  bool NewSPIData;

  //--------- forward JSON data string received from Sensor-Hub via SPI (data from sensors connected via wifi) to the Frontend -------
  noInterrupts();
  strcpy(SPIData, (char *)buffer_sensorhub);
  NewSPIData=SPI_dataready;
  SPI_dataready = false;
  interrupts();
  //Serial.print(millis()/1000);
  if(NewSPIData) {
    if(doSerialDebug) { Serial.print (SPIData); Serial.print("\n"); } //print the array on serial monitor    

    // Send to Frontend unchanged via serial
    // TODO: change to SPI
    Serial_Frontend.write(SPIData);
    Serial_Frontend.write('\n');
  
    // Parse JSON document and find cmd, devid and sid, process data if it's of interest for us (in alarms or other logic)    
    DynamicJsonDocument tmp_json(JSON_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)
    //tmp_json.clear();
    const char* p = SPIData;
    DeserializationError error = deserializeJson(tmp_json, p); // read-only input (duplication)
    tmp_json.shrinkToFit();

    if (error) {
      Serial.write(SPIData);
      Serial.print(F("\n^JSON_ERR:"));
      Serial.println(error.f_str());
      
    } else {

      if(tmp_json.getMember("cmd").as<uint8_t>() == 0x45){ // REMOTE_SENSOR_DATA

        uint32_t _devid = tmp_json.getMember("devid").as<uint32_t>();
        uint8_t sid = tmp_json.getMember("sid").as<uint8_t>();
        switch(_devid) {
          case 50406: // EP32 Probe in pump house
          {
            // sid 1=temp room, 2=humidity room, 3=temp motor
            if(sid == 0x01) { 
              WATERPUMP.temp_pumphouse_val = tmp_json.getMember("data").getMember("value").as<float>();
              tm_DataStale_50406_1.start(); // restart "old data" timer
            }
            if(sid == 0x02) { 
              WATERPUMP.hum_pumphouse_val = tmp_json.getMember("data").getMember("value").as<float>();
              tm_DataStale_50406_1.start(); // restart "old data" timer
            }
            if(sid == 0x03) { 
              WATERPUMP.temp_motor_val = tmp_json.getMember("data").getMember("value").as<float>();              
            }
          }
          break;
          default: Serial.print(F("got data from unknown devid: ")); Serial.println(_devid);
        }
      } // 0x45
    } // no json error
  } // NewSPIData

}


//******************************************************************
//  Timer2 Interrupt Service is invoked by hardware Timer 2 every 1 ms = 1000 Hz
//  16Mhz / 128 / 125 = 1000 Hz

ISR (TIMER2_COMPA_vect) 
{
  //static uint16_t t2_overflow_cnt;//, t2_overflow_cnt_500ms;
  static bool start_wp_suspension; // set to 1 if we clear an previous active waterpump related alarm (one of ALARMBITS_WATERPUMP_PROTECTION)

  if(ENABLE_BUZZER) {
    if(tm_buzzer.expired()) {
      tm_buzzer.stop();
      digitalWrite(PIN_BUZZER, 0);
    } else {
      if(tm_buzzer.running()) {
        digitalWrite(PIN_BUZZER, !digitalRead(PIN_BUZZER));
      }    
    }
  }

  digitalWrite(LED_BUSY, !APPFLAGS.isUpdatingData);
  digitalWrite(PIN_LED_WHITE, !APPFLAGS.isSendingData);

  if(!ADC_waterpressure.ready || millis() < 20000) return; // just powered up, let things stabilize

  // --------- Set LEDs and buzzer state -------------
  if( (IS_ACTIVE_ALARMS_WP() /*|| WATERPUMP.is_suspended*/)) { // Water pump related is critical/Alarm LED
      if(tm_blinkAlarm.expired()) {
        LED_TOGGLE(LED_ALARM);
        buzzer_on(PIN_BUZZER, 100);
      }
  } else {
    LED_OFF(LED_ALARM);
  }

  if(WATERPUMP.status == SUSPENDED) { // if WP is suspended, turn on Warning LED to indicate
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
        if(ALARMS_SYS.low_memory) {
          start_wp_suspension = 1; // was active until now, start suspension period
          WATERPUMP.suspend_reason = 10;
        }

      ALARMS_SYS.low_memory = 0; 
    }

    if(ALARMS_SYS.low_memory) {
      Serial.print(F("LOW MEM: "));
      Serial.println(freeMemory());
    }

    // Only update alarms (for external data) if not currently reading new ADC values/updating data, we might get zero/invalid results until it's completed
    if(!APPFLAGS.isUpdatingData) {

      /////// EMON Sensor alarms ////////////////
      ALARMS_EMON.sensor_error = 0;

      if(EMONMAINS.Vrms_L_PE < 20.0 || EMONMAINS.Vrms_N_PE < 20.0 || EMONMAINS.Vrms_L_PE > 200.0 || EMONMAINS.Vrms_N_PE > 200.0) {
        ALARMS_EMON.sensor_error = 1; 
      }
      if(EMONMAINS.Vrms_L_N < 50.0 || EMONMAINS.Vrms_L_N > 280.0) {
        ALARMS_EMON.sensor_error = 1; 
      }

      ALARMS_EMON.emon_K1 = 0;
      ALARMS_EMON.emon_K2 = 0;
      if(EMON_K1.error || EMON_K1.FinalRMSCurrent > 60.0 || EMON_K1.FinalRMSCurrent < 0.0) {
          ALARMS_EMON.emon_K1 = 1;
      }
      if(EMON_K2.error || EMON_K2.FinalRMSCurrent > 20.0 || EMON_K2.FinalRMSCurrent < 0.0) {
          ALARMS_EMON.emon_K2 = 1;
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

      // TODO: check data age
      if(WATERPUMP.temp_pumphouse_val < -20.0 || WATERPUMP.temp_pumphouse_val > 40.0) {
        ALARMS_WP.sensor_error = 1;
      } else {    
         if(ALARMS_WP.sensor_error) {
           start_wp_suspension = 1; // was active until now, start suspension period
           WATERPUMP.suspend_reason = 11;
         }
      }


      if(ADC_waterpressure.average == 0) {
        ALARMS_WP.sensor_error = 1;
      } else {    
          if(ALARMS_WP.sensor_error) {
            start_wp_suspension = 1; // was active until now, start suspension period
            WATERPUMP.suspend_reason = 12;
          }
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
          if(ALARMS_WP.temperature_pumphouse) {
            start_wp_suspension = 1; // was active until now, start suspension period
            WATERPUMP.suspend_reason = 13;
          }

        ALARMS_WP.temperature_pumphouse = 0;
      }
        

      if(WATERPUMP.status == RUNNING && WATERPUMP.state_age > (uint32_t)APPCONFIG.wp_max_runtime) { // have we run to long?
        ALARMS_WP.waterpump_runtime = 1;
      } else {    
        if(ALARMS_WP.waterpump_runtime) {
          start_wp_suspension = 1; // was active until now, start suspension period
          WATERPUMP.suspend_reason = 14;
        }

        ALARMS_WP.waterpump_runtime = 0;
      }

      /*** SET/CLEAR ALARMS DONE ***/
    } // if(!APPFLAGS.isUpdatingData)
  } // tm_100ms_setAlarms


  // --------- Water pump suspension logic -------------  
  if(IS_ACTIVE_ALARMS_WP()) { 
    if(WATERPUMP.status != DISABLED) {
      WATERPUMP.status = DISABLED;
      WATERPUMP.state_age=0; //reset state age
    }
  } else {  // No active WP alarms, it's ok to start or continue suspension period

    if(WATERPUMP.status != SUSPENDED && start_wp_suspension) {// not suspended and we just cleared an WP alarm, start suspension timer
      WATERPUMP.status = SUSPENDED;
      WATERPUMP.state_age=0; //reset state age
      WATERPUMP.suspend_count++; // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
    } else { // see if we should increase or disable suspension timer

      if(WATERPUMP.status == SUSPENDED) {
        if(WATERPUMP.suspend_timer >= APPCONFIG.wp_suspendtime)  {// Suspension period is over
          WATERPUMP.status = STOPPED;
          WATERPUMP.state_age=0; //reset state age
        }
      }
    }
  }

  // --------- Water pump second counters -------------  
  if(tm_WP_seconds_counter.expired()) {
    WATERPUMP.state_age++; 
    WATERPUMP.pressure_state_t++;
    if(WATERPUMP.status == RUNNING) {
      WATERPUMP.total_runtime++; 
    }

    if(WATERPUMP.status == SUSPENDED) {
      WATERPUMP.suspend_timer++; 
      WATERPUMP.suspend_timer_total++;
    } else {
      WATERPUMP.suspend_timer = 0;
    }
  }

  // --------- Water pump state logic -------------
  if(WATERPUMP.water_pressure_bar_val <= APPCONFIG.wp_lower && ADC_waterpressure.average > 0) {
    if(WATERPUMP.pressure_state != PRESSURE_LOW) { // Pressure State has changed!
      WATERPUMP.pressure_state_t = 0;  // Reset state time (inc in timer2)
      WATERPUMP.pressure_state = PRESSURE_LOW;
    }
  } else if(WATERPUMP.water_pressure_bar_val >= APPCONFIG.wp_lower && ADC_waterpressure.average > 0) {
    if(WATERPUMP.pressure_state != PRESSURE_OK) { // Pressure State has changed!
      WATERPUMP.pressure_state_t = 0;  // Reset state time (inc in timer2)
      WATERPUMP.pressure_state = PRESSURE_OK;
    }
  }

  if(WATERPUMP.status == STOPPED) {
    if(WATERPUMP.pressure_state == PRESSURE_LOW) {
      if(WATERPUMP.pressure_state_t > 5) {  // PRESSURE_LOW for more than 5 sec (multiple readings), so data is consistent and it's OK to turn pump on
        if(WATERPUMP.state_age > 10) {  // always wait minimum n sec after stop before we start again, no hysterese...
          WATERPUMP.status = RUNNING;
          WATERPUMP.state_age=0; //reset state age
          WATERPUMP.start_counter++; // increase the start counter 
        }

      } else {
        //not turning on yet, pressure_state_t < n sec
      }
    } else { // PRESSURE_OK
      if(WATERPUMP.status == RUNNING && WATERPUMP.water_pressure_bar_val >= APPCONFIG.wp_upper) {
        // check if runtime too short, it indicates too low air pressure in accumulator tank
        if(WATERPUMP.state_age < APPCONFIG.wp_runtime_accumulator_alarm) {
          WATERPUMP.accumulator_ok = false;
        } else {
          WATERPUMP.accumulator_ok = true;
        }
        
        WATERPUMP.status = STOPPED;
        WATERPUMP.state_age=0; //reset state age
      } else {
        
      }
    }
  }


  // FINAL SAFETY NET, OVERRIDES ALL OTHER LOGIC (this will also include max_runtime alarm)  
  if(IS_ACTIVE_ALARMS_WP()) {
    //WATERPUMP.status = STOPPED;
  }

}


void loop() // run over and over
{
  JsonArray data;
  int pulsecount = 0;  
  uint32_t duration = 0;
  uint32_t currentMicros = 0;
  int samples[250];
  bool doSerialDebug = Timers[TM_SerialDebug]->expired();

  wdt_reset();

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

    //----------- Update EMON data -------------
    if(Timers[TM_UpdateEMON]->expired() && !APPFLAGS.isUpdatingData) {
      for(int x=0; x<250; x++) {
        for(int x=0; x<NUM_EMONS; x++) {
          KRAEMONS[x]->Calc();
        }
      }
    }

    EMONMAINS.Vrms_L_N = EMON_K1.RMSVoltageMean; // voltageSensor_L_N.getVoltageAC();
    
    //----------- Update Phase/Ground Voltages -------------
    EMONMAINS.Vrms_L_PE = voltageSensor_L_PE.getVoltageAC();
    EMONMAINS.Vrms_N_PE = voltageSensor_N_PE.getVoltageAC();

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

    // Control MOD-IO board relays
    if(ModIO_GetRelayState(CONF_RELAY_WP) != (WATERPUMP.status == RUNNING)) // flag set in ISR, control relay if needed
      if(!ModIO_SetRelayState(CONF_RELAY_WP, (WATERPUMP.status == RUNNING)) == MODIO_ACK_OK) ModIO_Reset();
    
    ModIO_Update();


    APPFLAGS.isUpdatingData = 0;

  // ------------ RESET/CLEAR REMOTE_SENSOR DATA IF TOO OLD ---------------
  if(Timers[TM_DataStale_50406_1]->expired()) {
    WATERPUMP.temp_pumphouse_val = -99.9;
  }

  // ------------- SEND DATA TO FRONTEND ----------------------------------
  if (Timers[TM_DataTX]->expired()) {
    
    APPFLAGS.isSendingData = 1;
    //APPFLAGS.isUpdatingData = 1;

    //------------ Create JSON docs and send to Frontend ----------------
    //LED_ON(PIN_LED_WHITE);

    DynamicJsonDocument doc(JSON_SIZE); // Dynamic; store in the heap (recommended for documents larger than 1KB)
    //doc.clear();
    JsonObject root = doc.to<JsonObject>();
    
    // ---------------- SEND ADCSYSDATA --------------------
    root.clear();
    root["cmd"] = 0x10; // ADCSYSDATA
    root["devid"] = 0x10;
    root["firmware"] = FIRMWARE_VERSION;
    root["uptimesecs"] = (uint32_t) millis() / 1000;
    root["freemem"] = freeMemory();

    data = doc.createNestedArray("alarms");

    //if(getAlarmStatus_WP(ALARMS_WP.allBits) || getAlarmStatus_EMON(ALARMS_EMON.allBits) || getAlarmStatus_SYS(ALARMS_SYS.allBits)) { // any alarm bit set?
      
      for(uint8_t x=0; x<8; x++) {
        if( (ALARMS_SYS.allBits >> x) & 1) { // is bit set?
          const char * text = alarm_Text_SYS[x];
          data.add(text);
          strncpy(lastAlarm, text, sizeof(lastAlarm));
        }
      }      
      
      for(uint8_t x=0; x<8; x++) {
        
        const char * text = alarm_Text_WP[x];
        if( (ALARMS_WP.allBits >> x) & 1) { // is bit set?
          data.add(text);
          strncpy(lastAlarm, text, sizeof(lastAlarm));
        }
      }

      for(uint8_t x=0; x<8; x++) {
        if( (ALARMS_EMON.allBits >> x) & 1) { // is bit set?
          const char * text = alarm_Text_EMON[x];
          data.add(text);
          strncpy(lastAlarm, text, sizeof(lastAlarm));
        }
      }

    //}
    root["lastAlarm"] = lastAlarm;

    dataString = "";
    serializeJson(root, dataString);
    if(doSerialDebug) Serial.println(dataString);
    Serial_Frontend.println(dataString);
    delay(500);
    wdt_reset();

    // ---------------- SEND ADCEMONDATA --------------------
    root.clear();
    root["cmd"] = 0x11; // ADCEMONDATA
    root["devid"] = 0x10;

    doc["emon_freq"] = EMONMAINS.Freq;
    doc["emon_vrms_L_N"] = EMONMAINS.Vrms_L_N;
    doc["emon_vrms_L_PE"] = EMONMAINS.Vrms_L_PE;
    doc["emon_vrms_N_PE"] = EMONMAINS.Vrms_N_PE;

    JsonObject circuits = root.createNestedObject("circuits");

    for(int x=0; x<NUM_EMONS; x++) {
      JsonObject circuit = circuits.createNestedObject(KRAEMONS[x]->id);
      circuit["I"] = KRAEMONS[x]->FinalRMSCurrent;
      circuit["P_a"] = KRAEMONS[x]->apparentPower;
      circuit["PF"] = KRAEMONS[x]->powerFactor;
    }

    dataString = "";
    serializeJson(root, dataString);
    if(doSerialDebug) Serial.println(dataString);
    Serial_Frontend.println(dataString);
    delay(500);
    wdt_reset();

    // ---------------- SEND ADCWATERPUMPDATA --------------------
    root.clear();
    root["cmd"] = 0x12; // ADCWATERPUMPDATA
    root["devid"] = 0x10;
    root["temp_c"] = WATERPUMP.temp_pumphouse_val; // TODO: rename to temp_room_c
    root["temp_motor_c"] = WATERPUMP.temp_motor_val;
    root["hum_room_pct"] = WATERPUMP.hum_pumphouse_val;
    root["pressure_bar"] = WATERPUMP.water_pressure_bar_val;

    JsonObject json = root.createNestedObject("WP");

    switch(WATERPUMP.status) {
      case RUNNING: json["status"] = "RUN"; break;
      case STOPPED: json["status"] = "STOP"; break;
      case SUSPENDED: json["status"] = "SUSPENDED"; break;
      case DISABLED: json["status"] = "DISABLED"; break;
      default: json["status"] = WATERPUMP.status;
    }
    
    json["t_state"] = WATERPUMP.state_age;
    json["susp_r"] = WATERPUMP.suspend_reason;
    json["cnt_starts"] = WATERPUMP.start_counter;
    json["cnt_susp"] = WATERPUMP.suspend_count;
    json["t_susp"] = WATERPUMP.suspend_timer;
    json["t_susp_tot"] = WATERPUMP.suspend_timer_total;
    json["t_totruntime"] = WATERPUMP.total_runtime;
    json["t_press_st"] = WATERPUMP.pressure_state_t;
    json["press_st"] = WATERPUMP.pressure_state;

    dataString = "";
    serializeJson(root, dataString);
    if(doSerialDebug) Serial.println(dataString);
    Serial_Frontend.println(dataString);

    //LED_OFF(PIN_LED_WHITE);
    APPFLAGS.isSendingData = 0;
  }

}
/*
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

  //The circuit is amplified by 2 times, so it is divided by 2.
  voltageVirtualValue = (voltageVirtualValue / 1024 * Vdd_calib ) / 2;  

  ACCurrtntValue = voltageVirtualValue * ACTectionRange;

  return ACCurrtntValue;
}
*/

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