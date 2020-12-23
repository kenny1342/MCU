#include "18F1320.h"

/****** LEDS *************
com=low and DL1=high: green
com=high and DL1/DL2=low: orange
com=low and DL3=high: red
- com usually low = green
***************************/

/****** SWITCHES *************
Alltid lav på en side
For å teste SW2:
- Kjør B2 høy
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

#BYTE PORTB=0xF81
//#BIT  B2bit=PORTB.2
//#BIT  B5bit=PORTB.5

#define VDD 4.95
#define MINIMUM_STOP_TIME 10    // 40 in prod. minimum seconds between stop and start of motors
#define MAX_RUNTIME_MINS  50     // 50 in prod. max minutes a motor can run (fail-safe system...)

#define ON 1
#define OFF 0
#define BLINK 3
#define YES 1
#define NO 0
#define GREEN 0
#define AMBER 1

#define SAMPLE_COUNT 2//20 // number of samples for avg reading of temps (ISR TIMER1 sets interval)

#define INVALID_SENSOR_VOLTAGE 127

#define FREEZER_TO_WARM -13
#define FREEZER_TO_COLD -28 //fiktivt, setter dette om unormalt lav temp eller ukjent feil

#define FRIDGE_TO_WARM 6
#define FRIDGE_TO_COLD -2 //fiktivt, setter dette om unormalt lav temp eller ukjent feil

#define POT_OFF -100
#define POT_MAX -127
// Used for alerting
#define FREEZER_MIN_TEMP -18 

#define FREEZER_MAX_DIFF 3   // max diff between pot and real temp
#define FRIDGE_MIN_TEMP 4    
#define FRIDGE_MAX_DIFF 2    // max diff between pot and real temp

#define FREEZER_MAX_RUN_MIN  60 // never run longer than X mins (except if deep-freeze selected)
#define FREEZER_MIN_RUN_MIN  5  // never run less than X mins
#define FREEZER_MIN_STOP_MIN 5   // always stop for X mins

#define FRIDGE_MAX_RUN_MIN  60  // never run longer than X mins (except if deep-freeze selected)
#define FRIDGE_MIN_RUN_MIN  20  // never run less than X mins
#define FRIDGE_MIN_STOP_MIN 5   // always stop for X mins

#use delay(clock=4000000)
//#use rs232(baud=9600,parity=N,xmit=PIN_A6,rcv=PIN_A1,bits=8)
#use rs232(baud=9600,parity=N,xmit=PIN_A6,rcv=PIN_A1,bits=8,INVERT)


#use fast_io(A)
//#use standard_io(A)
#use fast_io(B)

#include <math.h>


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

 typedef struct 
 {
     signed int8 fridge;
     signed int8 pot_fridge;
     signed int8 freezer;
     signed int8 pot_freezer;
 } TempsStruct;
 
//signed int8 fridge, pot_fridge, freezer, pot_freezer;
/****** LEDIG ***********/
// A0 p1

        
/*** SKAL VÆRE OK ***/
//#define ADC_FREEZER     5 // p9 (B1/AN5) via R8 1K -> CN2-4
//#define ADC_FRIDGE      4 // p8 (B0/AN4) via R7 1K -> CN2-1 

#define ADC_FRIDGE     5 // p9 (B1/AN5) via R8 1K -> CN2-4
#define ADC_FREEZER      4 // p8 (B0/AN4) via R7 1K -> CN2-1 

#define ADC_POT_FREEZER 1 // p2 (A1/AN1)
#define ADC_POT_FRIDGE  6 // p10 (B4/AN6)

#define POT_FRIDGE_PULLDOWN   PIN_A0 // p1 setting low when ad read the pots
//#define SENSOR_COM_7K  PIN_B6 // p12 via R9 til sensor-com (pin 2+3 på CN2)
#define POT_FREEZER_PULLDOWN  PIN_B6 // p12 (was SENSOR_COM_7K R9 til sensor-com, pin 2+3 på CN2)

#define SENSOR_COM      PIN_B7 // p13 via R10 til sensor-com (pin 2+3 på CN2)

