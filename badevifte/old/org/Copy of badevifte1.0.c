#include <16F876.H>

#include <kenny.h>
#include <lcd.inc>

#device ADC=10



#fuses XT, NOPROTECT, NOPUT, NOWDT, NOBROWNOUT, NOLVP, NOCPD, NOWRT

// ADC
#define VDD                 5.00

// INTERNAL EEPROM ASSIGNMENTS
#define SAMPLE_INTERVAL_HI 0
#define SAMPLE_INTERVAL_LO 1
#define SAMPLE_COUNT_HI    2
#define SAMPLE_COUNT_LO    3
#define LOGGING_STATE      4
#define RANGE              5

/*
// EXTERNAL EEPROM ASSIGNMENTS 128Kbit EEPROM is 16384 bytes
#define EEPROM_BYTE_SIZE   16384
#define EEPROM_SCL         PIN_B0
#define EEPROM_SDA         PIN_B1
*/

// LCD STUFF
#define LCD_D0  PIN_C3
#define LCD_D1  PIN_C4
#define LCD_D2  PIN_C5
#define LCD_D3  PIN_C6
#define LCD_EN  PIN_B5
#define LCD_RS  PIN_B6
#define LINE_1  0x00
#define LINE_2  0x40
#define CLEAR_DISP  0x01

#define MENU_DEC_SWITCH        PIN_C0
#define SELECT_INC_SWITCH      PIN_C1
#define RANGE_SHUNT            PIN_C2
#define SEL0                   PIN_B2
#define SEL1                   PIN_B4


//#define RX_IN               PIN_A4 //pinne 6 RA4
#define LED_1               PIN_C7 //pinne 18 RC7
#define CMD_NUL     0

#define MINIMUM_INTERVAL   1
#define STATE_START        0
#define STATE_STOP         1
#define STATE_AUTO         2
#define STATE_DELAYED      3
#define MAX_MENU_STATE     3

#define hi(x)  (*(&x+1))

#use delay ( clock=4000000 )
#use standard_io ( A )
#use standard_io ( B )
#use standard_io ( C )


void LCD_Init ( void );
void LCD_SetPosition ( unsigned int cX );
void LCD_PutChar ( unsigned int cX );
void LCD_PutCmd ( unsigned int cX );
void LCD_PulseEnable ( void );
void LCD_SetData ( unsigned int cX );
void PrintMenu ( void );       // protos
void SetTime ( void );
void CheckSample ( void );
void CheckSwitches ( void );
void StartFan ( void );
void StopFan ( void );
void DelayedFan ( void );
void AutoFan ( void );

char GetEchoNumChar ( void );


//void DisplayVolts ( long iAdcValue, char cLoc );
//float ScaleAdc ( long iValue );
//void SetRange ( BYTE cDisplay );



static long iIntervalCount, iIntervalTrigger, iSampleCount;
static char cLogging, cSampleFlag /*,cLedCount*/;
static char cLoggingIndicatorFlag, cAdcFlag, cToggleFlag;
static char cInterruptCount, cViewing;
static char cMenuState, cSelectFlag, cRange;
static char cMenuDecSwitchOn, cMenuSwitchCount;
static char cSelIncSwitchOn, cSelectSwitchCount;
static char cDelayedActive; // AUTO or MANUAL
//static char cTimeoutCounter, cError;
static char cRxErrorFlag,cSerialCmd;


/*******************************************************************/



