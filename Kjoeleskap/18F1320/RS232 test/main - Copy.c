#include "18F1320.h"

/****** LEDS *************
com=low and DL1=high: green
com=high and DL1/DL2=low: orange
com=low and DL3=high: red
- com usually low = green
***************************/
#device adc=10 *=16

#FUSES NOWDT                 	//No Watch Dog Timer
#FUSES WDT128                	//Watch Dog Timer uses 1:128 Postscale
#FUSES INTRC_IO              	//Internal RC Osc, no CLKOUT
#FUSES NOFCMEN               	//Fail-safe clock monitor disabled
#FUSES NOBROWNOUT              	//Reset when brownout detected
#FUSES BORV27                	//Brownout reset at 2.7V
#FUSES NOPUT                 	//No Power Up Timer
#FUSES NOCPD                 	//No EE protection
#FUSES STVREN                	//Stack full/underflow will cause reset
#FUSES NODEBUG               	//No Debug mode for ICD
#FUSES NOLVP                 	//No low voltage prgming, B3(PIC16) or B5(PIC18) used for I/O
#FUSES NOWRT                 	//Program memory not write protected
#FUSES NOWRTD                	//Data EEPROM not write protected
#FUSES NOWRTC                	//configuration not registers write protected
#FUSES NOIESO                	//Internal External Switch Over mode disabled
//#FUSES IESO                  	//Internal External Switch Over mode enabled
#FUSES NOEBTR                	//Memory not protected from table reads
#FUSES NOEBTRB               	//Boot block not protected from table reads
#FUSES NOMCLR                	//Master Clear pin used for I/O
#FUSES NOPROTECT             	//Code not protected from reading
#FUSES NOCPB                 	//No Boot Block code protection
#FUSES NOWRTB                	//Boot block not write protected


#define VDD 4.95
#define MINIMUM_STOP_TIME 20  // minimum seconds between stop and start of motors


#define ON 1
#define OFF 0
#define BLINK 3
#define YES 1
#define NO 0
#define GREEN 0
#define AMBER 1


#use delay(clock=4000000)
//#use rs232(baud=9600,parity=N,xmit=PIN_A6,rcv=PIN_A7,bits=8)

 
//#use fast_io(A)
#use standard_io(A)
#use fast_io(B)

typedef struct
{
    int1 COM;
    int8 DL1:2; // 0-3 dec
    int8 DL2:2;
    int8 DL3:2;
    
} LEDStruct;
 
 typedef struct 
 {
     int1 fridge_temp;
     int1 freezer_temp;
 } AlarmStruct;
        
/*** SKAL VÆRE OK ***/
#define ADC_FREEZER     4 // p8 (B0/AN4) via R7 1K -> CN2-1 
#define ADC_FRIDGE      5 // p9 (B1/AN5) via R7 1K -> CN2-4
// OBS!! MÅ BYTTES MED P1 PÅ HW
#define ADC_POT_FREEZER 0 // p1 (A0/AN0)
#define ADC_POT_FRIDGE  6 // p10 (B4/AN6)

// TODO: maybe remove/short the resistors, not needed
#define SENSOR_COM_7K  PIN_B6 // p12 via R9 til sensor-com (pin 2+3 på CN2)
#define SENSOR_COM_1K  PIN_B7 // p13 via R10 til sensor-com (pin 2+3 på CN2)

#define POT_FRIDGE_R4   PIN_B4
#define POT_FREEZE_R5   PIN_A0

#define SW_FRIDGE  PIN_B2    // "super cool", kjøl, DL2 orange til ferdig?
#define SW_FREEZE  PIN_B3    // nedfrysning, DL1 orange til ferdig

#define LED_COM PIN_A1 // p2, common for LEDs
#define LED_DL3 PIN_B2 // p8 red led, alarm fryser
#define LED_DL1 PIN_B3 // p9 
#define LED_DL2 PIN_B5 // p11 "super cool" kjøl, orange en stund mens kjøl tvangskjøres?


