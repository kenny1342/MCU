#include "C:\kenny\PIC\Kjøleskap\wizard\main.h"
  #ZERO_RAM
#int_TIMER3
void  TIMER3_isr(void) 
{

}



void main()
{

   setup_adc_ports(sAN0|sAN4|sAN5|sAN6|VSS_VDD);
   setup_adc(ADC_CLOCK_INTERNAL|ADC_TAD_MUL_0);
   setup_wdt(WDT_OFF);
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_bit);
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8);
   setup_timer_2(T2_DIV_BY_4,255,1);
   setup_timer_3(T3_INTERNAL|T3_DIV_BY_1);
   enable_interrupts(INT_TIMER3);
   enable_interrupts(GLOBAL);
   setup_oscillator(OSC_8MHZ|OSC_INTRC);

   // TODO: USER CODE!!

}
