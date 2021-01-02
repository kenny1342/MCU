#pragma once
#ifndef __MAIN_H
#define __MAIN_H

// Uncomment to get ZMPT101B voltage sensors zero-point values. DISCONNECT FIRST, NO CURRENT SHOULD FLOW THROUGH SENSORS
// note down the values and define them in ZERO_POINT_...
//#define DO_VOLTAGE_CALIBRATION
// 231, n=127, l=130
#define ZERO_POINT_L_N              463
#define ZERO_POINT_L_PE             495
#define ZERO_POINT_N_PE             483

#define FIRMWARE_VERSION            "2.02"
#define JSON_SIZE                   250
#define PIN_LED_BUSY                13 // LED OFF if we are running normally (not busy) (13=led_builtin on mega2560)
#define PIN_LED_ALARM               5 // LED ON of we have active alarms
#define PIN_ModIO_Reset             4
#define PIN_BACKLIGHT               7

#define CONF_I2C_ID_MODIO_BOARD     0x58 // ID of MOD-IO board #1
#define CONF_RELAY_12VBUS           3 // MOD-IO relay controlling 12v bus (sensors, mains relays etc)
#define CONF_RELAY_WP               2 // MOD-IO relay controlling water pump

#define numReadings                 10 // Define the number of samples to keep track of for ADC smoothing
#define PRESSURE_SENS_MAX           10 // sensor maxmimum value (in Bar*1), currently using a 0-10Bar sensor
#define CORR_FACTOR_PRESSURE_SENSOR 0.5  // correction factor (linear) (in Bar)
#define ADC_CH_WATERP               A1 // ADC connected to water pressure sensor
#define ADC_CH_TEMP_1               A0 // ADC connected to LM335 temp sensor 1
#define ADC_CH_CT_K2                A2 // ADC connected to current sensor K2 (living room) Input
#define ADC_CH_CT_K3                A6 // ADC connected to current sensor K3 (kitchen) Input
#define ADC_CH_VOLT_L_N             A3 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_L_PE            A4 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_N_PE            A5 // ADC connected to ZMPT101B voltage sensor

#define DEF_EMON_ICAL_K2            0.40
#define DEF_EMON_ICAL_K3            3.02
#define DEF_CONF_WP_LOWER           3.77 // water pressure lower threshold (bar*100) before starting pump
#define DEF_CONF_WP_UPPER           4.20 // water pressure upper threshold (bar*100) before stopping pump
#define DEF_CONF_WP_MAX_RUNTIME     600L  // max duration we should let pump run (seconds)
#define DEF_CONF_WP_SUSPENDTIME     20L  // seconds to wait after alarms are cleared before we start pump again
#define DEF_CONF_MIN_TEMP_PUMPHOUSE 10L   // minimum temp pumphouse in degrees C*10 before raising alarm
#define LOWMEM_LIMIT                50  // minimum free memory before raising alarm


// Structs

typedef struct
{
  //uint8_t ADC_CH_CT;
  //uint8_t ADC_CH_CT_VGND;
  //double ICAL;
  uint16_t apparentPower;
  //double Vrms;
  double Irms;
  double PF;
  uint8_t error;
} emondata_type;

typedef struct
{
  double Vrms_L_N;
  double Vrms_L_PE;
  double Vrms_N_PE;
  uint8_t error;
} emonvrms_type;


typedef struct
{
  uint16_t wp_max_runtime;
  uint16_t wp_suspendtime;
  double wp_lower;
  double wp_upper;
  uint8_t  min_temp_pumphouse;
} appconfig_type;

typedef union {
  byte allBits;
  struct {
    byte _2sec:1 ;
    byte _1sec:1 ;
    byte _500ms:1 ; 
    byte _200ms:1 ;
    byte _100ms:1;
    byte bitFive:1;
    byte bitSix:1;
    byte bitSeven:1; 
  };
} intervals_type;

typedef struct
{
  uint16_t readings[numReadings];      // the readings from the analog input
  uint8_t readIndex = 0;              // the index of the current reading
  uint16_t total = 0;                  // the running total
  uint16_t average = 0;                // the average
  uint8_t ready = 0;                // set to 1 after boot when numReadings is reached
} adc_avg;
/*
typedef struct
{
  //char temp_pumphouse[5];
  //char water_pressure_bar[4];
  double temp_pumphouse_val;
  double water_pressure_bar_val;

} sensors_type;
*/
typedef struct
{
  uint8_t is_running;
  uint32_t state_age; // seconds since is_running was changed (avoid too frequent start/stops) (uint32_t = 0-4294967295)
  uint16_t start_counter; // increases +1 every time we start the waterpump (volatile, #nr of starts since boot)
  uint16_t suspend_timer; // seconds to wait after alarms are cleared before we start pump again
  bool is_suspended; // 1 if we have a suspension period after alarms before we can run again
  uint8_t suspend_count; // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
  uint32_t  total_runtime;
  double temp_pumphouse_val;
  double water_pressure_bar_val;  
} waterpump_type;

//Flags,  Use each bit in a byte as a flag bit
typedef union {
  byte allBits;
  struct {
    byte is_home:1 ; // 1 if user is home, see isHome() (ping phone etc)
    byte is_busy:1 ; // ISR turns PIN_LED_BUSY ON if we are busy handling web, booting, doing i2c or other time consuming tasks (that might hang)
    byte bitTwo:1 ;
    byte bitThree:1;
    byte bitFour:1;
    byte bitFive:1;
    byte bitSix:1;
    byte is_updating:1; // 1 if reading adc etc, signal to ISR not to do alarm/other logic because values might be zero until done
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

typedef union {
  byte allBits;
  struct {
    byte waterpump_runtime:1 ;
    byte b1:1;
    byte b2:1;
    byte temperature_pumphouse:1; // should never be below 0C/freezing
    byte b4:1;
    byte b5:1;
    byte sensor_error:1; // invalid ADC readings etc any sensor
    byte b7:1;
  };
} alarm_BitField_WP;

typedef union {
  byte allBits;
  struct {
    byte power_voltage:1 ; // line voltage too high >= 253V or low <= 209V (230V +/- 10%)
    byte power_groundfault:1 ; // V L/N-GND !~= nettspenning/rot av 3=230/132,9 (mellom 100-140V )
    byte b2:1;
    byte b3:1;
    byte emon_K2:1; // if O/R or CT sensor error
    byte emon_K3:1; // if O/R or CT sensor error
    byte sensor_error:1; // invalid ADC readings etc any sensor
    byte b7:1;
  };
} alarm_BitField_EMON;

#define IS_ACTIVE_ALARMS_WP() (ALARMS_SYS.low_memory || ALARMS_WP.sensor_error || ALARMS_WP.temperature_pumphouse || ALARMS_WP.waterpump_runtime)

bool getAlarmStatus_WP(uint8_t Alarm);
bool getAlarmStatus_EMON(uint8_t Alarm);
//bool getAlarmStatus(&struct ALARMS uint8_t Alarm);

#endif