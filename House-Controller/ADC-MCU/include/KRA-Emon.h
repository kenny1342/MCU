
// TO REPLACE ZMPT101B CLASS


#ifndef KRAEMON_h
#define KRAEMON_h

#include <Arduino.h>


class KRAEMON {
public:
	KRAEMON(uint8_t _CurrentAnalogInputPin, uint8_t _VoltageAnalogInputPin, float _mVperAmpValue);
    //KRAEMON(uint8_t _CurrentAnalogInputPin, uint8_t _VoltageAnalogInputPin, double _Vdd_calib, uint8_t _ACTectionRange);
    void Calc() volatile;
    
    float RMSVoltageMean ;                            /* square roof of voltageMean*/
    float FinalRMSCurrent ;                           /* the final RMS current reading*/
    float apparentPower;                              /* the apparent power reading (VA) */
    float realPower = 0;                              /* the real power reading (W) */
    float powerFactor = 0;                            /* to display power factor value*/ 
    bool error = false;

private:
	void getCurrentAC() volatile;
	void getVoltageRmsAC() volatile;
    void getPower() volatile;
    double Vdd_calib = 4.45;
    int ACTectionRange = 20;                          // Default AC Current Sensor tection range (5A,10A,20A)
    int decimalPrecision = 2;                         // decimal places for all values shown in LED Display & Serial Monitor

    /* 1- AC Voltage Measurement */

    int voltageOffset1  = -9;                          // key in offset value
    int voltageOffset2  = -20;                          // key in offset value
    int VoltageAnalogInputPin = A2;                   // Which pin to measure voltage Value
    float voltageSampleRead  = 0;                     /* to read the value of a sample*/
    float voltageLastSample  = 0;                     /* to count time for each sample. Technically 1 milli second 1 sample is taken */
    float voltageSampleSum   = 0;                     /* accumulation of sample readings */
    float voltageSampleCount = 0;                     /* to count number of sample. */
    float voltageMean ;                               /* to calculate the average value from all samples*/ 
    


    /* 2- AC Current Measurement */

    int currentOffset1  = -5;                          // key in offset value
    int currentOffset2  = -13;                          // key in offset value
    int CurrentAnalogInputPin = A3;                   // Which pin to measure Current Value
    float mVperAmpValue = 31.25;                      // If using ACS712 current module : for 5A module key in 185, for 20A module key in 100, for 30A module key in 66
                                                        // If using "Hall-Effect" Current Transformer, key in value using this formula: mVperAmp = maximum voltage range (in milli volt) / current rating of CT
                                                        /* For example, a 20A Hall-Effect Current Transformer rated at 20A, 2.5V +/- 0.625V, mVperAmp will be 625 mV / 20A = 31.25mV/A */
    float currentSampleRead  = 0;                     /* to read the value of a sample*/
    float currentLastSample  = 0;                     /* to count time for each sample. Technically 1 milli second 1 sample is taken */
    float currentSampleSum   = 0;                     /* accumulation of sample readings */
    float currentSampleCount = 0;                     /* to count number of sample. */
    float currentMean ;                               /* to calculate the average value from all samples*/ 
    float RMSCurrentMean =0 ;                         /* square roof of currentMean*/


    /* 3- AC Power Measurement */

    int powerOffset =0;                               // key in offset value
    float sampleCurrent1 ;                            /* use to calculate current*/
    float sampleCurrent2 ;                            /* use to calculate current*/
    float sampleCurrent3 ;                            /* use to calculate current*/
    float powerSampleRead  = 0;                       /* to read the current X voltage value of a sample*/
    float powerLastSample   = 0;                      /* to count time for each sample. Technically 1 milli second 1 sample is taken */       
    float powerSampleCount  = 0;                      /* to count number of sample. */
    float powerSampleSum    = 0;                      /* accumulation of sample readings */         
    


};


#endif