// TODO: finn ut hva som trigger Q5....
#define TRANSISTOR_Q5C  PIN_A5 //p4 MCLR, en transistor som tydligvis kjører pinne high/low av en eller annen grunn....?

// TODO:
#define VOLTAGE         PIN_A4 //p3 , denne har konstant +5..... og noe mer, SJEKK UT DETTE!!!

#define POT_FRIDGE_R4   PIN_B4 //p10
#define POT_FREEZE_R5   PIN_A1 // p2

#define SW_FRIDGE  PIN_B5    // p17 SW2 "super cool", kjøl, DL2 orange til ferdig?
#define SW_FREEZE  PIN_B2    // SW3 nedfrysning, DL1 orange til ferdig

//#define LED_COM PIN_A1 // p2, common for LEDs
#define LED_COM PIN_A7 // p16, common for LEDs
#define LED_DL3 PIN_B2 // p17 red led, alarm fryser
#define LED_DL1 PIN_B3 // p9 
#define LED_DL2 PIN_B5 // p11 "super cool" kjøl, orange en stund mens kjøl tvangskjøres?


// PINS OK, MEN MÅ SJEKKES I SKAPET HVILKEN MOTOR ER HVA
#define OUT_RL1 PIN_A2 // p6, rele HVA?
#define OUT_RL2 PIN_A3  // p7, rele HVA?

#define TRISA_NORMAL         0b00110011 // A4 +5, A1 is a POT, read by ADC. A0=POT_FRIDGE_PULLDOWN, needs to float (input) when not sampling. (same with B6)
#define TRISA_SAMPLE_SENSORS 0b00110011 // A4 +5, A1 is a POT, read by ADC. A0=POT_FRIDGE_PULLDOWN, output low when sampling. (same with B6)
#define TRISA_SAMPLE_POTS    0b00110010 // A4 +5, A0 pulldown output

#define TRISB_NORMAL         0b01010011  //0b01010011 B7-B0: B7=SENSOR-COM, B6=PULLDOWN, B5=DL2+SW3, B4=ANA-POT, B3=DL1,B2=DL3+SW2, B1-B0=ANA-SENSORS
//#define TRISB_NORMAL 0b00110111
#define TRISB_SAMPLE_SENSORS 0b01110111 
#define TRISB_SAMPLE_POTS    0b00110111 // B6 pulldown output

/**** MACROS ***/
#define SET_RELAY1(STATUS) (output_bit(OUT_RL1, !STATUS))
#define SET_RELAY2(STATUS) (output_bit(OUT_RL2, !STATUS))
#define SET_LED(LED,STATUS) (output_bit(LED, !STATUS))
#define Start_Fridge_Motor (SET_RELAY1(ON), LEDs.DL2=ON, fridge_running=YES, started2++)
#define Stop_Fridge_Motor (SET_RELAY1(OFF), LEDs.DL2=OFF, fridge_running=NO, stopped2++)
#define Start_Freezer_Motor (SET_RELAY2(ON), LEDs.DL1=ON, freezer_running=YES, started1++)
#define Stop_Freezer_Motor (SET_RELAY2(OFF), LEDs.DL1=OFF, freezer_running=NO, stopped1++)
//#define EnableSensors       (disable_interrupts(INT_TIMER2), output_high(SENSOR_COM_1K), output_high(SENSOR_COM_7K)) // change to 1K resistor while reading sensors
// enable 7K resistor to pots for fine tuning the sensor R, and tie the other end of pot's to ground (remember TRIS before this macro!!)
//#define EnablePots          (enable_interrupts(INT_TIMER2), output_float(SENSOR_COM_1K), output_high(SENSOR_COM_7K), output_low(POT_FRIDGE_R4), output_low(POT_FREEZE_R5)) 

#define EnableSensors       (output_low(SENSOR_COM), sampling=YES) // Turn transistor ON to power sensors 
// enable 7K resistor to pots for fine tuning the sensor R, and tie the other end of pot's to ground (remember TRIS before this macro!!)
#define DisableSensors      (output_high(SENSOR_COM), sampling=NO) // Turn transistor OFF to turn off sensors

