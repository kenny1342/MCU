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

#define PERCENT             'p'
#define CELCIUS             'C'


#define MENU_SWITCH        PIN_C1
#define SELECT_SWITCH      PIN_C2

//#define RX_IN               PIN_A4 //pinne 6 RA4
#define LED_1               PIN_B3 //pinne 24
#define LED_2               PIN_B4 //pinne 25
#define FAN_1               PIN_C0

#define CMD_NUL     0

//#define MINIMUM_INTERVAL   1
#define STATE_START        0
#define STATE_STOP         1
#define STATE_AUTO         2
#define MAX_MENU_STATE     2

#define MINIMUM_INTERVAL   20 // minimum minutes to run when auto started (prevent hysteresis)
#define UPPER_RH           40 //rh % when fan should start
//#define LOWER_RH           30 //rh % when fan should stop


#define hi(x)  (*(&x+1))

#use delay ( clock=8000000 )
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
void CheckSwitches ( void );
void printVersionInfo ( void );
void DisplayData ( void );
void StartFan ( void );
void StopFan ( void );
void AutoFan ( void );
int16 getRH ();

//void DisplayData ( long iAdcValue, char cLoc );

static char cDisplayDataFlag, cFan1Flag;
static char cInterruptCount, cViewing;
static char cMenuState, cSelectFlag;
static char cMenuSwitchOn, cMenuSwitchCount;
static char cSelectSwitchOn, cSelectSwitchCount;
static char cFan1State [ 9 ];
static char cDisplayUpdate;
static char cFan1RunTime, cFan1AutoState;


/*******************************************************************/

/*
The way to calculate the timing as an 8-bit timer is:
Fosc/4 /prescaler/256 = freq.
The main oscillator is divided by four. This is a hardware design and cannot be changed.
The prescaler is an 8-bit register that can be programmed to one of nine settings to be a 1:1 to 1:256 divider.
The 8-bit value in the setup statement tells the compiler to configure the timer as an 8-bit timer.
So, if you have a 20MHZ oscillator running, with the prescaler set to 8 the formula is:
20MHZ/4/8/256 = 20,000,000 /4 / 8 / 256
This will give you aproximately 2441 interrupts each second or an interrupt every 409uS.


Fra http://www.vermontficks.org/pic_calculations.xls
(256-C5)*1 / (A5 * 1000000 / B5 / 4) * 1000
C5: Preset Timer0 count before leaving interrupt (0-255)
A5: Clock frequency (MHz)
B5: Prescaler value
= Resulting interrupt rate (mS)

*/

//interrupt rate (mS): (256-6)*1 / (8 * 1000000 / 128 / 4) * 1000 = 16,000

#int_rtcc
void TimerInterrupt ( void ) // Gets here every 16.4mS at 8MHz, 8.2mS at 16MHz
{
   if ( cInterruptCount++ == 16 )      // if one second yet
   {

      cInterruptCount = 0;

      if(cDisplayUpdate ++ == 5) // update disp every 5 sec
      {
         cDisplayUpdate = 0;
         cDisplayDataFlag = ON; // signal time to update temp/humidity
      }

      // Vifte skal gå minst 20 minutter når AUTO slår den på
      //TODO: bytt fra sec til min
      if ( cFan1AutoState == ON )
      {
         cFan1RunTime ++;
         if( cFan1RunTime >= MINIMUM_INTERVAL ) // (MINIMUM_INTERVAL*60)
         {
            if( cFan1Flag==ON )
            {
               cFan1Flag = OFF; // signal ok to turn off fan if rh is under limits
            }
            cFan1RunTime = 0;
         }
      }
   }


   if ( input ( MENU_SWITCH ) == LOW )
   {
      if ( cMenuSwitchCount++ == 1 )     // debounce for 30mS, (was 2)
      {
         cMenuSwitchOn = YES;        // signal that switch was pressed
         cMenuSwitchCount = cViewing ? 252 : 240;       // set up for auto repeat (faster if viewing)
      }
   }
   else
   {
      cMenuSwitchCount = 0;             // switch up, restart
   }


   if ( input ( SELECT_SWITCH ) == LOW )
   {
      cSelectSwitchOn = YES;        // signal that switch was pressed
      cSelectSwitchCount = cViewing ? 252 : 240;       // set up for auto repeat (faster if viewing)
   }
   else
   {
      cSelectSwitchCount = 0;             // switch is up, restart count
   }

   set_rtcc ( 6 );     // Prescaler value, restart at adjusted value for 1-second accuracy
}

