#case
//#device HIGH_INTS=TRUE
#device adc=10 *=16 


#FUSES WDT                 	    //Watch Dog Timer enabled in 
#FUSES WDT4096                	// WDT base is 4ms on 18f1320, wdt timeout = 4*4096=16384ms = 16,3 sec
#FUSES INTRC_IO              	//Internal RC Osc, no CLKOUT
#FUSES NOFCMEN               	//Fail-safe clock monitor disabled
#FUSES BROWNOUT              	//Reset when brownout detected
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


#use delay(clock=4000000, RESTART_WDT)
#use rs232(baud=9600,parity=N,xmit=PIN_A6,rcv=PIN_A1,bits=8,INVERT, RESTART_WDT)


#use fast_io(A)
#use fast_io(B)

#BYTE PORTB=0xF81
//#BIT  B2bit=PORTB.2

#priority TIMER0


/************************ FLAGS USED UNDER DEVELOPMENT PHASE ************************/
//#define NO_RELAY // uncomment for not letting motors start up
#define DEBUG  // level 1
#define DEBUG_ALARMS
//#define DEBUG2 // level 2, for rare logging


/****** Values written to EEPROM when Flashed *****/
#define SW_MAJOR_V (int8) 01                 
#define SW_MINOR_V (int8) 16                
#define HW_MAJOR_V (int8) 01
#define HW_MINOR_V (int8) 06


/******* ADC Ref. Voltage Scale Factor, used in ADC 2 Temperature function **********
Example: Scalefactor for measure VDD (5V):
5.000 divide this number by 1023 (max output of 10 bit A/D) to obtain the ScaleFactor. 
5000000/1023 = 4888 
4900000/1023 = 4789
Calc millivolt from factor with: Factor*1023/1000
The reference in use is 5V so for the 10bit ADC each ADC bit is worth 5.0/1023 = 4.88mV
- temp increase when higher VDD_SF (+10 = ~+0,5C
******************************************************************************/
#define VDD_SF 4850 // 4.961V (240V, normal operation) prev: v1.14:4858 - org når alt virka ok 4868 4.979V 
//#define VDD_SF 4680 // 4.787V (ICD2)
//#define VDD_SF 4789 // 4.9V (240V, normal operation)
//#define VDD_SF 4888 // 5.0V

/************************ DIV OPERATIONAL SETTINGS AND TUNING *****************************/
#define SAMPLE_COUNT     3 // number of samples for avg reading of temps (ISR TIMER1 sets interval)
#define SAMPLE_COUNT_POT 4 // number of samples for avg reading of POTs (ISR TIMER1 sets interval)
#define SAMPLE_INTERVAL_COUNT 6   // required nr of equal samples with SAMPLE_INTERVAL_SECS secs between, before any actions 
#define SAMPLE_INTERVAL_SECS  15  // seconds between each long time samples

/******* Thresholds for alarms + SuperCool/DeepFreeze modes *******/
#define FREEZER_MAX_TEMP -15      // for flagging "freezer_to_warm"
#define FREEZER_MIN_TEMP -28      // for flagging "freezer_to_cold"
//replaced by calc from POT #define FRIDGE_MAX_TEMP 7         // for flagging "fridge_to_warm"
#define FRIDGE_MIN_TEMP 0         // for flagging "fridge_to_cold"


#define M_SUPERCOOL_TEMP     0    // when button pressed, we try to force temp down to this
#define M_FASTFREEZE_TEMP   -26   // when button pressed, we try to force temp down to this
#define M_FASTFREEZE_MIN    120   // Minutes FastFreeze should run after reaching M_FASTFREEZE_TEMP.

/******* Used for controlling motors *******/
#define WAIT_MIN_AFTER_LONG_RUN 30 // minutes to wait before starting motors after too long run