//enum LED_MODE {LED_ON, LED_OFF, LED_BLINK};

int1 do_read_temp, do_actions, blink_toggle;
int1 run_fridge, run_freezer, fridge_running, freezer_running;
int1 SW_fridge_pressed, SW_freezer_pressed;
int1 sampling;

unsigned int8 ISR_counter1;
unsigned int16 ISR_counter0, ISR_counter2, ISR_max_run_c;
unsigned int8 fridge_stopped_mins, freezer_stopped_mins;
volatile unsigned int8 fridge_run_mins, freezer_run_mins;
//signed int8 fridge, pot_fridge, freezer, pot_freezer;
//volatile unsigned int32 volt;
static unsigned int16 freezer_avg, fridge_avg;
static unsigned int8 freezer_avg_count, fridge_avg_count;

static unsigned int8 started1, started2, stopped1, stopped2;

TempsStruct Temps;
LEDStruct LEDs;
AlarmStruct Alarms;

unsigned int8 s_debug[50];
//static unsigned int16 
static signed int8 freezer_min, freezer_max, fridge_min, fridge_max;

//signed int8 CONST FridgeLookup[8] = {POT_OFF,5,4,3,2,1,0,POT_MAX};
//signed int8 CONST TempLookup2[41] = {-25,-24,-23,-22,-21,-20,-19,-18,-17,-16,-15,-14,-13,-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

    
/********* PROTOS *****************************/
unsigned int16 getAvgADC(unsigned int8 channel, unsigned int8 samples);
void doActions(void);
void readTemp(void);
void readPots(void);
signed int8 SensorADToDegrees(int16 ad_val, int1 is_fridge);
void debug(void);



#int_TIMER0
void  TIMER0_isr(void) // OF every 1ms
{
        
    if(++ISR_counter0 == 60000) // 60 sec
    {        
        if(!fridge_running)
            if(fridge_stopped_mins < 255) fridge_stopped_mins++;

        if(!freezer_running)
            if(freezer_stopped_mins < 255) freezer_stopped_mins++;
                        
        ISR_counter0 = 0;
    }

    if(!sampling)
    {
        set_tris_b(TRISB_SAMPLE_SENSORS);  // this inc switches
        
        delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
        if(bit_test(PORTB, 2)) SW_fridge_pressed = NO; else SW_fridge_pressed = YES;        
        if(bit_test(PORTB, 5)) SW_freezer_pressed = NO; else SW_freezer_pressed = YES;   
        
        
        //if(!input(SW_FRIDGE)) SW_fridge_pressed = 1; else SW_fridge_pressed = 0;
        //if(!input(SW_FREEZE)) SW_freezer_pressed = 1; else SW_freezer_pressed = 0;
        
        set_tris_b(TRISB_NORMAL); 
    }          
    
}

#int_TIMER1
void  TIMER1_isr(void) // OF 524ms
{
        
    ISR_counter1++;
    if((ISR_counter1 % 2) == 0) // ~every 1 sec
    {    
        //ISR_counter1 = 0;
        do_read_temp = 1;
    }    
    
    if(ISR_counter1 == 120) // ~every 60 sec
    {    
        ISR_counter1 = 0;
        do_actions = 1;
    }    
    
    if(++ISR_max_run_c >= 120L) // 1 min
    {
        if(fridge_running) fridge_run_mins++; else fridge_run_mins = 0L;
        if(freezer_running) freezer_run_mins++; else freezer_run_mins = 0L;
        ISR_max_run_c = 0;
    }   
}

