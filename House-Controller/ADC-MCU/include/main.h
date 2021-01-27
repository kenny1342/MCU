#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#define ENABLE_BUZZER               1
#define DEVID_PROBE_WATERPUMP       9457 // devid of ESP32 wifi probe with pump motor/temp sensors
#define DEVID_PROBE_BATHROOM        50406 // devid of ESP32 wifi probe

// Uncomment to get ZMPT101B voltage sensors zero-point values. DISCONNECT FIRST, NO CURRENT SHOULD FLOW THROUGH SENSORS
// note down the values and define them in ZERO_POINT_...
//#define DO_VOLTAGE_CALIBRATION
// 231, n=127, l=130
//#define ZERO_POINT_L_N              463
#define ZERO_POINT_L_PE             495
#define ZERO_POINT_N_PE             483

#define FIRMWARE_VERSION            "2.23"
#define JSON_SIZE                   512
#define DATA_TX_INTERVAL            800 // interval (ms) to send JSON data via serial to ESP-32 webserver
#define PIN_MISO                    50  // SPI  Master-In-Slave-Out
#define PIN_MOSI                    51
#define PIN_SCK                     52
#define PIN_SS                      53  // SPI  Slave-Select
#define PIN_LED_RED                 13 // RED, LED OFF if we are running normally (not busy) (13=led_builtin on mega2560)
#define PIN_LED_YELLOW              11 // YELLOW, LED ON of we have active alarms
#define PIN_LED_BLUE                9 // BLUE
#define PIN_LED_WHITE               7 // WHITE, 
#define PIN_BUZZER                  8
#define PIN_ModIO_Reset             6
#define LED_BUSY                    PIN_LED_BLUE
#define LED_ALARM                   PIN_LED_RED
#define LED_WARNING                 PIN_LED_YELLOW

#define CONF_I2C_ID_MODIO_BOARD     0x58 // ID of MOD-IO board #1
#define CONF_RELAY_12VBUS           3 // MOD-IO relay controlling 12v bus (sensors, mains relays etc)
#define CONF_RELAY_WP               2 // MOD-IO relay controlling water pump

#define CORR_FACTOR_PRESSURE_SENSOR 0.5  // correction factor (linear) (in Bar)

#define Serial_Frontend             Serial1
#define Serial_SensorHub            Serial3 // TO BE REMOVED AFTER SPI

#define numReadings                 8 // Define the number of samples to keep track of for ADC smoothing
#define PRESSURE_SENS_MAX           10 // sensor maxmimum value (in Bar*1), currently using a 0-10Bar sensor
#define ADC_CH_WATERP               A1 // ADC connected to water pressure sensor
//#define ADC_CH_TEMP_1               A0 // ADC connected to LM335 temp sensor 1
#define ADC_CH_CT_K1                A8 // ADC connected to current sensor (Main intake) Input
#define ADC_CH_CT_K1_MVPRAMP        11.0  //11 mv/Ampere current clamp
#define ADC_CH_CT_K2                A9 // ADC connected to current sensor (living room) Input
#define ADC_CH_CT_K2_MVPRAMP        40.0  //33 mv/Ampere current clamp
#define ADC_CH_CT_K3                A10 // ADC connected to current sensor (kitchen) Input
#define ADC_CH_CT_K3_MVPRAMP        40.0  //33 mv/Ampere current clamp
#define ADC_CH_CT_K5                A12 // ADC connected to current sensor (pump house) Input
#define ADC_CH_CT_K5_MVPRAMP        40.0  //33 mv/Ampere current clamp
#define ADC_CH_CT_K13               A11 // ADC connected to current sensor (heatpump) Input
#define ADC_CH_CT_K13_MVPRAMP       40.0  //33 mv/Ampere current clamp
#define ADC_CH_VOLT_L_N             A3 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_L_PE            A4 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_N_PE            A5 // ADC connected to ZMPT101B voltage sensor

#define DEF_EMON_ICAL_K2            0.40
#define DEF_EMON_ICAL_K3            3.02
#define DEF_CONF_WP_LOWER           3.75 // water pressure lower threshold (bar*100) before starting pump
#define DEF_CONF_WP_UPPER           4.10 // water pressure upper threshold (bar*100) before stopping pump
#define DEF_CONF_WP_MAX_RUNTIME     1800L  // max duration we should let pump run (seconds) (1800=30min)
#define DEF_CONF_WP_SUSPENDTIME     180L  // seconds to wait after alarms are cleared before we start pump again
#define DEF_CONF_WP_RUNTIME_ACC_ALARM  4U    // if we run shorter than this we raise accumulator/low air pressure alarm
#define DEF_CONF_MIN_TEMP_PUMPHOUSE 0U   // uint8_t! minimum temp pumphouse in degrees C*10 before raising alarm
#define LOWMEM_LIMIT                100  // minimum free memory before raising alarm


// Structs

typedef struct
{
  double Vrms_L_N;
  double Vrms_L_PE;
  double Vrms_N_PE;
  double Freq;
  uint8_t error;
} emonvrms_type;


