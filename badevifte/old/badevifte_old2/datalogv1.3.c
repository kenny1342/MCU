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


// KENNY
//#define RX_IN               PIN_A4 //pinne 6 RA4
#define LED_1               PIN_C7 //pinne 18 RC7
#define CMD_NUL     0

#define MINIMUM_INTERVAL   1
#define STATE_START        0
#define STATE_STOP         1
#define STATE_STATUS       2
#define STATE_RESET        3
#define STATE_RANGE        4
#define STATE_INTERVAL     5
#define STATE_VIEW         6
#define STATE_DUMP         7
#define MAX_MENU_STATE     7

#define EOF             0x00
#define COMMA           ','
#define DOLLAR          '$'
#define CR              13
#define SPACE           ' '
#define NULL            0
#define RX_BUFFER_SIZE  70

#define SW_VERSION      151

#define hi(x)  (*(&x+1))

#use delay ( clock=4000000 )
#use standard_io ( A )
#use standard_io ( B )
#use standard_io ( C )
//#use rs232 ( baud=9600, xmit=PIN_B3 )
//#use rs232 ( baud=9600, xmit=PIN_B3, RCV=PIN_A4, FLOAT_HIGH )
//#use i2c ( master, scl=EEPROM_SCL, sda=EEPROM_SDA, address=0xa0)


void PrintMenu ( void );       // protos
void init_ext_eeprom ( void );
void write_ext_eeprom ( long int lngAddress, BYTE intData );
BYTE read_ext_eeprom ( long int lngAddress );
void SetTime ( void );
void CheckSample ( void );
void CheckSwitches ( void );
char GetEchoNumChar ( void );
void LCD_Init ( void );
void LCD_SetPosition ( unsigned int cX );
void LCD_PutChar ( unsigned int cX );
void LCD_PutCmd ( unsigned int cX );
void LCD_PulseEnable ( void );
void LCD_SetData ( unsigned int cX );
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
static char cOperationMode; // AUTO or MANUAL
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
   if ( LCD == YES)
   {
    LCD_SetPosition ( LINE_1 + 0 );
    printf ( LCD_PutChar, "DATA LOGGER v1.7" );
    LCD_SetPosition ( LINE_2+ 0 );
    printf ( LCD_PutChar, "Created by Kenny" );
   }
   else //rs232
   {
    printf ( "\r\nDATA LOGGER v1.7" );
    printf ( "\r\nCreated by Kenny" );
    printf ( "\r\n" );
   }   
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
    LCD_PutCmd ( CLEAR_DISP );    LCD_Init();

    LCD_SetPosition ( LINE_1 + 0 );
    printf ( LCD_PutChar, "Temp and Humidity");
    LCD_SetPosition ( LINE_2 + 1 );
    printf ( LCD_PutChar, "S/W 17.09.2007" );

    delay_ms ( 3000 );
    LCD_PutCmd ( CLEAR_DISP );

    // SETUP
    output_float ( RX_IN );
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
    cMenuState = ( cLogging == YES ) ? STATE_STOP : STATE_START;  // set first menu

    output_float ( LED_1 );

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
    // ACTIVITY INDICATOR
    LCD_SetPosition ( LINE_1 + 15 );
    if ( cLogging == NO )       // if not logging at this time
        {
        printf ( LCD_PutChar, " " );    // blank symbol for activity indicator
        }
    else                    // if presently logging
        {
        if ( cLoggingIndicatorFlag == ON )  // turned on once per second by interrupt
            {
            cToggleFlag ^= 1;              // toggle the activity indicator symbol
            if ( cToggleFlag == 1 )
                {
                printf ( LCD_PutChar, "%c", 255 );  // 255 symbol
                }
            else
                {
                printf ( LCD_PutChar, " " );        // blank symbol
                }
            cLoggingIndicatorFlag = OFF;
            }
        }

    // PRINT LOWER LINE OF MENU
    LCD_SetPosition ( LINE_2 + 0 );
    switch ( cMenuState )
        {
        case STATE_START:
            {
            if(cOperationMode=="MANUAL")
            {
               printf ( LCD_PutChar, "Start       Auto" );
            }
            //else
            //   printf ( LCD_PutChar, "Start       Manual" );
            break;
            }
        case STATE_STOP:
            {
            if(cOperationMode=="MANUAL")
            {
               printf ( LCD_PutChar, "Stop       Auto" );
            }
            break;
            }
        case STATE_AUTO:
            {
            if(cOperationMode=="MANUAL")
            {
               printf ( LCD_PutChar, "Start       Auto" );
            }
            else
            {
               printf ( LCD_PutChar, "----    Manual" );
            }
            }
            break;
         case STATE_MANUAL:
            {
            if(cOperationMode=="MANUAL")
            {
               printf ( LCD_PutChar, "Start       Auto" );
            }
            else
            {
               printf ( LCD_PutChar, "----    Manual" );
            }

            break;
            }
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
                    cMenuState = STATE_STOP;    // menu displays "STOP"
                    break;
                    }
                }
            }
        case ( STATE_STOP ): // STOPP VIFTE
            {
            if ( cSelectFlag == ON )    // if switch is pressed
                {
                cSelectFlag = OFF;         // turn flag off
                cMenuState = STATE_START;    // menu displays "START"
                break;
                }
            }
         case ( STATE_AUTO ):
            {
            if ( cSelectFlag == ON )    // if switch is pressed
                {
                cSelectFlag = OFF;         // turn flag off
                cMenuState = STATE_MANUAL;    // menu displays "MANUAL"
                break;
                }
            }
         case ( STATE_MANUAL ):
            {
            if ( cSelectFlag == ON )    // if switch is pressed
                {
                cSelectFlag = OFF;         // turn flag off
                cMenuState = STATE_AUTO;    // menu displays "AUTO"
                break;
                }
            }
        }
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