// PINS OK, MEN MÅ SJEKKES I SKAPET HVILKEN MOTOR ER HVA
#define OUT_RL1 PIN_A2 // p6, rele HVA?
#define OUT_RL2 PIN_A3  // p7, rele HVA?

#define TRISA_NORMAL 0b10000000 // A7 RS232 RCV
#define TRISA_SAMPLE 0b10000001 // A0 is a POT, read by ADC. A7 RS232 RCV
//#define TRISA_NORMAL 0b00000000 // A7 RS232 RCV
//#define TRISA_SAMPLE 0b00000001 // A0 is a POT, read by ADC. A7 RS232 RCV

#define TRISB_LED 0b00010011  // B7-B0: B7-B6=SENSOR-COM, B5=DL2, B4=ANA-POT, B3-B2=LEDs & SW, B1-B0=ANA-SENSORS
#define TRISB_SW  0b00011111

/**** MACROS ***/
#define SET_RELAY1(STATUS) (output_bit(OUT_RL1, !STATUS))
#define SET_RELAY2(STATUS) (output_bit(OUT_RL2, !STATUS))
#define SET_LED(LED,STATUS) (output_bit(LED, !STATUS))
#define Start_Fridge_Motor (SET_RELAY1(ON), LEDs.DL2=ON)
#define Stop_Fridge_Motor (SET_RELAY1(OFF), LEDs.DL2=OFF)
#define Start_Freezer_Motor (SET_RELAY2(ON), LEDs.DL1=ON)
#define Stop_Freezer_Motor (SET_RELAY2(OFF), LEDs.DL1=OFF)
#define EnableSensors       (output_high(SENSOR_COM_1K), output_low(SENSOR_COM_7K)) // change to 1K resistor while reading sensors
// enable 7K resistor to pots for fine tuning the sensor R, and tie the other end of pot's to ground (remember TRIS before this macro!!)
#define EnablePots          (output_low(SENSOR_COM_1K), output_high(SENSOR_COM_7K), output_low(POT_FRIDGE_R4), output_low(POT_FREEZE_R5)) 


//enum LED_MODE {LED_ON, LED_OFF, LED_BLINK};

int1 do_read_temp, blink_toggle;
int1 run_fridge, run_freezer, fridge_running, freezer_running;
int1 SW_fridge_pressed, SW_freeze_pressed;

unsigned int8 ISR_counter1;
unsigned int16 ISR_counter0, ISR_counter2;
unsigned int8 fridge_stopped_secs, freezer_stopped_secs;
signed int8 temp_fridge, temp_pot_fridge, temp_freezer, temp_pot_freezer;

LEDStruct LEDs;
AlarmStruct Alarms;

/********* PROTOS *****************************/
unsigned int16 getAvgADC(unsigned int8 channel, unsigned int8 samples);
void readTemp(void);
void readPots(void);
signed int8 voltToDegrees(int32 volt);




#int_TIMER0
void  TIMER0_isr(void) // OF every 1ms
{
    if(++ISR_counter0 == 1000) // 1 sec
    {        
        if(!fridge_running)
            if(fridge_stopped_secs < 255) fridge_stopped_secs++;

        if(!freezer_running)
            if(freezer_stopped_secs < 255) freezer_stopped_secs++;
                        
        ISR_counter0 = 0;
    }
        
    set_tris_b(TRISB_SW); 
    if(!input(SW_FRIDGE)) SW_fridge_pressed = 1;// else SW_fridge_pressed = 0;
    if(!input(SW_FREEZE)) SW_freeze_pressed = 1;    
    set_tris_b(TRISB_LED); 
          
    
}

#int_TIMER1
void  TIMER1_isr(void) // OF 524ms
{
    if(++ISR_counter1 >= 4) // ~every 2 sec
    {    
        ISR_counter1 = 0;
        do_read_temp = 1;
    }    
}

#int_TIMER2
void  TIMER2_isr(void)  // OF 1ms
{
    
    if(++ISR_counter2 == 1000) // 1 sec
    {
        ISR_counter2 = 0;
        //blink_toggle ^=1;           
    }    

    output_bit(LED_COM, LEDs.COM);
    
    if(LEDs.DL1 == BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, LEDs.DL1);
    if(LEDs.DL2 == BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, LEDs.DL2);
    if(LEDs.DL3 == BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);
        
}

