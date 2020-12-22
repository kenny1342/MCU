#include <16F876.H>
#device ADC=10

#case

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef YES
#define YES 1
#endif

#ifndef NO
#define NO 0
#endif

#ifndef HIGH
#define HIGH 1
#endif

#ifndef LOW
#define LOW 0
#endif

#ifndef ON
#define ON 1
#endif

#ifndef OFF
#define OFF 0
#endif

#ifndef UP
#define UP 1
#endif

#ifndef DOWN
#define DOWN 0
#endif

#ifndef UCHAR
#define UCHAR char
#endif

#ifndef UINT
#define UINT long
#endif

#ifndef BIT
#define BIT short
#endif

#ifndef SCHAR
#define SCHAR signed int
#endif

#ifndef SINT
#define SINT signed long
#endif

#ifndef FLOAT
#define FLOAT float
#endif


#fuses XT, NOPROTECT, NOPUT, NOWDT, NOBROWNOUT, NOLVP, NOCPD, NOWRT

// ADC
#define VDD                 5.00

// LCD STUFF
#define MENU_DEC_SWITCH        PIN_C0
#define SELECT_INC_SWITCH      PIN_C1
#define RANGE_SHUNT            PIN_C2
#define SEL0                   PIN_B2
#define SEL1                   PIN_B4


// KENNY

#define CMD_NUL     0

#define LED_1   PIN_B5
#define LED_2   PIN_B4

#define hi(x)  (*(&x+1))

#use delay ( clock=4000000 )
#use standard_io ( A )
#use standard_io ( B )
#use standard_io ( C )

void SetTime ( void );
void CheckSample ( void );

/*******************************************************************/



#int_rtcc
void TimerInterrupt ( void )      // 32.768mS tic, ~30 interrupts per second
    {
    set_rtcc ( 4 );     // restart at adjusted value for 1-second accuracy
    }

//*****************************************************************************

void main ( void )
{
    //char recChar;
    delay_ms ( 200 );           // wait enough time after VDD rise
    output_low ( LED_2 );

    delay_ms ( 3000 );

    // SETUP
    setup_adc_ports ( RA0_ANALOG );     // these three statements set up the ADC
    setup_adc ( ADC_CLOCK_INTERNAL );   // clock source
    set_adc_channel ( 0 );              // select channel
    enable_interrupts ( INT_RTCC );     // turn on timer interrupt
    enable_interrupts ( GLOBAL );       // enable interrupts

    // INITIALIZE VARIABLES
    output_float ( LED_1 );

    while ( TRUE )              // do forever
        {
        CheckSample();          // check if it's time to sample and store ADC
        }
}

//****************************************************************************


void CheckSample ( void )
    {
    long ieeVal;

            ieeVal = read_adc();
    }

