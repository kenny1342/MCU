#include <Arduino.h>
#include <KRA-Emon.h>

/**
 * Constructor
 * 
 * @_mVperAmpValue: If using ACS712 current module : for 5A module key in 185, for 20A module key in 100, for 30A module key in 66 \n
    If using "Hall-Effect" Current Transformer, key in value using this formula: mVperAmp = maximum voltage range (in milli volt) / current rating of CT \n
    For example, a 20A Hall-Effect Current Transformer rated at 20A, 2.5V +/- 0.625V, mVperAmp will be 625 mV / 20A = 31.25mV/A
 */
KRAEMON::KRAEMON(uint8_t _CurrentAnalogInputPin, uint8_t _VoltageAnalogInputPin, float _mVperAmpValue, const char* _id) {
    VoltageAnalogInputPin = _VoltageAnalogInputPin;
    CurrentAnalogInputPin = _CurrentAnalogInputPin;
    id = _id;
    mVperAmpValue = _mVperAmpValue;
}

void KRAEMON::Calc() volatile {
    this->getVoltageRmsAC();
    this->getCurrentAC();
    this->getPower();
}

void KRAEMON::getVoltageRmsAC() volatile {

    //Serial.println("CALC1");

if(millis() >= voltageLastSample + 1 )                                                    /* every 1 milli second taking 1 reading */
    {
    voltageSampleRead = 2*(analogRead(VoltageAnalogInputPin)- 512) + voltageOffset1;      /* read the sample value */           
    voltageSampleSum = voltageSampleSum + sq(voltageSampleRead) ;                         /* accumulate value with older sample readings*/     
    voltageSampleCount = voltageSampleCount + 1;                                          /* to move on to the next following count */
    voltageLastSample = millis() ;                                                        /* to reset the time again so that next cycle can start again*/ 
    //Serial.println("CALC2"); Serial.println(voltageSampleCount);
    }

if(voltageSampleCount == 1000)                                                            /* after 1000 count or 1000 milli seconds (1 second), do the calculation and display value*/
    {           
    voltageMean = voltageSampleSum/voltageSampleCount;                                    /* calculate average value of all sample readings taken*/
    RMSVoltageMean = sqrt(voltageMean)+ voltageOffset2;                                   /* square root of the average value*/
    //Serial.print("Voltage RMS: ");
    //Serial.print(RMSVoltageMean,decimalPrecision);
    //Serial.println("V  ");
    voltageSampleSum =0;                                                                  /* to reset accumulate sample values for the next cycle */
    voltageSampleCount=0;                                                                 /* to reset number of sample for the next cycle */
    //Serial.println("CALC3");
    }
}


void KRAEMON::getCurrentAC() volatile {
/*
    peakVoltage += analogRead(CurrentAnalogInputPin);   //read peak voltage
  peakVoltage = peakVoltage / readcount;   
  voltageVirtualValue = peakVoltage * 0.707;    //change the peak voltage to the Virtual Value of voltage

  //The circuit is amplified by 2 times, so it is divided by 2.
  voltageVirtualValue = (voltageVirtualValue / 1024 * Vdd_calib ) / 2;  
  FinalRMSCurrent = voltageVirtualValue * ACTectionRange;
  */
    
    
    //mVperAmpValue = 50;

    if(millis() >= currentLastSample + 1)                                                     /* every 1 milli second taking 1 reading */
        {
        //currentSampleRead = analogRead(CurrentAnalogInputPin)-512 + currentOffset1;           /* read the sample value */
        currentSampleRead = analogRead(CurrentAnalogInputPin) + currentOffset1;
        //currentSampleSum = currentSampleSum + sq(currentSampleRead) ;                         /* accumulate value with older sample readings*/
        currentSampleSum += currentSampleRead;
        currentSampleCount = currentSampleCount + 1;                                          /* to move on to the next following count */
        currentLastSample = millis();                                                         /* to reset the time again so that next cycle can start again*/ 
        }

    if(currentSampleCount == 1000)                                                            /* after 1000 count or 1000 milli seconds (1 second), do the calculation and display value*/
        {       
        RMSCurrentMean = (currentSampleSum/currentSampleCount) * 0.707;                                    /* calculate average value of all sample readings taken*/
        RMSCurrentMean += currentOffset2;
        FinalRMSCurrent = (((RMSCurrentMean /1024) *AC_CURRENT_VDD_CALIB) /mVperAmpValue) / 2;

        //RMSCurrentMean = sqrt(currentMean)+currentOffset2 ;
        /*
        RMSCurrentMean = sqrt(currentMean)+currentOffset2 ;                                   // square root of the average value
        FinalRMSCurrent = (((RMSCurrentMean /1024) *AC_CURRENT_VDD_CALIB) /mVperAmpValue) / 2;           // calculate the final RMS current
        */
        //Serial.print("I RMS: ");
        //Serial.println(FinalRMSCurrent,decimalPrecision);
        //Serial.println(" A  ");
        
        currentSampleSum =0;                                                                  /* to reset accumulate sample values for the next cycle */
        currentSampleCount=0;                                                                 /* to reset number of sample for the next cycle */
        }


}


 /* 3- AC Power with Direction */
void KRAEMON::getPower() volatile {
    if(millis() >= powerLastSample + 1)                                                       /* every 1 milli second taking 1 reading */
        {
        sampleCurrent1 = 2*analogRead(CurrentAnalogInputPin)-512+ currentOffset1;
        //sampleCurrent1 = analogRead(CurrentAnalogInputPin)+ currentOffset1;
        sampleCurrent2 = (sampleCurrent1/1024)*5000;
        //sampleCurrent2 = (sampleCurrent1/1024)*AC_CURRENT_VDD_CALIB;
        sampleCurrent3 = sampleCurrent2/mVperAmpValue;
        voltageSampleRead = 2*(analogRead(VoltageAnalogInputPin)- 512)+ voltageOffset1 ;
        //voltageSampleRead = 2*(analogRead(VoltageAnalogInputPin))+ voltageOffset1 ;
        powerSampleRead = voltageSampleRead * sampleCurrent3 ;                                /* real power sample value */
        powerSampleSum = powerSampleSum + powerSampleRead ;                                   /* accumulate value with older sample readings*/
        powerSampleCount = powerSampleCount + 1;                                              /* to move on to the next following count */
        powerLastSample = millis();                                                           /* to reset the time again so that next cycle can start again*/ 
        }
    
    if(powerSampleCount == 1000)                                                              /* after 1000 count or 1000 milli seconds (1 second), do the calculation and display value*/
        {
        realPower = ((powerSampleSum/powerSampleCount)+ powerOffset) ;                        /* calculate average value of all sample readings */
        //Serial.print("Real Power (W): ");
        //Serial.print(realPower);
        //Serial.println(" W  ");           
        apparentPower= FinalRMSCurrent*RMSVoltageMean;                                       /*Apparent power do not need to recount as RMS current and RMS voltage values available*/
        //Serial.print("Apparent Power (VA): ");
        //Serial.print(apparentPower,decimalPrecision);
        //Serial.println(" VA ");
        powerFactor = (float) (realPower/apparentPower);    
        if(powerFactor >1 || powerFactor<0)
        {
            powerFactor = 0;
        }
        //powerFactor = powerFactor / 1.0 * 100;
        //Serial.print("Power Factor: ");
        //Serial.println(powerFactor,decimalPrecision);  
        //Serial.println(" ");                                           
        powerSampleSum =0;                                                                    /* to reset accumulate sample values for the next cycle */
        powerSampleCount=0;                                                                   /* to reset number of sample for the next cycle */
        }
}