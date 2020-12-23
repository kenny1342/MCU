#include "16F628A.h"

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES INTRC_IO                 //Internal RC Osc, no CLKOUT
//#FUSES RC_IO                    //Resistor/Capacitor Osc
//#FUES XT
#FUSES NOPUT                    //No Power Up Timer
#FUSES NOPROTECT                //Code not protected from reading
#FUSES NOBROWNOUT               //No brownout reset
#FUSES NOMCLR                   //Master Clear pin used for I/O
//#FUSES MCLR
#FUSES NOLVP                    //No low voltage prgming, B3(PIC16) or B5(PIC18) used for I/O
#FUSES NOCPD                    //No EE protection
//#FUSES RESERVED                 //Used to set the reserved FUSE bits

#define ON 1
#define OFF 0
#define GREEN 0
#define AMBER 1

#use delay(clock=4000000)
#use fast_io(A)

typedef struct
{
    int1 DL1;
    int1 DL2;
    int1 DL3;
    int1 COM;
} LEDStruct;
    
#define LED_DL3 PIN_B2 // p8 red led, alarm fryser
#define LED_DL1 PIN_B3 // p9 
#define LED_DL2 PIN_A2 // p1 "super cool" kjøl, orange en stund mens kjøl tvangskjøres?
#define LED_COM PIN_A3 // p2, common for LEDs

#define SW_FRIDGE  PIN_B2    // "super cool", kjøl, DL2 orange til ferdig?
#define SW_FREEZE  PIN_B3    // nedfrysning, DL1 orange til ferdig

#define SENSOR_FREEZER PIN_A0 // p17, CN2-1
#define SENSOR_FRIDGE  PIN_A1 // p18, CN2-4
#define SENSOR_COM_7K  PIN_B6 // via R9 til sensor-com (pin 2+3 på CN2)
#define SENSOR_COM_1K  PIN_B7 // via R10 til sensor-com (pin 2+3 på CN2)

#define OUT_RL1 PIN_B0 // p6, rele HVA?
#define OUT_RL2 PIN_B1  // p7, rele HVA?

#define ADC_FREEZER 0
#define ADC_FRIDGE  1

#define SET_RELAY1(STATUS) (output_bit(OUT_RL1, !STATUS))
#define SET_RELAY2(STATUS) (output_bit(OUT_RL2, !STATUS))

#define SET_LED(LED,STATUS) (output_bit(LED, !STATUS))

int1 SW_fridge_pressed, SW_freeze_pressed;
int8  AD_timeout;
int16 AD_val, AD_counter;

LEDStruct LEDs;


void get_AD();


#int_TIMER1
void  TIMER1_isr(void) // OF 524ms
{
    //get_AD();
}

#int_TIMER2
void  TIMER2_isr(void)  // OF 256us
{
    AD_counter++; // add 256us
}

#int_TIMER0
void  TIMER0_isr(void) // OF every 1ms
{
    //AD_counter++;
    
    if(++AD_timeout == 255) AD_timeout = 0;
    
    /*
    set_tris_a(0b11110011);
    if(!input(SW_FRIDGE)) SW_fridge_pressed = 1;// else SW_fridge_pressed = 0;
    if(input(SW_FREEZE)) SW_freeze_pressed = 1;    
    set_tris_a(0b11110000);
    */
    
    output_bit(LED_COM, LEDs.COM);
    output_bit(LED_DL1, LEDs.DL1);
    output_bit(LED_DL2, LEDs.DL2);
    output_bit(LED_DL3, LEDs.DL3);
    
}