#int_TIMER2
void  TIMER2_isr(void)  // OF 1ms
{
    
    if(++ISR_counter2 == 1000) // 1 sec
    {
        ISR_counter2 = 0;
        blink_toggle ^=1;           
    }    
    
    output_bit(LED_COM, LEDs.COM);
    
    /*// TEST
    output_bit(LED_COM, AMBER);
    output_bit(LED_DL1, 0);
    output_bit(LED_DL2, 0);
    output_bit(LED_COM, GREEN);
    output_bit(LED_DL1, 0);
    output_bit(LED_DL2, 1);
*/
    
    if(LEDs.COM == AMBER)
    {
        if(LEDs.DL1 == BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, !LEDs.DL1);
        if(LEDs.DL2 == BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, !LEDs.DL2);
        if(LEDs.DL3 == BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, !LEDs.DL3);
    }
    else
    {    
        if(LEDs.DL1 == BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, LEDs.DL1);
        if(LEDs.DL2 == BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, LEDs.DL2);
        if(LEDs.DL3 == BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);
    }            
    
}

#int_TIMER3
void  TIMER3_isr(void) // 65.5ms
{
//do_read_temp = 1;
}

#zero_ram
void main()
{

   
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_bit); // ISR 1ms
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
   //setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us, ISR 4ms
   setup_timer_2(T2_DIV_BY_4,255,1); // OF 1ms, ISR 1ms
   setup_timer_3(T3_INTERNAL|T3_DIV_BY_1); // OF 65,5ms
   
          
   
