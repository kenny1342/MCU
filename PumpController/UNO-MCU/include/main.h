#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#define FIRMWARE_VERSION            "2.02"
#define JSON_SIZE                   210
#define PIN_LED_BUSY                6 // LED OFF if we are running normally (not busy)
#define PIN_LED_ALARM               5 // LED ON of we have active alarms
#define numReadings                 10 // Define the number of samples to keep track of for ADC smoothing
#define PRESSURE_SENS_MAX           10 // sensor maxmimum value (in Bar*1), currently using a 0-10Bar sensor
#define CORR_FACTOR_PRESSURE_SENSOR 0.5  // correction factor (linear) (in Bar)
#define ADC_CH_WATERP               1 // ADC connected to water pressure sensor
#define ADC_CH_TEMP_1               0 // ADC connected to LM335 temp sensor 1
#define ADC_CH_CT_K2                2 // ADC connected to current sensor K2 (living room) Input
#define ADC_CH_CT_K3                3 // ADC connected to current sensor K3 (kitchen) Input
#define ADC_CH_VOLT_L_N             4 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_L_GND           5 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_N_GND           6 // ADC connected to ZMPT101B voltage sensor
//#define ADC_CH_CT_K2_VGND              3 // ADC connected to current sensor K2 Virtual Ground
#define ADC_SAMPLE_COUNT            50 // nr of ADC readings to calculate average from

#define DEF_EMON_ICAL_K2            3.01
#define DEF_EMON_ICAL_K3            3.02
#define DEF_CONF_WP_LOWER           3.77 // water pressure lower threshold (bar*100) before starting pump
#define DEF_CONF_WP_UPPER           4.20 // water pressure upper threshold (bar*100) before stopping pump
#define DEF_CONF_WP_MAX_RUNTIME     600L  // max duration we should let pump run (seconds)
#define DEF_CONF_WP_SUSPENDTIME     20L  // seconds to wait after alarms are cleared before we start pump again
#define DEF_CONF_MIN_TEMP_PUMPHOUSE 10L   // minimum temp pumphouse in degrees C*10 before raising alarm
#define LOWMEM_LIMIT                50  // minimum free memory before raising alarm

// define theoretical vref calibration constant for use in readvcc()
// 1100mV*1024 ADC steps http://openenergymonitor.org/emon/node/1186
// override in your code with value for your specific AVR chip
// determined by procedure described under "Calibrating the internal reference voltage" at
// http://openenergymonitor.org/emon/buildingblocks/calibration
#ifndef READVCC_CALIBRATION_CONST
#define READVCC_CALIBRATION_CONST 1126400L
#endif

#if defined(__arm__)
#define ADC_BITS    12
#else
#define ADC_BITS    10
#endif
#define ADC_COUNTS  (1<<ADC_BITS)

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
  double Vrms_L_GND;
  double Vrms_N_GND;
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
    byte waterpump_runtime:1 ;
    byte power_voltage:1 ; // line voltage too high >= 253V or low <= 209V (230V +/- 10%)
    byte power_groundfault:1 ; // V L/N-GND ~= nettspenning/rot av 3=mellom 100-140V 
    byte temperature_pumphouse:1; // should never be below 0C/freezing
    byte emon_K2:1; // if O/R or CT sensor error
    byte emon_K3:1; // if O/R or CT sensor error
    byte sensor_error:1; // invalid ADC readings etc any sensor
    byte low_memory:1; // set if RAM < x bytes free
  };
} alarm_BitField;

/*
// Define names for the bitfields in alarm_BitField
#define ALARMBIT_WATERPUMP_RUNTIME      0x01 //0b00000001
#define ALARMBIT_POWER_VOLTAGE          0x02 //0b00000010
#define ALARMBIT_GROUNDFAULT            0x04 //0b00000100
#define ALARMBIT_TEMPERATURE_PUMPHOUSE  0x08 //0b00001000
#define ALARMBIT_POWER_GENERIC          0x10 //0b00010000
#define ALARMBIT_SENSOR_ERROR           0x40 //0b01000000
#define ALARMBIT_LOW_MEMORY             0x80 //0b10000000
#define ALARMBIT_ALL                    0xff //0b11111111
// if any of these alarm bits are set, waterpump will not start (@TODO: make as config option)
#define ALARMBITS_WATERPUMP_PROTECTION  (ALARMBIT_SENSOR_ERROR | ALARMBIT_WATERPUMP_RUNTIME | ALARMBIT_TEMPERATURE_PUMPHOUSE | ALARMBIT_LOW_MEMORY) 
*/

#define IS_ACTIVE_ALARMS_WP() (ALARMS.low_memory || ALARMS.sensor_error || ALARMS.temperature_pumphouse || ALARMS.waterpump_runtime)

//void calcEMON(emondata_type &emon, uint8_t Number_of_Samples);
double zmpt101bReadVoltage(uint8_t adc_channel);

long readVcc();
//void calcEMON(String const & s);
bool getAlarmStatus(uint8_t Alarm);

#endif