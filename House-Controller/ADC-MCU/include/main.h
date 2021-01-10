#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#define ENABLE_BUZZER               0
// Uncomment to get ZMPT101B voltage sensors zero-point values. DISCONNECT FIRST, NO CURRENT SHOULD FLOW THROUGH SENSORS
// note down the values and define them in ZERO_POINT_...
//#define DO_VOLTAGE_CALIBRATION
// 231, n=127, l=130
#define ZERO_POINT_L_N              463
#define ZERO_POINT_L_PE             495
#define ZERO_POINT_N_PE             483

#define FIRMWARE_VERSION            "2.18"
#define JSON_SIZE                   1024
#define DATA_TX_INTERVAL            800 // interval (ms) to send JSON data via serial to ESP-32 webserver
#define PIN_MISO                    50  // SPI  Master-In-Slave-Out
#define PIN_LED_RED                 13 // RED, LED OFF if we are running normally (not busy) (13=led_builtin on mega2560)
#define PIN_LED_YELLOW               11 // YELLOW, LED ON of we have active alarms
#define PIN_LED_BLUE                9 // BLUE
#define PIN_LED_WHITE               7 // WHITE, 
#define PIN_BUZZER                  8
#define PIN_ModIO_Reset             4
#define PIN_AM2320_SDA_PUMPROOM     2
#define PIN_AM2320_SCL_PUMPROOM     3
#define LED_BUSY                    PIN_LED_BLUE
#define LED_ALARM                   PIN_LED_RED
#define LED_WARNING                 PIN_LED_YELLOW

#define CONF_I2C_ID_MODIO_BOARD     0x58 // ID of MOD-IO board #1
#define CONF_RELAY_12VBUS           3 // MOD-IO relay controlling 12v bus (sensors, mains relays etc)
#define CONF_RELAY_WP               2 // MOD-IO relay controlling water pump

#define CORR_FACTOR_PRESSURE_SENSOR 0.5  // correction factor (linear) (in Bar)

#define Serial_Frontend             Serial1
//#define Serial_SensorHub            Serial2 // TO BE REMOVED AFTER SPI

#define numReadings                 10 // Define the number of samples to keep track of for ADC smoothing
#define PRESSURE_SENS_MAX           10 // sensor maxmimum value (in Bar*1), currently using a 0-10Bar sensor
#define ADC_CH_WATERP               A1 // ADC connected to water pressure sensor
//#define ADC_CH_TEMP_1               A0 // ADC connected to LM335 temp sensor 1
#define ADC_CH_CT_K2                A8 // ADC connected to current sensor K2 (living room) Input
#define ADC_CH_CT_K2_VDD_CALIB      4.45 // VDD calibrated (5.0*some factor) for current calc
#define ADC_CH_CT_K3                A6 // ADC connected to current sensor K3 (kitchen) Input
#define ADC_CH_CT_K3_VDD_CALIB      4.45 // VDD calibrated (5.0*some factor) for current calc
#define ADC_CH_VOLT_L_N             A3 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_L_PE            A4 // ADC connected to ZMPT101B voltage sensor
#define ADC_CH_VOLT_N_PE            A5 // ADC connected to ZMPT101B voltage sensor

#define DEF_EMON_ICAL_K2            0.40
#define DEF_EMON_ICAL_K3            3.02
#define DEF_CONF_WP_LOWER           3.75 // water pressure lower threshold (bar*100) before starting pump
#define DEF_CONF_WP_UPPER           4.20 // water pressure upper threshold (bar*100) before stopping pump
#define DEF_CONF_WP_MAX_RUNTIME     1800L  // max duration we should let pump run (seconds) (1800=30min)
#define DEF_CONF_WP_SUSPENDTIME     180L  // seconds to wait after alarms are cleared before we start pump again
#define DEF_CONF_WP_RUNTIME_ACC_ALARM  6    // if we run shorter than this we raise accumulator/low air pressure alarm
#define DEF_CONF_MIN_TEMP_PUMPHOUSE 4L   // minimum temp pumphouse in degrees C*10 before raising alarm
#define LOWMEM_LIMIT                50  // minimum free memory before raising alarm


// Structs

typedef struct
{
  //uint8_t ADC_CH_CT;
  //uint8_t ADC_CH_CT_VGND;
  //double ICAL;
  uint16_t apparentPower;
  //double Freq;
  double Irms;
  double PF;
  uint8_t error;
} emondata_type;

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
/*
typedef union {
  byte allBits;
  struct {
    byte OLD__2sec:1 ;
    byte OLD_1sec:1 ;
    byte OLD_500ms:1 ; 
    byte OLD_200ms:1 ;
    byte OLD_100ms:1;
    byte bitFive:1;
    byte bitSix:1;
    byte bitSeven:1; 
  };
} intervals_type;
*/
typedef struct {
  uint8_t pin = 8;
  //bool state = 0;
  //uint16_t duration = 1000;
  //uint16_t _mscnt = 0;
} Buzzer;

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
enum { PRESSURE_LOW=0, PRESSURE_OK=1, PRESSURE_HIGH=2 };

typedef struct
{
  uint8_t is_running;
  uint32_t state_age; // seconds since is_running was changed (avoid too frequent start/stops) (uint32_t = 0-4294967295)
  uint16_t start_counter; // increases +1 every time we start the waterpump (volatile, #nr of starts since boot)
  uint16_t suspend_timer; // seconds to wait after alarms are cleared before we start pump again
  bool is_suspended; // 1 if we have a suspension period after alarms before we can run again
  uint8_t suspend_count; // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
  uint32_t suspend_timer_total;
  uint32_t  total_runtime;
  double temp_pumphouse_val;
  double hum_pumphouse_val;
  double temp_motor_val;
  double water_pressure_bar_val;  
  uint8_t pressure_state;
  uint16_t pressure_state_t;
  bool accumulator_ok;
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
    byte accumulator_low_air:1;
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
#define LED_ON(pin) digitalWrite(pin, 0)
#define LED_OFF(pin) digitalWrite(pin, 1)
#define LED_TOGGLE(pin) digitalWrite(pin, !digitalRead(pin))

float readACCurrentValue(uint8_t pin, uint8_t ACTectionRange, double Vdd_calib);
bool getAlarmStatus_SYS(uint8_t Alarm);
bool getAlarmStatus_WP(uint8_t Alarm);
bool getAlarmStatus_EMON(uint8_t Alarm);
void buzzer_on(uint8_t pin, uint16_t duration);
void buzzer_off(uint8_t pin);
//bool getAlarmStatus(&struct ALARMS uint8_t Alarm);
int readline(int readch, char *buffer, int len);

#endif