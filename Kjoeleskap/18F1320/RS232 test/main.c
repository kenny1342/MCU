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


#use delay(clock=4000000)
#use rs232(baud=9600,parity=N,xmit=PIN_A6,rcv=PIN_A7,bits=8)

#use standard_io(A)

void main()
{

   
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_bit); // ISR 1ms
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
   //setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us, ISR 4ms
   setup_timer_2(T2_DIV_BY_4,255,1); // OF 1ms, ISR 1ms
   setup_timer_3(T3_INTERNAL|T3_DIV_BY_1); // OF 65,5ms
   
          
  
while(true)
{
  
 
  printf("Startup complete ");
delay_ms(1000);
}

}  