//*****************************************************************************

void printVersionInfo ( void )
{
   LCD_SetPosition ( LINE_1 + 0 );
   printf ( LCD_PutChar, " BADEVIFTE v1.2 " );
   LCD_SetPosition ( LINE_2+ 0 );
   printf ( LCD_PutChar, "Kenny 07.10.2007" );
}

void main ( void )
{
   //char recChar;
   delay_ms ( 200 );           // wait enough time after VDD rise
   setup_counters ( RTCC_INTERNAL, RTCC_DIV_128 );       // 16mS roll @8MHz

   output_low ( FAN_1 );
   output_low ( LED_1 );
   output_low ( LED_2 );


   LCD_Init();
   LCD_PutCmd ( CLEAR_DISP );

   printVersionInfo();
   delay_ms ( 5000 );
   LCD_PutCmd ( CLEAR_DISP );
   LCD_Init();

   // SETUP
   //port_c_pullups ( ON );
   setup_ccp1 ( CCP_OFF );
   setup_ccp2 ( CCP_OFF );
   setup_adc_ports ( RA0_ANALOG );     // these three statements set up the ADC
   setup_adc ( ADC_CLOCK_INTERNAL );   // clock source
   set_adc_channel ( 0 );              // select channel
   enable_interrupts ( INT_RTCC );     // turn on timer interrupt
   enable_interrupts ( GLOBAL );       // enable interrupts

   // INITIALIZE VARIABLES
   cFan1Flag = OFF;
   cSelectFlag = OFF;
   //cToggleFlag = 0;
   cMenuSwitchOn = OFF;
   cSelectSwitchOn = OFF;
   cMenuSwitchCount = 0;
   cSelectSwitchCount = 0;

   // start moduser
   cMenuState = STATE_AUTO;  // set first menu
   AutoFan();
   cDisplayDataFlag = ON; // update display

   output_float ( LED_2 );

   while ( TRUE )              // do forever
   {
      PrintMenu();            // display screens and enviroment info
      CheckSwitches();        // check and do any switch activity
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
         printf ( LCD_PutChar, "Stop          OK" );
      }
      break;

      case STATE_STOP:
      {
         printf ( LCD_PutChar, "Start         OK" );
      }
      break;

      case STATE_AUTO:
      {
         printf ( LCD_PutChar, "Auto          OK" );
      }
      break;

   }



   // DISPLAY KLIMA DATA
   if ( cDisplayDataFlag == ON )                    // if interrupt signalled an ADC reading
   {
      cDisplayDataFlag = OFF;
      DisplayData();
   }
}

void CheckSwitches ( void )
{

   // INCREMENT/DECREMENT MENU
   if ( cMenuSwitchOn == YES )      // if interrupt caught the switch press
   {
      if ( cMenuState++ >= MAX_MENU_STATE )      // if at maximum
      {
         cMenuState = 0;            // roll
      }
      cMenuSwitchOn = NO;     // turn back off
   }

   if ( cSelectSwitchOn == YES )      // if interrupt caught the switch press
   {
      cSelectFlag = ON;
      cSelectSwitchOn = NO;     // turn back off
   }

   // CHECK IF SELECT SWITCH IS PRESSED, PERFORM ACTION AND PRINT MENU
   switch ( cMenuState )
      {
         case ( STATE_START ): // START VIFTE
         {
            if ( cSelectFlag == ON )    // if SELECT (OK) switch is pressed
            {
               cSelectFlag = OFF;         // turn flag off

               StartFan();
               delay_ms ( 1000 );
               cDisplayDataFlag = ON;
               LCD_PutCmd ( CLEAR_DISP );
               cMenuSwitchOn = NO;
               cSelectSwitchOn = NO;
               cMenuState = STATE_STOP;    // menu displays "STOP"
               break;
            }
         }

         case ( STATE_STOP ): // STOPP VIFTE
         {
            if ( cSelectFlag == ON )    // if SELECT (OK) switch is pressed
            {
               cSelectFlag = OFF;         // turn flag off

               StopFan();
               delay_ms ( 1000 );
               cDisplayDataFlag = ON;
               LCD_PutCmd ( CLEAR_DISP );
               cMenuSwitchOn = NO;
               cSelectSwitchOn = NO;
               cMenuState = STATE_START;    // menu displays "START"

               break;
            }
         }

         case ( STATE_AUTO ):
         {
            if ( cSelectFlag == ON )    // if SELECT (OK) switch is pressed
            {
               cSelectFlag = OFF;         // turn flag off

               AutoFan();

               delay_ms ( 1000 );
               cDisplayDataFlag = ON;
               LCD_PutCmd ( CLEAR_DISP );
               cMenuSwitchOn = NO;
               cSelectSwitchOn = NO;
               cMenuState = STATE_STOP;    // menu displays "STOP"

               break;
            }
         }
   }
}

