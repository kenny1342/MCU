#case
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
//#use standard_io(A)
#use fast_io(B)

#BYTE PORTB=0xF81
//#BIT  B2bit=PORTB.2

#priority TIMER0

/****** Values written to EEPROM when Flashed *****/
// for version info page
#define SW_MAJOR_V (int8) 01                  // using month while under development...
#define SW_MINOR_V (int8) 02                 // using date while under development...20-22 juni 2010 in hotel room in Stockholm :)
#define HW_MAJOR_V (int8) 01
#define HW_MINOR_V (int8) 06

// Ref. voltage used in math in ADC stuff
//#define VDD 5.0
//#define VDD 4.65 // (ICD2)
#define VDD 4.85 //4.9 (240V, normal operation w/power supply)


/****** LEDIG ***********/
// B3 p18 dig

//#define NO_RELAY
#define DEBUG  // level 1
//#define DEBUG2 // level 2, for rare logging
        
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

//#define LED_COM PIN_A1 // p2, common for LEDs
#define LED_COM PIN_A7 // p16, common for LEDs
#define LED_DL3 PIN_B2 // p17 red led, alarm fryser
#define LED_DL1 PIN_B3 // p9 
#define LED_DL2 PIN_B5 // p11 "super cool" kjøl, orange en stund mens kjøl tvangskjøres?


// PINS OK, MEN MÅ SJEKKES I SKAPET HVILKEN MOTOR ER HVA
#define OUT_RL2 PIN_A2 // p6, rele frys?
#define OUT_RL1 PIN_A3  // p7, rele kjøl?


/******* NOT USED (int32 stuff)!!!!! Voltage Scale Factor, used in ADC 2 Temperature function **********
Example: Scalefactor for measure VDD (5V):
5.000 divide this number by 1023 (max output of 10 bit A/D) to obtain the ScaleFactor. 
5000000/1023 = 4888 
******************************************************************************/
//#define VDD_SF 4789 // 4.9V (240V, normal operation)
//#define VDD_SF 4888 // 5.0V
//#define VDD_SF 4594 // 4.7V (ICD2)


#define TRISA_NORMAL         0b00110011 // A4 +5, A1 is a POT, read by ADC. A0=POT_FRIDGE_PULLDOWN, needs to float (input) when not sampling. (same with B6)
#define TRISA_SAMPLE_SENSORS 0b00110011 // A4 +5, A1 is a POT, read by ADC. A0=POT_FRIDGE_PULLDOWN, output low when sampling. (same with B6)
#define TRISA_SAMPLE_POTS    0b00110010 // A4 +5, A0 pulldown output

#define TRISB_NORMAL          0b01010011  //0b01010011 B7-B0: B7=SENSOR-COM, B6=PULLDOWN, B5=DL2+SW3, B4=ANA-POT, B3=DL1,B2=DL3+SW2, B1-B0=ANA-SENSORS
#define TRISB_SAMPLE_SENSORS  0b01010011 
#define TRISB_SAMPLE_SWITCHES 0b01110111
#define TRISB_SAMPLE_POTS     0b00010011 // B6 pulldown output

#define ON 1
#define OFF 0
#define YES 1
#define NO 0

#define SAMPLE_COUNT 3//20 // number of samples for avg reading of temps (ISR TIMER1 sets interval)

#define NOT_READY -101

#define POT_OFF -100
#define POT_MAX -127
    
// Temp thresholds for alarming (and motors to some degree I think...)
#define FREEZER_MAX_TEMP -17      // for flagging "freezer_to_warm"
#define FREEZER_MIN_TEMP -28      // for flagging "freezer_to_cold"
#define FRIDGE_MAX_TEMP 6         // for flagging "fridge_to_warm"
#define FRIDGE_MIN_TEMP -1        // for flagging "fridge_to_cold"

// Used for controlling motors
#define FREEZER_DEGREES_UNDER 3    // degrees diff between pot and real temp before stopping
#define FREEZER_DEGREES_OVER  2    // degrees diff between pot and real temp before starting
#define FRIDGE_DEGREES_UNDER 2    // real temp degrees under pot before stopping
#define FRIDGE_DEGREES_OVER  2    // real temp degrees over pot before starting

// Motor runtimes
#define FREEZER_MAX_RUN_MIN  240  //(4 hours) never run longer than X mins ( *2 if supercool-mode)
#define FREEZER_MIN_RUN_MIN  10   // when started, never run less than X mins
#define FREEZER_MIN_STOP_MIN 5   // when stopped, always stop for X mins