#int_rtcc
void TimerInterrupt ( void )      // 32.768mS tic, ~30 interrupts per second
    {
    if ( cInterruptCount++ == 30 )      // if one second yet
        {
        cAdcFlag = ON;              // allow write to display
        cInterruptCount = 0;
        if ( cLogging == YES )
            {
            cLoggingIndicatorFlag = ON;    // time to toggle "running" indicator on display
            }
        if ( ( iIntervalCount++ == iIntervalTrigger - 1 ) && ( cLogging == YES ) )  // if sample time yet
                {
                cSampleFlag = ON;                       // signal time to sample
                iIntervalCount = 0;                     // start count over
                }
           }
    if ( input ( MENU_DEC_SWITCH ) == LOW )
        {
        if ( cMenuSwitchCount++ == 1 )     // debounce for 30mS, (was 2)
            {
            cMenuDecSwitchOn = YES;        // signal that switch was pressed
            cMenuSwitchCount = cViewing ? 252 : 240;       // set up for auto repeat (faster if viewing)
            }
        }
    else
        {
        cMenuSwitchCount = 0;             // switch up, restart
        }
    if ( input ( SELECT_INC_SWITCH ) == LOW )
        {
        if ( cSelectSwitchCount++ == 1 )  // debounce for 30mS (was 2)
            {
            cSelIncSwitchOn = YES;        // signal that switch was pressed
            cSelectSwitchCount = cViewing ? 252 : 240;       // set up for auto repeat (faster if viewing)
            }
        }
    else
        {
        cSelectSwitchCount = 0;             // switch is up, restart count
        }
    set_rtcc ( 4 );     // restart at adjusted value for 1-second accuracy
    }

//*****************************************************************************

void printVersionInfo ( LCD )
{
    LCD_SetPosition ( LINE_1 + 0 );
    printf ( LCD_PutChar, "BADEVIFTE 1.0" );
    LCD_SetPosition ( LINE_2+ 0 );
    printf ( LCD_PutChar, "Kenny 30.09.2007" );
}

void main ( void )
    {
    //char recChar;
    delay_ms ( 200 );           // wait enough time after VDD rise
    output_float ( RANGE_SHUNT );
    output_low ( LED_1 );

    LCD_Init();
    LCD_PutCmd ( CLEAR_DISP );

    printVersionInfo( YES );
    delay_ms ( 5000 );
    LCD_PutCmd ( CLEAR_DISP );
    LCD_Init();

    // SETUP
    setup_adc_ports ( RA0_ANALOG );     // these three statements set up the ADC
    setup_adc ( ADC_CLOCK_INTERNAL );   // clock source
    set_adc_channel ( 0 );              // select channel
    enable_interrupts ( INT_RTCC );     // turn on timer interrupt
    enable_interrupts ( GLOBAL );       // enable interrupts

    // INITIALIZE VARIABLES
    cSelectFlag = OFF;
    cToggleFlag = 0;
    cMenuDecSwitchOn = OFF;
    cSelIncSwitchOn = OFF;
    cMenuSwitchCount = 0;
    cSelectSwitchCount = 0;
    cMenuState = STATE_AUTO;  // set first menu
    AutoFan();

    //output_float ( LED_1 );

    while ( TRUE )              // do forever
        {

        PrintMenu();            // display screens and voltage
        CheckSwitches();        // check and do any switch activity
        CheckSample();          // check if it's time to sample and store ADC
        }
    }

//****************************************************************************

void PrintMenu ( void )
    {

    // PRINT LOWER LINE OF MENU
    LCD_SetPosition ( LINE_2 + 0 );
    switch ( cMenuState )
        {
        case STATE_START:
            {
               printf ( LCD_PutChar, "Stop       OK" );
            }
            break;
        case STATE_STOP:
            {
               printf ( LCD_PutChar, "Start       OK" );
            }
            break;
        case STATE_AUTO:
            {
               printf ( LCD_PutChar, "Auto       OK" );
            }
            break;
         case STATE_DELAYED:
            {
               printf ( LCD_PutChar, "Delayed       OK" );
            }
            break;
        }
    // DISPLAY VOLTS
    if ( cAdcFlag == ON )                    // if interrupt signalled an ADC reading
        {
        cAdcFlag = OFF;
        }
    }