typedef struct
{
  uint16_t wp_max_runtime;
  uint16_t wp_suspendtime;
  uint8_t wp_runtime_accumulator_alarm;
  double wp_lower;
  double wp_upper;
  uint8_t  min_temp_pumphouse;  
} appconfig_type;

typedef struct {
  uint8_t pin = 8;
} Buzzer;

typedef struct
{
  uint16_t readings[numReadings];      // the readings from the analog input
  uint8_t readIndex = 0;              // the index of the current reading
  uint16_t total = 0;                  // the running total
  uint16_t average = 0;                // the average
  uint8_t ready = 0;                // set to 1 after boot when numReadings is reached
} adc_avg;

enum { PRESSURE_LOW=0, PRESSURE_OK=1, PRESSURE_HIGH=2 };

enum { STOPPED=0, RUNNING=1, SUSPENDED=2, ALARM=3 };

typedef struct
{
  uint8_t status; // enum
  uint8_t suspend_reason;
  uint32_t state_age; // seconds since is_running was changed (avoid too frequent start/stops) (uint32_t = 0-4294967295)
  uint16_t start_counter; // increases +1 every time we start the waterpump (volatile, #nr of starts since boot)
  uint16_t suspend_timer; // seconds to wait after alarms are cleared before we start pump again
  //bool is_suspended; // 1 if we have a suspension period after alarms before we can run again
  uint8_t suspend_count; // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
  uint32_t suspend_timer_total;
  uint32_t  total_runtime;
  double temp_pumphouse_val = 0;
  double hum_pumphouse_val = -1.0; // to indicate no data received yet
  double temp_motor_val = 0;
  double temp_inlet_val = 0;
  double water_pressure_bar_val;  
  uint8_t pressure_state;
  uint16_t pressure_state_t;
  //bool accumulator_ok;
} waterpump_type;

//Flags,  Use each bit in a byte as a flag bit
typedef union {
  byte allBits;
  struct {
    byte is_home:1 ; // 1 if user is home, see isHome() (ping phone etc)
    byte isSendingData:1 ; // ISR turns PIN_LED_BUSY ON if we are busy handling web, booting, doing i2c or other time consuming tasks (that might hang)
    byte bitTwo:1 ;
    byte bitThree:1;
    byte bitFour:1;
    byte bitFive:1;
    byte bitSix:1;
    byte isUpdatingData:1; // 1 if reading adc, transfering data etc, we signal ISR's not to do touch anything
  };
} flags_BitField;

// struct with 8 possible alarm bits
typedef union {
  byte allBits;
  struct {
    byte b0:1;
    byte b1:1;
    byte b2:1;
    byte b3:1;
    byte b4:1;
    byte b5:1;
    byte b6:1;
    byte low_memory:1; // set if RAM < x bytes free
  };
} alarm_BitField_SYS;
const char alarm_Text_SYS[9][8] = { "", "", "", "", "", "", "", "lowmem" }; // map in Frontend/config.json

typedef union {
  byte allBits;
  struct {
    byte waterpump_runtime:1 ;
    byte accumulator_low_air:1;
    byte b2:1;
    byte temperature_pumphouse:1; // if too cold
    byte b4:1;
    byte b5:1;
    byte sensor_error:1; // pressure
    byte sensor_error_room:1;
  };
} alarm_BitField_WP;
const char alarm_Text_WP[9][13] = { "wpruntime", "wpaccair", "", "wptemproom", "", "", "wppressens", "wproomsens" }; // map in Frontend/config.json

typedef union {
  byte allBits;
  struct {
    byte power_voltage:1 ; // line voltage too high >= 253V or low <= 209V (230V +/- 10%)
    byte power_groundfault:1 ; // V L/N-GND !~= nettspenning/rot av 3=230/132,9 (mellom 100-140V )
    byte b2:1;
    byte b3:1;
    byte emon_K1:1; // if O/R or CT sensor error
    byte b5:1;
    byte sensor_error:1; // invalid ADC readings etc any sensor
    byte b7:1;
  };
} alarm_BitField_EMON;
const char alarm_Text_EMON[9][15] = { "emon_mains_o_r", "emon_gndfault", "", "", "k1_o_r", "", "emon_sensor", "" }; // map in Frontend/config.json

#define IS_ACTIVE_ALARMS_WP() (ALARMS_SYS.low_memory || ALARMS_WP.sensor_error || ALARMS_WP.temperature_pumphouse || ALARMS_WP.waterpump_runtime)
#define LED_ON(pin) digitalWrite(pin, 0)
#define LED_OFF(pin) digitalWrite(pin, 1)
#define LED_TOGGLE(pin) digitalWrite(pin, !digitalRead(pin))

//float readACCurrentValue(uint8_t pin, uint8_t ACTectionRange, double Vdd_calib);
bool getAlarmStatus_SYS(uint8_t Alarm);
bool getAlarmStatus_WP(uint8_t Alarm);
bool getAlarmStatus_EMON(uint8_t Alarm);
void buzzer_on(uint8_t pin, uint16_t duration);
void buzzer_off(uint8_t pin);
//bool getAlarmStatus(&struct ALARMS uint8_t Alarm);
int readline(int readch, char *buffer, int len);
bool checkIfColdStart ();
#endif