#int_TIMER3
void  TIMER3_isr(void) 
{

}

void main()
{

   
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_bit); // ISR 1ms
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
   //setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us, ISR 4ms
   setup_timer_2(T2_DIV_BY_4,255,1); // OF 1ms, ISR 1ms
   setup_timer_3(T3_INTERNAL|T3_DIV_BY_1); // OF 65,5ms
   
          
   
   setup_adc_ports(sAN4|sAN5|VSS_VDD); // Not the POTs
   setup_adc(ADC_CLOCK_INTERNAL|ADC_TAD_MUL_0);
   
   enable_interrupts(INT_TIMER0);
   enable_interrupts(INT_TIMER1);
   enable_interrupts(INT_TIMER2);
   enable_interrupts(INT_TIMER3);   
   enable_interrupts(GLOBAL);
   
//   set_tris_a(TRISA_NORMAL); 
   set_tris_b(TRISB_LED); 
   
  LEDs.COM = GREEN; 

//SET_RELAY1(OFF);
//SET_RELAY2(OFF);

while(true)
{
  
  // little boot sequence to show we starts up...
  LEDs=0;
  delay_ms(500);
  LEDs.DL1 = ON;
  delay_ms(500);
  LEDs.DL3 = ON;
  delay_ms(500);
  LEDs.DL2 = ON;
  delay_ms(1000);
    blink_toggle ^=1;
  LEDs.COM = blink_toggle;

//  printf("Startup complete ");
  //putc('K');
}
  
  EnablePots;  // enable voltage on the pot's via 7K R for fine-tuning the sensors R in the circuit
  
 // printf("Startup complete\r");
   
  while(true)
  {
    if(do_read_temp)
    {
        do_read_temp = 0;
        readTemp(); 
        readPots();     
    }
    
    /******* Logic for start/stop/keep running the FRIDGE *******/
    run_fridge = NO;
    run_freezer = NO;
    
    if(temp_fridge < temp_pot_fridge) 
    {        
        run_fridge = YES;
        
        if( (temp_pot_fridge - temp_fridge) >4)        
            Alarms.fridge_temp = ON;            
        else
            Alarms.fridge_temp = OFF;
    }        
        
    if(!fridge_running && run_fridge) // we are supposed to start
        if(fridge_stopped_secs < MINIMUM_STOP_TIME) // minimum XX secs between stop and start of motor, no hysteresis...
            run_fridge = NO; // abort start
            
    if(temp_pot_fridge == -100) // pot turned all off
        run_fridge = NO;
        
    /******* Logic for start/stop/keep running the FREEZER *******/    
    if(temp_freezer < temp_pot_freezer) 
    {
        run_freezer = YES;

        if( (temp_pot_freezer - temp_freezer) >8)        
            Alarms.freezer_temp = ON;            
        else
            Alarms.freezer_temp = OFF;
    }    

    if(!freezer_running && run_freezer) // we are supposed to start
        if(freezer_stopped_secs < MINIMUM_STOP_TIME) break; // minimum XX secs between stop and start of motor, no hysteresis...
            run_freezer = NO; // abort start
                
    if(temp_pot_freezer == -100) // pot turned all off
        run_freezer = NO;


    /**************** Actions *************************/            
    if(run_fridge) 
    {        
        fridge_running = YES;
        Start_Fridge_Motor;
        fridge_stopped_secs = 0;
    }    
    else
    {
        if(!fridge_running) break;
        Stop_Fridge_Motor;
        fridge_stopped_secs = 0;
    }

    if(run_freezer) 
    {        
        freezer_running = YES;
        Start_Freezer_Motor;
        freezer_stopped_secs = 0;
    }    
    else
    {
        if(!freezer_running) break;
        Stop_Freezer_Motor;
        freezer_stopped_secs = 0;
    }    
            
    if(Alarms.fridge_temp || Alarms.freezer_temp)
        LEDs.DL3 = ON;    
    else
        LEDs.DL3 = OFF;    
                
            
    while(SW_fridge_pressed)
    {
        LEDs.DL3 = !LEDs.DL3;        
        SW_fridge_pressed = 0;
        delay_ms(100);
    }
        
  }

}
/**
* R synker ved varme, må være NTC basert.
* samples:
* 1,4 = 8,2K
* 1 = 9K
* -17 = 21,4K
* -18 = 22K
* TODO: using "Scaled Integer" method
*/
void readTemp(void)
{
    //unsigned int16 adc_val_fridge_temp, adc_val_freezer_temp;
    unsigned int16 adc_val;
    unsigned int32 volt;
            
    EnableSensors;
    
    // Scalefactor for measure VDD (5V):
    //5.000 divide this number by 1023 (max output of 10 bit A/D) to
    //obtain the ScaleFactor. 5000000/1023 = 4888        
    //4.95V: 4950000/1023=4838
    adc_val = getAvgADC(ADC_FRIDGE, 5);
    volt = ((int32)adc_val/1023*4838) / 100000; // divide with 10000 for 2 fraction digits instead of 1
    temp_fridge = voltToDegrees(volt);
    
  //  printf("Fridge Temp/ADC/Volt: %d/%Lu/%03.1w\r", temp_fridge, adc_val, volt);
    //volt = ( (float) adc_val ) / 1023.0 * VDD; //adc_val * 5 / 1023;
    //printf("Fridge temp ADC/Volt: %Lu/%2.1f\r", adc_val, volt);
    
    
    adc_val = getAvgADC(ADC_FREEZER, 5);
    volt = ((int32)adc_val/1023*4838) / 100000; // divide with 10000 for 2 fraction digits instead of 1
    temp_freezer = voltToDegrees(volt);
    
   // printf("Freezer Temp/ADC/Volt: %d/%Lu/%03.1w\r", temp_freezer, adc_val, volt);
    
    
    
    
    EnablePots;
}