//   setup_adc_ports(sAN4|sAN5|VSS_VDD); // Not the POTs
   setup_adc_ports(sAN1|sAN4|sAN5|sAN6|VSS_VDD); // Inc the POTs   
   setup_adc(ADC_CLOCK_INTERNAL|ADC_TAD_MUL_0);
   setup_ccp1(CCP_OFF);
   port_b_pullups(FALSE);   
   
   enable_interrupts(INT_TIMER0);
   enable_interrupts(INT_TIMER1);
   enable_interrupts(INT_TIMER2);
   enable_interrupts(INT_TIMER3);   
   enable_interrupts(GLOBAL);
   
   
  LEDs.COM = GREEN; 
  Stop_Fridge_Motor;
  Stop_Freezer_Motor;
  fridge_stopped_mins = FRIDGE_MIN_STOP_MIN; // instant turn-on at power on
  freezer_stopped_mins = FREEZER_MIN_STOP_MIN; // instant turn-on at power on
  do_actions = 1;
    
  DisableSensors; 
  set_tris_a(TRISA_NORMAL); 
  set_tris_b(TRISB_NORMAL); 
  
    
  strcpy(s_debug, "Startup complete"); debug();
     
  while(true)
  {
    while(SW_fridge_pressed)
    {
        //LEDs.DL3 = !LEDs.DL3;        
        //LEDs.COM = !LEDs.COM;
        //SW_fridge_pressed = 0;
        sprintf(s_debug, "Delays for 10 sec"); debug();        
        delay_ms(5000);delay_ms(5000);
        
    }
    while(SW_freezer_pressed)
    {
        sprintf(s_debug, "Delays for 5 sec"); debug();        
        delay_ms(5000);        
    }    

    if(do_read_temp && !sampling)
    {
        sprintf(s_debug, "SW2: %u, SW3: %u", SW_fridge_pressed, SW_freezer_pressed); debug();
        sprintf(s_debug, "started/stopped freezer: %u, %u", started1, stopped1-1); debug();
        sprintf(s_debug, "started/stopped fridge: %u, %u", started2, stopped2-1); debug();
        
        do_read_temp = 0;
        readTemp(); 
        readPots();     
    }
    
    /******* Logic for start/stop/keep running the FRIDGE *******/
    // TODO: USE FRXXXX_TO_COLD
    
    run_fridge = NO;
    run_freezer = NO;
    Alarms.fridge_temp = OFF;
    Alarms.freezer_temp = OFF;
    
    if( (Temps.fridge > Temps.pot_fridge || Temps.fridge == FRIDGE_TO_WARM) && Temps.fridge != INVALID_SENSOR_VOLTAGE && fridge_run_mins < MAX_RUNTIME_MINS) 
    {        
        run_fridge = YES;
        
        if( (Temps.pot_fridge > Temps.fridge && (Temps.pot_fridge - Temps.fridge) > FREEZER_MAX_DIFF ) || Temps.fridge > FRIDGE_MIN_TEMP)        
            Alarms.fridge_temp = ON;            
        else
            Alarms.fridge_temp = OFF;
    }        

    if(Temps.fridge == INVALID_SENSOR_VOLTAGE) Alarms.fridge_temp = ON;
        
    if(!fridge_running && run_fridge) // we are supposed to start
        if(fridge_stopped_mins < FRIDGE_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
            run_fridge = NO; // abort start
            
    if(Temps.pot_fridge == POT_OFF) // pot turned all off
        run_fridge = NO;
        
    /******* Logic for start/stop/keep running the FREEZER *******/    
    if(Temps.pot_freezer == POT_OFF) // pot turned all off
        run_freezer = NO;
    else if(freezer_run_mins > FREEZER_MAX_RUN_MIN) // run too long
    {
        run_freezer = NO;
        sprintf(s_debug, "freezer_run_mins > FREEZER_MAX_RUN_MIN: %u > %u", freezer_run_mins, FREEZER_MAX_RUN_MIN); debug();
    }    
    else if( (Temps.freezer > Temps.pot_freezer || Temps.freezer == FREEZER_TO_WARM) && Temps.freezer != INVALID_SENSOR_VOLTAGE) 
    {
        run_freezer = YES;
        //sprintf(s_debug, "YES1"); debug();
    }    
    
    if(freezer_running && !run_freezer) 
        if(freezer_stopped_mins < FREEZER_MIN_RUN_MIN)
            run_freezer = YES; // abort stop if not run minimum time
        
    
    if(!freezer_running && run_freezer) // we are supposed to start
        if(freezer_stopped_mins < FREEZER_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
        {    
            run_freezer = NO; // abort start
            //sprintf(s_debug, "NO1"); debug();
        }         

    doActions();

    if( (Temps.pot_freezer > Temps.freezer && (Temps.pot_freezer - Temps.freezer) > FRIDGE_MAX_DIFF ) || Temps.freezer > FREEZER_MIN_TEMP)        
        Alarms.freezer_temp = ON;            
    else
        Alarms.freezer_temp = OFF;        
    
    if(Temps.freezer == INVALID_SENSOR_VOLTAGE) Alarms.freezer_temp = ON;


//LEDs.DL3 = OFF;    

            
    if(Alarms.fridge_temp || Alarms.freezer_temp)
        LEDs.DL3 = BLINK;    
    else
        LEDs.DL3 = OFF;    
                
            
        
  }

}

/**************** Actions *************************/            
void doActions(void)
{
    if(!do_actions) return;    
    
    do_actions = NO;
    
// TEST
//run_fridge = YES;
//run_freezer = YES;

    
    if(run_fridge && !fridge_running) 
    {           
        Start_Fridge_Motor;
        
        sprintf(s_debug, "Starting fridge, runtime: %u", fridge_run_mins); debug();       
    }    
    
    if(!run_fridge && fridge_running)
    {
        Stop_Fridge_Motor;
        fridge_stopped_mins = 0;        
        sprintf(s_debug, "Stopping fridge, runtime: %u", fridge_run_mins); debug();       
    }    

    if(run_freezer && !freezer_running) 
    {           
        Start_Freezer_Motor;
        
        sprintf(s_debug, "Starting freezer, runtime: %u", freezer_run_mins); debug();       
    }    
    
    if(!run_freezer && freezer_running) 
    {
        Stop_Freezer_Motor;
        freezer_stopped_mins = 0;
        sprintf(s_debug, "Stopping freezer, runtime: %u", freezer_run_mins); debug();       
    }
    
}
    
/**
* R synker ved varme, må være NTC basert.
* samples:
* 1,4 = 8,2K
* 1 = 9K
* -17 = 21,4K
* -18 = 22K
* 
*/
void readTemp(void)
{
    //unsigned int16 adc_val_fridge_temp, adc_val_freezer_temp;
    unsigned int16 adc_val, n;
    //unsigned int32 volt;
            
    EnableSensors;
    set_tris_a(TRISA_SAMPLE_SENSORS); 
    set_tris_b(TRISB_SAMPLE_SENSORS); 
           
    // Scalefactor for measure VDD (5V):
    //5.000 divide this number by 1023 (max output of 10 bit A/D) to
    //obtain the ScaleFactor. 5000000/1023 = 4888        
    //4.95V: 4950000/1023=4838
    
    adc_val = getAvgADC(ADC_FREEZER, 0);
    //volt = ((int32)adc_val*4838) / 10000; // divide with 100000 for 1 fraction digits instead of 2    
    
    freezer_avg += adc_val;
    //if(adc_val > freezer_max) freezer_max = adc_val;
    //if(adc_val < freezer_min) freezer_min = adc_val;
    
    
    adc_val = getAvgADC(ADC_FRIDGE, 0);
    //volt = ((int32)adc_val*4838) / 10000; // divide with 100000 for 1 fraction digits instead of 2
    
    fridge_avg += adc_val;    

    // MAYBE USE MIN INSTEAD OF AVG, for alltid kaldest sample
    // feks fridge_avg += fridge_min
    

        
    if(++freezer_avg_count == SAMPLE_COUNT)
    {
        freezer_avg_count = 0;        
        freezer_avg /= SAMPLE_COUNT;
        
        //M går, 642,652=-20                        
        if(freezer_avg > 890) //830 FINN RETT VERDI HER!       
        {
            n=95; //lavere øker temp
        //    sprintf(s_debug, "----freezer_avg > 700 (%Lu), subtracting %Lu = %Lu ------", freezer_avg, n, freezer_avg-n); debug();    
        //    freezer_avg -= n; // no idea why sensors change if motors running/not running...trekk fra mindre for å øke temp
        }    
        /*
        else
        {            
            if(freezer_avg > (freezer_min+100L))
            {                
                //sprintf(s_debug, "----freezer_avg > min+100 (%Lu+100), subtracting 100 = %Lu ------", freezer_min, freezer_avg-100); debug();    
                //freezer_avg -=100; // etter motor har stoppet....
            }    
            else
                //freezer_avg -=150; //35 mens motor går... mer øker temp
           
        }
        */
        
        Temps.freezer = SensorADToDegrees(freezer_avg, NO);
        //if(Temps.freezer > freezer_max) freezer_max = Temps.freezer;
        //if(Temps.freezer < freezer_min) freezer_min = Temps.freezer;

        sprintf(s_debug, "Freezer Temp/AVG, Min/Max: %d/%Lu, %d/%d", Temps.freezer, freezer_avg, freezer_min, freezer_max); debug();    
        //freezer_max = 0; freezer_min = 0;
        freezer_avg = 0;
    }    


    if(++fridge_avg_count == SAMPLE_COUNT)
    {
        fridge_avg_count = 0;        
        fridge_avg /= SAMPLE_COUNT;

        // M går, 396=0, 384=-1
        if(fridge_avg < 1) //820if'er går under dette. 570 450 440
        {
            n=260; //230 290 mer øker temp
            sprintf(s_debug, "---FRIDGE ad_val < 820 (%Lu), subtracting %Lu = %Lu ------", fridge_avg, n, fridge_avg+n); debug();    
            fridge_avg += n; // no idea why sensors change if motors running/not running...
        }
        else
        {    
            //fridge_avg+=30;
        }    


        Temps.fridge = SensorADToDegrees(fridge_avg, YES);
        //if(Temps.fridge > fridge_max) fridge_max = Temps.fridge;
        //if(Temps.fridge < fridge_min) fridge_min = Temps.fridge;    
        
        sprintf(s_debug, "Fridge Temp/AVG, Max/Min: %d/%Lu, %d/%d", Temps.fridge, fridge_avg, fridge_max, fridge_min); debug();    
        //fridge_max = 0; fridge_min = 0;
        fridge_avg = 0;
    }    
    
    
    //sprintf(s_debug, "Fridge Temp/ADC/Volt: %d/%Lu/%03.2w", Temps.fridge, adc_val, volt); debug();    
    
    if(Temps.fridge == FRIDGE_TO_WARM) { sprintf(s_debug, "Fridge to warm!!"); debug(); }
    if(Temps.fridge == INVALID_SENSOR_VOLTAGE) { sprintf(s_debug, "Fridge INVALID_SENSOR_VOLTAGE!!"); debug(); }
            
    
    
    //sprintf(s_debug, "Freezer Temp/ADC/Volt: %d/%Lu/%03.2w", Temps.freezer, adc_val, volt); debug();       

    if(Temps.freezer == FREEZER_TO_WARM) { sprintf(s_debug, "Freezer to warm!!"); debug(); }
    if(Temps.freezer == INVALID_SENSOR_VOLTAGE) { sprintf(s_debug, "Freezer INVALID_SENSOR_VOLTAGE!!"); debug(); }
    
        
    sprintf(s_debug, "------------------------------------------"); debug();

    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    
    DisableSensors;
    
    delay_ms(3000);
}

/**
* Read the pot's value, this is only used for alarm and turning on/off a motor
* We need to pull port down after reading it, because the pot's (via 7K resistor) fine-tunes the sensors in the circuit, when 
* not reading them via the 1K resistor
*/
void readPots(void)
{
    unsigned int16 adc_val_fridge_pot, adc_val_freezer_pot;    
    
    // tris also set pulldown's to output
    set_tris_a(TRISA_SAMPLE_POTS); 
    set_tris_b(TRISB_SAMPLE_POTS); 
    delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
        
    EnableSensors; // we need to power the pots via transistor (and R6 1K) to read them, just like the sensors...
    output_low(POT_FRIDGE_PULLDOWN); // and pull down voltage divider resistors
    output_low(POT_FREEZER_PULLDOWN);   
    
    delay_ms(1);

    adc_val_fridge_pot = getAvgADC(ADC_POT_FRIDGE, 30);
    output_float(POT_FRIDGE_PULLDOWN);
    delay_ms(1);
    
    adc_val_freezer_pot = getAvgADC(ADC_POT_FREEZER, 30);
    output_float(POT_FREEZER_PULLDOWN);   
    
    //pot_freezer = FridgeLookup[1/ adc_val_freezer_pot / 8];
    //259 - 752
    if(adc_val_freezer_pot > 870L) Temps.pot_freezer = POT_OFF;
    else if(adc_val_freezer_pot > 650L) Temps.pot_freezer = -18;
    else if(adc_val_freezer_pot > 570L) Temps.pot_freezer = -19;
    
    else if(adc_val_freezer_pot > 450L) Temps.pot_freezer = -21;
    else if(adc_val_freezer_pot > 638L) Temps.pot_freezer = -25; //OK
    
    else if(adc_val_freezer_pot > 250L) Temps.pot_freezer = -22;
    else if(adc_val_freezer_pot > 200L) Temps.pot_freezer = -23;
    else if(adc_val_freezer_pot > 300L) Temps.pot_freezer = -24; // TODO: lower to 200...
        
    // 238 - 674
    if(adc_val_fridge_pot > 800L)      Temps.pot_fridge = POT_OFF;
    else if(adc_val_fridge_pot > 600L) Temps.pot_fridge = 6;
    else if(adc_val_fridge_pot > 510L) Temps.pot_fridge = 5;
    else if(adc_val_fridge_pot > 410L) Temps.pot_fridge = 4;
    else if(adc_val_fridge_pot > 340L) Temps.pot_fridge = 3;        
    else if(adc_val_fridge_pot > 310L) Temps.pot_fridge = 2;
    else
        Temps.pot_fridge = 1;
                                                        
    // tris also set pulldown's to input, so we don't need to set them float.
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    // not needed, many cycles before tris is used again  //delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
    
 //   sprintf(s_debug, "POT Fridge  Temp/ADC %d/%Lu", Temps.pot_fridge, adc_val_fridge_pot); debug();       
 //   sprintf(s_debug, "POT Freezer Temp/ADC %d/%Lu", Temps.pot_freezer, adc_val_freezer_pot); debug();   
    sprintf(s_debug, "Transistor Q5: %d", input(TRANSISTOR_Q5C)); debug();
    sprintf(s_debug, "------------------------------------------"); debug();
            
    DisableSensors;
    
}

/**
* TODO: some magic math or lookup table...
* approx 0.5 V between 
*/
signed int8 SensorADToDegrees(int16 ad_val, int1 is_fridge)
{
    signed int8 temp;
    //unsigned int8 offset;
    //int16 warmest;//, n;
    //if(!freezer_running)
        //
        
    if(!is_fridge)
    {
        /************** BRUK ALLTID KALDESTE SAMPLE  ****************/
        
        
/*
     m off, m on
27 = ,430 
26 = 475,440
25 = 495-510,450
24 = 520,460    
23 = 540,
22 = 
21 = 
20 = 
19 = 
18 = 
17 = 
*/        
                    
        
        sprintf(s_debug, "----FREEZE ad_val: %Lu ----", ad_val); debug();      


        if(ad_val > 999 ) temp = FREEZER_TO_WARM; // above -17 degrees
        
        else if (ad_val >= 999 ) temp = -17;
        else if (ad_val >= 999 ) temp = -18;
        else if (ad_val >= 999) temp = -19; 
        else if (ad_val >= 999) temp = -20;  
        else if (ad_val >= 999) temp = -21;  
        else if (ad_val >= 999) temp = -22; 
        else if (ad_val >= 999) temp = -23; // 
        else if (ad_val >= 460) temp = -24; // ok
        else if (ad_val >= 450) temp = -25; // ok
        else if (ad_val >= 440) temp = -26; // ok
        else if (ad_val >= 430) temp = -27; // ok
        else                     temp = FREEZER_TO_COLD; 
/*
        offset=25; //26 lavere så synker temp
        //warmest=700; //-17 // høyere så synker temp
        warmest=890; //
        
        for(x=-17; x>=-27; x--)
        {
            if(ad_val > (warmest+20L)) 
            {
                temp = FREEZER_TO_WARM;
                break;
            }
                
            if(ad_val > warmest) 
            { 
                temp=x;
                break;
            } 
               
            warmest -= offset;
        }
        if(temp == 0) 
        {
            if(ad_val < warmest) temp = FREEZER_TO_COLD;
        }    
*/
        //sprintf(s_debug, "----temp: %d----", temp); debug();    
        sprintf(s_debug, "--------- FRYS STOPP ------"); debug();    
    }    
    else
    {    
                
        /************** BRUK ALLTID KALDESTE SAMPLE  ****************/
        
        sprintf(s_debug, "--------- KJØL START ------"); debug();    
/*
 m off, m on
8= , > 705
7= , 695
6= , 690
5= , 680
4= , 670
3= 
2= 
1= ,
0= ,
-1= 
-2=
-3= , 
*/

        //val = ad_val;

        sprintf(s_debug, "----KJØL ad_val: %Lu----", ad_val); debug();            
        
        if(ad_val > 700 ) temp = FRIDGE_TO_WARM; // above 680 (5 degrees)
        else if (ad_val >= 680 ) temp = 5; // ok
        else if (ad_val >= 670) temp = 4; // ok
        else if (ad_val >= 660) temp = 3;  // 
        else if (ad_val >= 650) temp = 2; //  
        else if (ad_val >= 640) temp = 1; 
        else if (ad_val >= 630) temp = 0; // 
        else if (ad_val >= 620) temp = -1; // 
        else                     temp = FRIDGE_TO_COLD; //      maybe used for "frost alarm" in fridge...
        
        sprintf(s_debug, "--------- KJØL STOPP ------"); debug();            
        // BRUK ALLTID KALDESTE SAMPLE
    }

        
   //if(!is_fridge && ad_val > 820) temp = TO_WARM; // ca -15 
   //if(is_fridge && ad_val > 645) 
    
    //sprintf(s_debug, "Volt: %Lu-%03.2w", volt, volt); debug();   
    
    return temp;
    
    //return TempLookup2[volt *1.5 / 16];    
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



void debug(void)
{
  //using SW UART we can't interrupt while writing...
  disable_interrupts(GLOBAL);
  printf("%s\r\n", s_debug);
  delay_ms(5);
  enable_interrupts(GLOBAL);
    
}    