#define FREEZER_DEGREES_UNDER 3    // degrees diff between pot and real temp before stopping
#define FREEZER_DEGREES_OVER  2    // degrees diff between pot and real temp before starting
#define FREEZER_RUN_TEMP_OFFSET -5 // pga sensor plassert i grillene (lavere stopper tidligere)
#define FREEZER_STOP_TEMP_OFFSET -7 //-6 avg når grillen tempereres (lavere starter senere)

//A safe tempreture for a fridge to be at is 3C, plus/minus 2 degrees.
#define FRIDGE_DEGREES_UNDER 2    // real temp degrees under pot before stopping
#define FRIDGE_DEGREES_OVER  1    // real temp degrees over pot before starting
#define FRIDGE_RUN_TEMP_OFFSET -14  // (lavere stopper tidligere)
#define FRIDGE_STOP_TEMP_OFFSET -8 // (lavere starter senere)

/******* Motor runtimes *******/
#define FREEZER_MAX_RUN_MIN  240  //(4 hours) never run longer than X mins ( *2 if supercool-mode)
#define FREEZER_MIN_RUN_MIN  15   // when started, never run less than X mins
#define FREEZER_MIN_STOP_MIN 10   // when stopped, always stop for X mins

#define FRIDGE_MAX_RUN_MIN  240   //(4 hours) never run longer than X mins ( *2 if deepfreeze-mode)
#define FRIDGE_MIN_RUN_MIN  15    // when started, never run less than X mins
#define FRIDGE_MIN_STOP_MIN 10     // when stopped, always stop for X mins



/******************************** PIN/ADC DEFINES ********************************/
// *LEDIG* 
// B3 p18 dig
        
/*** SKAL VÆRE OK ***/
#define ADC_FRIDGE     5 // p9 (B1/AN5) -> CN2-4
#define ADC_FREEZER      4 // p8 (B0/AN4) -> CN2-1 

#define ADC_POT_FREEZER 1 // p2 (A1/AN1)
#define ADC_POT_FRIDGE  6 // p10 (B4/AN6)

#define POT_FRIDGE_PULLDOWN   PIN_A0 // p1 setting low when ad read the pots
//#define SENSOR_COM_7K  PIN_B6 // p12 via R9 til sensor-com (pin 2+3 på CN2)
#define POT_FREEZER_PULLDOWN  PIN_B6 // p12 (was SENSOR_COM_7K R9 til sensor-com, pin 2+3 på CN2)

#define SENSOR_COM      PIN_B7 // p13 til base på transistor som mater +5 til sensor-com (pin 2+3 på CN2)

// TODO: finn ut hva som trigger Q5....
#define TRANSISTOR_Q5C  PIN_A5 //p4 MCLR, en transistor som tydligvis kjører pinne high/low av en eller annen grunn....?

// TODO:
#define VOLTAGE         PIN_A4 //p3 , denne har konstant +5..... og noe mer, SJEKK UT DETTE!!!

#define POT_FRIDGE_R4   PIN_B4 //p10
#define POT_FREEZE_R5   PIN_A1 // p2

#define SW_FRIDGE  PIN_B5    // p17 SW2 "super cool", kjøl, DL2 orange til ferdig?
#define SW_FREEZE  PIN_B2    // SW3 nedfrysning, DL1 orange til ferdig

#define LED_COM PIN_A7 // p16, common for LEDs
#define LED_DL3 PIN_B2 // p17 red led, alarm fryser
#define LED_DL1 PIN_B3 // p9 
#define LED_DL2 PIN_B5 // p11 "super cool" kjøl, orange en stund mens kjøl tvangskjøres?

#define OUT_RL2 PIN_A2 // p6, rele frys
#define OUT_RL1 PIN_A3  // p7, rele kjøl


/******************************** TRIS SETTINGS ********************************/

#define TRISA_NORMAL         0b00110011 // A4 +5, A1 is a POT, read by ADC. A0=POT_FRIDGE_PULLDOWN, needs to float (input) when not sampling. (same with B6)
#define TRISA_SAMPLE_SENSORS 0b00110011 // A4 +5, A1 is a POT, read by ADC. A0=POT_FRIDGE_PULLDOWN, output low when sampling. (same with B6)
#define TRISA_SAMPLE_POTS    0b00110010 // A4 +5, A0 pulldown output

