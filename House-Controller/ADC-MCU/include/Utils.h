#pragma once
#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>

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


long readVcc();

#endif