#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#define FIRMWARE_VERSION            "0.15"
#define numReadings                 10 // Define the number of samples to keep track of for ADC smoothing
#define PRESSURE_SENS_MAX           10 // sensor maxmimum value (in Bar*1), currently using a 0-10Bar sensor
#define CORR_FACTOR_PRESSURE_SENSOR 0.5  // correction factor (linear) (in Bar)
#define ADC_CH_WATERP               1 // ADC connected to water pressure sensor
#define ADC_CH_TEMP_1               0 // ADC connected to LM335 temp sensor 1
#define ADC_SAMPLE_COUNT            50 // nr of ADC readings to calculate average from

typedef struct
{
  uint16_t readings[numReadings];      // the readings from the analog input
  uint8_t readIndex = 0;              // the index of the current reading
  uint16_t total = 0;                  // the running total
  uint16_t average = 0;                // the average
} adc_avg;

typedef struct
{
  char temp_pumphouse[5];
  char water_pressure_bar[4];
} sensors_type;

/*
typedef struct
{
  //uint16_t runtime;
  uint8_t is_running;
  //uint32_t state_age; // seconds since is_running was changed (avoid too frequent start/stops) (uint32_t = 0-4294967295)
  //float pressure_bar;
  uint16_t pressure_bar;
  
  //uint16_t start_counter; // increases +1 every time we start the waterpump (volatile, #nr of starts since boot)
  //uint16_t suspend_timer; // seconds to wait after alarms are cleared before we start pump again
  bool is_suspended; // 1 if we have a suspension period after alarms before we can run again
  uint8_t suspend_count; // FOR DEBUG MOSTLY (in practice it also count number of times an ALARMBITS_WATERPUMP_PROTECTION alarm is set)
  //uint16_t pressure_bar_whole; // whole number
  //uint16_t pressure_bar_part; // decimal number
} waterpump_type;
*/
#endif