#define TRISB_NORMAL          0b01010011  //0b01010011 B7-B0: B7=SENSOR-COM, B6=PULLDOWN, B5=DL2+SW3, B4=ANA-POT, B3=DL1,B2=DL3+SW2, B1-B0=ANA-SENSORS
#define TRISB_SAMPLE_SENSORS  0b01010011 
#define TRISB_SAMPLE_SWITCHES 0b01110111
#define TRISB_SAMPLE_POTS     0b00010011 // B6 pulldown output

/******************************** GENERAL CONSTANTS ********************************/
#define ON 1
#define OFF 0
#define YES 1
#define NO 0

#define NOT_READY -101
#define POT_OFF -100
#define POT_MAX -127
    

/******************************** EEPROM ADDRESSES ********************************/
#define MEM_ADDR_RESTART    0xFF

#rom int8 0xf00000 = {SW_MAJOR_V, SW_MINOR_V, HW_MAJOR_V, HW_MINOR_V}
#rom int8 0xf00010 = {FREEZER_RUN_TEMP_OFFSET}
#rom int8 0xf00020 = {FREEZER_STOP_TEMP_OFFSET}
#rom int8 0xf00030 = {FRIDGE_RUN_TEMP_OFFSET}
#rom int8 0xf00040 = {FRIDGE_STOP_TEMP_OFFSET}
#rom      0xf00050 = {VDD_SF}

/******************************** Structs & Enums ********************************/
typedef struct
{
    //int8 COM; 
    int8 DL1;
    int8 DL2;
    int8 DL3;
    int1 DL1_Color;
    int1 DL2_Color;
    int1 COM_Last;
    
} LEDStruct;

 typedef struct 
 {
     int1 alarm_active;
     int1 reset_done;
     int1 fridge_temp;
     int1 freezer_temp;     
     int1 fridge_sensor_fault;
     int1 freezer_sensor_fault;
     int1 fridge_run_to_long;
     int1 freezer_run_to_long;     
     int16 auto_reset_timeout_mins;
     //int8 first_alarm[20];
     //int8 last_alarm[20];
     //int8 tmp[20];
 } AlarmStruct;

 typedef struct 
 {
     signed int8 fridge_real;
     signed int8 fridge_calc;
     signed int8 fridge_run;
     signed int8 pot_fridge;
     signed int8 freezer_real;
     signed int8 freezer_calc;
     signed int8 freezer_run;
     signed int8 pot_freezer;
     int1 fridge_to_warm;
     int1 fridge_to_cold;
     int1 freezer_to_warm;
     int1 freezer_to_cold;
 } TempsStruct;

 typedef struct 
 {
     //int1 sample_ready;
     int8 index;
     int8 tries;
     signed int8 samples[SAMPLE_COUNT];
     int8 last_index;
     int8 last_tries;
     signed int8 last_samples[6]; // last 6 samples need to be equal, before we call for actions
     int1 do_actions;
     int1 do_final_sample;
     int16 avg_adc_value;
 } SampleStruct;

 typedef struct 
 {
     int8 index;     
     int16 samples[SAMPLE_COUNT_POT];     
     int16 avg_adc_value;
 } SampleStructPot;

typedef struct
{
    int1 run_fridge;
    int1 run_freezer;
    int1 fridge_running;
    int1 freezer_running;
    int1 mode_supercool;
    int1 mode_fastfreeze;
    int1 fastfreeze_reached_temp;
    int1 sampling;
    int1 sampling_switches;
    int8 fridge_run_mins;
    int16 fridge_total_run_mins;
    int8 freezer_run_mins;
    int16 freezer_total_run_mins;
    int8 fridge_stopped_mins;
    int8 freezer_stopped_mins;
    int8 fastfreeze_run_mins;
} StatusStruct;
    