void main()
{

   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4);
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
   //setup_timer_2(T2_DIV_BY_16,255,16); // OF 4ms
   setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us
   
   setup_comparator(NC_NC_NC_NC);
   setup_vref(FALSE);
   enable_interrupts(INT_TIMER1);
   enable_interrupts(INT_TIMER2);
   enable_interrupts(INT_TIMER0);
   enable_interrupts(GLOBAL);
   
   set_tris_a(0b11110000); // A7-A0, A0-A3 LEDs & SW
  
  LEDs.COM = GREEN; 
  LEDs=0;
  
  while(true)
  {
    
    //if(input(SENSOR_FREEZER)) LEDs.DL3 = ON; else LEDs.DL3 = OFF;
    //continue;
    
    get_AD();
    
    if(AD_val >0 && AD_val < 2)    
    {
        LEDs.DL1 = ON;
        LEDs.DL2 = OFF;
        LEDs.DL3 = OFF;
    }    
    
    if (AD_val >= 2 && AD_val < 4)
    {    
        LEDs.DL2 = ON;
        LEDs.DL1 = OFF;
        LEDs.DL3 = OFF;        
    }                
    
    if(AD_val >= 4)
    {
        LEDs.DL3 = ON;
        LEDs.DL1 = OFF;
        LEDs.DL2 = OFF;

    }    
  
  }
      
  while(true)
  {
    while(SW_fridge_pressed)
    {
        //LEDs.DL3 = !LEDs.DL3;
        //LEDs.COM = !LEDs.COM;
        SW_fridge_pressed = 0;
        delay_ms(100);
    }
    /*
    while(SW_freeze_pressed)
    {
        LEDs.DL3 = !LEDs.DL3;
        //LEDs.COM = !LEDs.COM;
        SW_freeze_pressed = 0;
        delay_ms(100);
    }
*/
     
    
    LEDs.DL1 = ON;
    //SET_RELAY1(ON);
    delay_ms(500);    
    LEDs.DL2 = ON;
    //SET_RELAY2(ON);
    
    delay_ms(500);    
    
    LEDs.DL1 = OFF;
    SET_RELAY1(OFF);
    delay_ms(500);    
    LEDs.DL2 = OFF;
    SET_RELAY2(OFF);
    delay_ms(500);
    
    
    //LEDs.DL3 = OFF;
    
    //delay_ms(5000);
    //output_high(LED_DL3);
  }

   while(0)
   {
/****** LEDS *************
com=low and DL1=high: green
com=high and DL1/DL2=low: orange
com=low and DL3=high: red
- com usually low = green
***************************/

      output_high(LED_COM); 
      output_low(LED_DL1);  
      output_high(LED_DL2);
      delay_ms(2000);
      output_high(LED_DL1);
      output_low(LED_DL2);
      delay_ms(2000);
      output_low(LED_COM);
      output_low(LED_DL1); 
      output_high(LED_DL2); 
      delay_ms(2000);
      output_high(LED_DL1);
      output_low(LED_DL2);
      delay_ms(2000);
      output_low(LED_DL1);
      output_low(LED_DL2);
      output_high(LED_DL3);
      delay_ms(2000);
      output_low(LED_DL3);
      
   }

}

void get_AD()
{
    AD_timeout = 0;
    
    output_float(SENSOR_COM_1K); 
    output_float(SENSOR_COM_7K); 
    
        // make pin output
    set_tris_a(0b11110000); // A0, A1 output
    // make the pin POSITIVE (+) for one milisecond
    
    output_high(SENSOR_FREEZER);
    //output_low(SENSOR_FREEZER);
    
    delay_ms(100);
    
    // Make the I/O pin as INPUT and measure how long stays as POSITIVE (+). 
    AD_counter=0;
    
    //output_low(SENSOR_COM_1K);
    //output_float(SENSOR_FREEZER);

    set_tris_a(0b11110011); // A0, A1 input
    
    //output_low(SENSOR_COM_1K);
    output_low(SENSOR_COM_7K);
    //output_high(SENSOR_COM_1K); // so cap can discharge
    //output_high(SENSOR_COM_7K); // so cap can discharge
    
    
    while(input(SENSOR_FREEZER) && AD_timeout < 254) 
    {
        //LEDs.DL3 = ON;
        AD_counter++;
    }
    //LEDs.DL3 = OFF;
    
    AD_val = AD_counter;

    //output_float(SENSOR_COM_1K); 
    //output_float(SENSOR_COM_7K); 

    //LEDs.DL2 = ON;
    //delay_ms(500);
    //LEDs.DL2 = OFF;
}    
/**
* Read X samples from ADC, average them and return result
* If samples=1, just one reading is done (rearly used)
**/
/*
unsigned int8 getAvgADC(unsigned int8 channel, unsigned int8 samples)
{
   unsigned int16 temp_adc_value=0;
   if(samples == 0) samples = 20;
   
   for (x=0;x<samples;x++)
   {
      set_adc_channel ( channel );
      delay_us(50);
      temp_adc_value += read_adc();
   }

   temp_adc_value /= samples;

   if (temp_adc_value < 1) temp_adc_value=1; // never divide by zero:

   return (int8) temp_adc_value;
}
*/