/**
* Read the pot's value, this is only used for alarm and turning on/off a motor
* We need to pull port down after reading it, because the pot's (via 7K resistor) fine-tunes the sensors in the circuit, when 
* not reading them via the 1K resistor
*/
void readPots(void)
{
    unsigned int16 adc_val_fridge_pot, adc_val_freezer_pot;    
    
    set_tris_a(TRISA_SAMPLE); 
    setup_adc_ports(sAN0|sAN4|sAN5|sAN6|VSS_VDD); // Inc the POTs   
    
    EnableSensors; // we need to power the pots via 1K to read them, just like the sensors...
   
    adc_val_fridge_pot = getAvgADC(ADC_POT_FRIDGE, 5);
    adc_val_freezer_pot = getAvgADC(ADC_POT_FREEZER, 5);
    
    setup_adc_ports(sAN4|sAN5|VSS_VDD); // Not the POTs
    
    
    //printf("ADC pot fridge/freezer: %Lu/%Lu\r", adc_val_fridge_pot, adc_val_freezer_pot);
    
    temp_pot_fridge = 4;
    temp_pot_freezer = -100; // means pot completly "off", telling us to stop motor etc...
    
    set_tris_a(TRISA_NORMAL); 
    EnablePots;
}

/**
* TODO: some magic math or lookup table...
*/
signed int8 voltToDegrees(int32 volt)
{

    return -16;    
}

        
/**
* Read X samples from ADC, average them and return result
* If samples=1, just one reading is done (rearly used)
**/
unsigned int16 getAvgADC(unsigned int8 channel, unsigned int8 samples)
{
   unsigned int16 temp_adc_value=0;
   unsigned int8 x;
   
   if(samples == 0) samples = 20;
   
   for (x=0;x<samples;x++)
   {
      set_adc_channel ( channel );
      delay_us(50);
      temp_adc_value += read_adc();
   }

   temp_adc_value /= samples;

   if (temp_adc_value < 1) temp_adc_value=1; // never divide by zero:

   return temp_adc_value;
}