void DisplayData ( void )
{
   int16 f_temp, f_rh;

   f_rh = getRH();

   // Relative Humidity
   LCD_SetPosition ( LINE_1 );

   if ( f_rh == 0x3FF )
   {
       printf ( LCD_PutChar, "O/R" );    // out of range
   }
   else
   {
      //printf ( LCD_PutChar, "%2.1f%c", f_rh, PERCENT );
      printf ( LCD_PutChar, "%2.1f", f_rh ); // LCD takler jo ikke % ...
   }

   // Temperature
   f_temp = 0; // not implemented yet
   LCD_SetPosition ( LINE_1 + 14);
   //printf ( LCD_PutChar, "%2.1f%c", f_temp, CELCIUS );
   printf ( LCD_PutChar, "%2.1f", f_temp );

   // FAN operational status
   LCD_SetPosition ( LINE_1 + 4); // '0.0 AUTO OFF 0.0'
   if( cMenuState == STATE_AUTO )
   {
      if( cFan1AutoState == OFF )
      {
         //strcpy ( cFan1State, "AUTO  OFF" );
         printf ( LCD_PutChar, "AUTO OFF" );
      }
      else
      {
         //strcpy ( cFan1State, "AUTO  ON" );
         printf ( LCD_PutChar, "AUTO ON" );
      }
   }
   else if( cMenuState == STATE_START )
   {
         //strcpy ( cFan1State, " RUNNING " );
         printf ( LCD_PutChar, " RUNNING " );
   }
   else if( cMenuState == STATE_STOP )
   {
         //strcpy ( cFan1State, " STOPPED " );
         printf ( LCD_PutChar, "STOPPED" );
   }



   //printf ( LCD_PutChar, "%s", cFan1State ); // display fan status

}


int16 getRH()
{

   int16 adc_value, adc_volt, rh;

   // RH: RH = ((A/D voltage / supply voltage) - 0.16) / 0.0062
   // volt adc = iAdcHumValue / 1024 * 5
   // (417 / 1024) * 5 = 2,0361328125
   // (2,0361328125 / 5) = 0,4072265625 - 0,16 = 0,2472265625 / 0,0062 = 39,87 RH
   // ELLER
   // (417 / 1024) = 0,4072265625 - 0,16 = 0,2472265625 / 0,0062 = 39,87 RH
   // 0,4072265625 - 0,16 =
   //
   set_adc_channel ( 0 );
   adc_value = read_adc();
   /*
   // out of range
   if( adc_value == 0x3FF )
   {
      return adc_value;
   }
   */
   adc_volt = ( adc_value / 1024 ) * VDD;
   rh = ( ( adc_volt / VDD ) - 0.16 ) / 0.0062;

   return rh;

}


void Reboot( void )
{
   reset_cpu();
}

void StartFan( void )
{
   //cDelayedActive = NO;

   cFan1Flag = ON;
   output_low ( FAN_1 );
   output_low ( LED_2 );
}

void StopFan( void )
{
   //cDelayedActive = NO;

   cFan1Flag = OFF;
   output_float ( FAN_1 );
   output_float ( LED_2 );
}


void AutoFan( void )
{
   float rh;

   rh = getRH();

   if( rh > UPPER_RH ) // RH is above upper threshold, start fan if not already running
   {
      if ( cFan1AutoState == OFF )
      {
         cFan1AutoState = ON;
         cFan1RunTime = 0; // starting fan, reset interval counter. increases by interrupt timer.
         output_low ( FAN_1 );
         output_low ( LED_2 );
      }
   }
   else
   {
      if( cFan1Flag == ON ) // time forces us to keep running (to prevent hysteresis)
      {
         cFan1AutoState = ON;
      }
      else
      {
         if ( cFan1AutoState == ON )
         {
            cFan1AutoState = OFF;
            cFan1RunTime = 0; // stopping fan, reset interval counter
            output_float ( FAN_1 );
            output_float ( LED_2 );
         }
      }
   }

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