void CheckSwitches ( void )
    {
//    char cX, cDigit, cDigitPointer, cDone;
//    long iX, iY, iVal, iPlace;

    // INCREMENT/DECREMENT
    if ( cMenuDecSwitchOn == YES )      // if interrupt caught the switch press
        {
        if ( cMenuState++ >= MAX_MENU_STATE )      // if at maximum
            {
            cMenuState = 0;            // roll
            }
        cMenuDecSwitchOn = NO;     // turn back off
        }
    if ( cSelIncSwitchOn == YES )      // if interrupt caught the switch press
        {
        cSelectFlag = ON;
        cSelIncSwitchOn = NO;     // turn back off
        }

    // PRINT MENU (upper line and sometimes overwrite lower line)
    switch ( cMenuState )
        {
        case ( STATE_START ): // START VIFTE
            {
            if ( cSelectFlag == ON )    // if switch is pressed
                {
                   cSelectFlag = OFF;         // turn flag off

                    StartFan();

                    cMenuState = STATE_STOP;    // menu displays "STOP"
                    break;
                    }
                }

        case ( STATE_STOP ): // STOPP VIFTE
            {
            if ( cSelectFlag == ON )    // if switch is pressed
                {
                cSelectFlag = OFF;         // turn flag off

                StopFan();

                cMenuState = STATE_STOP;    // menu displays "STOP"
                //stopp vifte
                //set auto modus
                break;
                }
            }
         case ( STATE_AUTO ):
            {
            if ( cSelectFlag == ON )    // if switch is pressed
                {
                cSelectFlag = OFF;         // turn flag off

                AutoFan();

                cMenuState = STATE_AUTO;    // menu displays "AUTO"

                break;
                }
            }
         case ( STATE_DELAYED ):
            {
            if ( cSelectFlag == ON )    // if switch is pressed
                {
                cSelectFlag = OFF;         // turn flag off
                if(cDelayedActive==YES)
                {
                   cMenuState = STATE_DELAYED;    // menu displays "DELAYED"
                }
                else //delayed er ferdig
                {
                   cMenuState = STATE_AUTO;    // menu displays "AUTO"
                   //set auto modus
                }
                // set delayed modus
                break;
                }
            }
        }
    }

void StartFan( void )
{
   cDelayedActive = NO;
}

void StopFan( void )
{
   cDelayedActive = NO;
}

void DelayedFan( void )
{
   cDelayedActive = YES;

}

void AutoFan( void )
{
   StopFan();
   cDelayedActive = NO;
}

void CheckSample ( void )
    {
    long ieeVal;

            ieeVal = read_adc();
    }






// LCD FUNCTIONS =================================

void LCD_Init ( void )
    {
    LCD_SetData ( 0x00 );
    delay_ms ( 200 );       // wait enough time after Vdd rise
    output_low ( LCD_RS );
    LCD_SetData ( 0x03 );   // init with specific nibbles to start 4-bit mode
    LCD_PulseEnable();
    LCD_PulseEnable();
    LCD_PulseEnable();
    LCD_SetData ( 0x02 );   // set 4-bit interface
    LCD_PulseEnable();      // send dual nibbles hereafter, MSN first
    LCD_PutCmd ( 0x2C );    // function set (all lines, 5x7 characters)
    LCD_PutCmd ( 0x0C );    // display ON, cursor off, no blink
    LCD_PutCmd ( 0x01 );    // clear display
    LCD_PutCmd ( 0x06 );    // entry mode set, increment
    }

void LCD_SetPosition ( unsigned int cX )
    {
    // this subroutine works specifically for 4-bit Port A
    LCD_SetData ( swap ( cX ) | 0x08 );
    LCD_PulseEnable();
    LCD_SetData ( swap ( cX ) );
    LCD_PulseEnable();
    }

void LCD_PutChar ( unsigned int cX )
    {
    // this subroutine works specifically for 4-bit Port A
    output_high ( LCD_RS );
    LCD_SetData ( swap ( cX ) );     // send high nibble
    LCD_PulseEnable();
    LCD_SetData ( swap ( cX ) );     // send low nibble
    LCD_PulseEnable();
    output_low ( LCD_RS );
    }

void LCD_PutCmd ( unsigned int cX )
    {
    // this subroutine works specifically for 4-bit Port A
    LCD_SetData ( swap ( cX ) );     // send high nibble
    LCD_PulseEnable();
    LCD_SetData ( swap ( cX ) );     // send low nibble
    LCD_PulseEnable();
    }

void LCD_PulseEnable ( void )
    {
    output_high ( LCD_EN );
    delay_us ( 10 );
    output_low ( LCD_EN );
    delay_ms ( 5 );
    }

void LCD_SetData ( unsigned int cX )
    {
    output_bit ( LCD_D0, cX & 0x01 );
    output_bit ( LCD_D1, cX & 0x02 );
    output_bit ( LCD_D2, cX & 0x04 );
    output_bit ( LCD_D3, cX & 0x08 );
    }