#define FRIDGE_MAX_RUN_MIN  240   //(4 hours) never run longer than X mins ( *2 if deepfreeze-mode)
#define FRIDGE_MIN_RUN_MIN  10    // when started, never run less than X mins
#define FRIDGE_MIN_STOP_MIN 7    // when stopped, always stop for X mins

#define M_SUPERCOOL_TEMP     0    // when button pressed, we try to force temp down to this
#define M_DEEPFREEZE_TEMP   -26   // when button pressed, we try to force temp down to this



#rom int8 0xf00000 = {SW_MAJOR_V, SW_MINOR_V, HW_MAJOR_V, HW_MINOR_V}

typedef struct
{
    int8 COM; 
    int8 DL1;
    int8 DL2;
    int8 DL3;
    
} LEDStruct;

 typedef struct 
 {
     int1 fridge_temp;
     int1 freezer_temp;
     int1 fridge_sensor_fault;
     int1 freezer_sensor_fault;
 } AlarmStruct;

 typedef struct 
 {
     signed int8 fridge;
     signed int8 pot_fridge;
     signed int8 freezer;
     signed int8 pot_freezer;
     int1 fridge_to_warm;
     int1 fridge_to_cold;
     int1 freezer_to_warm;
     int1 freezer_to_cold;
 } TempsStruct;

 typedef struct 
 {
     int1 sample_ready;
     int8 index;
     int8 tries;
     signed int8 samples[SAMPLE_COUNT];
 } SampleStruct;


typedef struct
{
    unsigned int8 freezer_degrees_under; // really a constant, but can change in code in some circumstanses
    unsigned int8 fridge_degrees_under;  // really a constant, but can change in code in some circumstanses
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
#define Start_Fridge_Motor (SET_RELAY1(ON), fridge_running=YES, started2++)
#define Stop_Fridge_Motor (SET_RELAY1(OFF), fridge_running=NO)
#define Start_Freezer_Motor (SET_RELAY2(ON), freezer_running=YES, started1++)
#define Stop_Freezer_Motor (SET_RELAY2(OFF), freezer_running=NO)

#define EnableSensors       (output_low(SENSOR_COM), sampling=YES) // Turn transistor ON to power sensors 
#define DisableSensors      (output_high(SENSOR_COM), sampling=NO) // Turn transistor OFF to turn off sensors

enum LED_MODE {LED_OFF=0, LED_ON=1, LED_BLINK=3};//, LED_GREEN=4, LED_AMBER=5};
#define LED_GREEN 0
#define LED_AMBER 1

int1 do_read_temp, do_actions, blink_toggle;
static volatile int1 run_fridge, run_freezer, fridge_running, freezer_running;
static volatile int1 mode_supercool, mode_deepfreeze;
int1 sampling;

unsigned int8 ISR_counter1;
unsigned int16 ISR_counter0, ISR_counter2;//, ISR_max_run_c;
unsigned int8 fridge_stopped_mins, freezer_stopped_mins;
unsigned int16 fridge_run_mins, freezer_run_mins;
//volatile unsigned int32 volt;

static unsigned int16 SW_fridge_time, SW_freezer_time;
static unsigned int8 started1, started2;
static unsigned int8 poweredup_secs;
static unsigned int16 adc_val;
signed int8 min_val, max_val, min_val_fridge, max_val_fridge;

ConfigStruct config;
TempsStruct Temps;
LEDStruct LEDs;
AlarmStruct Alarms;
SampleStruct SampleFridge, SampleFreezer;


unsigned int8 s_debug[50];

//signed int8 CONST FridgeLookup[8] = {POT_OFF,5,4,3,2,1,0,POT_MAX};

    
/********* PROTOS *****************************/

void main(void);
#separate
void initialize_hardware(void);
#separate
void initialize_software(void);
#separate
void doDecision(void);
void doActions(void);
//#separate
void readTempFreezer(void);
//#separate
void readTempFridge(void);
void updateAlarms(void);
signed int8 SensorADToDegrees(int16 ad_val, int1 is_fridge);
void readPots(void);
void updateStructs(void);
unsigned int16 getAvgADC(unsigned int8 channel, unsigned int8 samples);
//void PUT_SERIAL(unsigned int8 *str);
void debug(void);