typedef struct
{
    unsigned int8 freezer_degrees_under; // really a constant, but can change in code in some circumstanses
    unsigned int8 fridge_degrees_under;  // really a constant, but can change in code in some circumstanses
    signed int8 freezer_run_temp_offset;
    signed int8 freezer_stop_temp_offset;
    signed int8 fridge_run_temp_offset;    
    signed int8 fridge_stop_temp_offset;    
} ConfigStruct;

/**** MACROS ***/
#ifdef NO_RELAY // always off
    #define SET_RELAY1(STATUS) (output_bit(OUT_RL1, 1))
    #define SET_RELAY2(STATUS) (output_bit(OUT_RL2, 1))
#else
    #define SET_RELAY1(STATUS) (output_bit(OUT_RL1, !STATUS))
    #define SET_RELAY2(STATUS) (output_bit(OUT_RL2, !STATUS))
#endif

#define SET_LED(LED,STATUS) (output_bit(LED, !STATUS))
#define Start_Fridge_Motor (SET_RELAY1(ON), Status.fridge_running=YES, started2++)
#define Stop_Fridge_Motor (SET_RELAY1(OFF), Status.fridge_running=NO)
#define Start_Freezer_Motor (SET_RELAY2(ON), Status.freezer_running=YES, started1++)
#define Stop_Freezer_Motor (SET_RELAY2(OFF), Status.freezer_running=NO)

#define EnableSensors       (output_low(SENSOR_COM), Status.sampling=YES) // Turn transistor ON to power sensors 
#define DisableSensors      (output_high(SENSOR_COM), Status.sampling=NO) // Turn transistor OFF to turn off sensors

enum LED_MODE {LED_OFF, LED_ON, LED_BLINK=3, LED_BLINK_FAST=4};
enum LED_COLOR {LED_GREEN, LED_AMBER};

//typedef enum {red, green=2,blue}colors;

/******************************** RAM VARIABLES ********************************/

int1 do_read_temp, do_LED_updates, blink_toggle, blink_toggle_fast, print_debug;

unsigned int8 ISR_counter1, sample_timer_sec, debug_timer_sec;
unsigned int16 ISR_counter0, ISR_counter0_2;

static unsigned int16 SW_fridge_time, SW_freezer_time;
static unsigned int8 started1, started2;
static unsigned int8 poweredup_mins;
unsigned int8 x; // general loop counter etc
static unsigned int16 adc_val;
static volatile signed int8 min_val, max_val;
unsigned int8 prev_restart_reason, curr_restart_reason;
//static signed int8 sample_run_f, sample_run_k, sample_stop_f, sample_stop_k;
//unsigned int8 *ptrstr;

ConfigStruct config;
TempsStruct Temps;
LEDStruct LEDs;
AlarmStruct Alarms;
SampleStruct SampleFridge;
SampleStructPot SampleFridgePot;
SampleStruct SampleFreezer;
SampleStructPot SampleFreezerPot;
StatusStruct Status;

//signed int8 CONST FridgeLookup[8] = {POT_OFF,5,4,3,2,1,0,POT_MAX};

//a lookup table that is not declared static is almost certainly a mistake
//const int16 FridgeLookup[6] = {700,600,500,400,300,200};


/******************************** FUNCTION PROTOS ********************************/

void main(void);
#separate
void initialize_hardware(void);
#separate
void initialize_software(void);
#separate
void doDecision_Freezer(void);
#separate
void doDecision_Fridge(void);
void doActions(void);
#separate
void setLEDs(void);
void updateAlarms(void);
#separate
void debug2(unsigned int8 c);
#separate
void STREAM_DEBUG(unsigned int8 c);

//#include <stdlibm.h> //malloc
#include <string.h>


#include "ISR.c"
#include "common.h"
#include "common.c"
#include "Sensors.h"
#include "Sensors.c"
