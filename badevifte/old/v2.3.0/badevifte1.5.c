/*

Datastyring for baderomsvifte, basert p� relativ luftfuktighet

Ken-Roger Andersen Oktober 2007

v2.2: Lagt inn temperatur sensor (NTC 1k@25�C) + menyvalget for visning av settings
v1.5: Fjernet OK knappen for valg av modus. Modus velges kun med 1 bryter, ettersom menyvalg increments, g�r den automatisk
til denne modusen.


*/

#include <16F876.H>

#device ADC=10


#include <kenny.h>
#include <math.h>
#include <ntc.h>

#include "ntc.c"




#fuses XT, NOPROTECT, NOPUT, NOWDT, NOBROWNOUT, NOLVP, NOCPD, NOWRT

// ADC
#define VDD                 5.00

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

#define C_PERCENT			0x25 // % = 00100101 = 25 = 0x25
#define C_DEGREES			0xDF // � = 11011111 = DF = 0xDF


#define MENU_SWITCH        PIN_C1
//#define SELECT_SWITCH      PIN_C2
#define INCLUDE_TEMP_SWITCH		PIN_C2

//#define RX_IN               PIN_A4 //pinne 6 RA4
#define LED_1               PIN_B3 //pinne 24
#define LED_2               PIN_B4 //pinne 25
#define FAN_1               PIN_C0

#define CMD_NUL     0

#define STATE_AUTO         0
#define STATE_START        1
#define STATE_STOP         2
#define STATE_SETTINGS     3
#define MAX_MENU_STATE     3

#define MINIMUM_INTERVAL   1  // minimum minutes to run when auto started (prevent hysteresis)
#define MAXIMUM_RH         45 // rh % when fan should start
#define MINIMUM_TEMP       18 // �C, never auto run if under this


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
float getRH ( void );
float getTemp ( void );

//void DisplayData ( long iAdcValue, char cLoc );

static char cDisplayDataFlag, cFan1Flag;
static char cInterruptCount;
static char cMenuState, cSelectFlag;
static char cMenuSwitchOn, cMenuSwitchCount;
static char cSelectSwitchOn, cSelectSwitchCount;
static char cDisplayUpdate, cToggleFlag;
static char cFan1RunTime, cFan1AutoState, /*cFan1IsAutoMode,*/ cFan1CanStop;


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
	if ( cInterruptCount++ >= 31 )      // (16) if one second yet ! TROR DET SKAL V�RE 31 !!!
	{

		cInterruptCount = 0;

		if(cDisplayUpdate ++ == 5) // update disp every 5 sec
		{
			cDisplayUpdate = 0;
			cDisplayDataFlag = ON; // signal time to update temp/humidity
		}


		// Fan must run N minutes after auto mode is stopped, to avoid hysteresis
		if ( cMenuState == STATE_AUTO && cFan1Flag == ON && cFan1CanStop == NO )
		{
			if( cFan1RunTime ++ == ( MINIMUM_INTERVAL * 60 ) ) // (MINIMUM_INTERVAL*60)
			{
				cFan1CanStop = YES; // signal ok to turn off fan if rh is under limits
				cDisplayDataFlag = ON; // signal time to update temp/humidity
			}
			else
			{
				cFan1CanStop = NO; // keep on running
			}
		}
		else
		{
			cFan1RunTime = 0;
		}

		//if ( input ( FAN_1 ) == LOW )
		if ( cFan1Flag == ON ) //v1.6
		{
			cToggleFlag ^= 1;              // toggle (blink) LED while fan is running
			if ( cToggleFlag == 1)
			{
				output_low ( LED_2 );
			}
			else
			{
				output_float ( LED_2 );
			}
		}
		else
		{
			output_float ( LED_2 ); // sl� av LED hvis ikke vifte g�r
		}

	}
// bruk menystate
// if fan1flag run && rh > limit ikke kj�rt output  etc

	if ( input ( MENU_SWITCH ) == LOW )
	{
		if ( cMenuSwitchCount++ == 1 )     // debounce for 30mS, (was 2)
		{
			cMenuSwitchOn = YES;        // signal that switch was pressed
			cMenuSwitchCount = 240; //cViewing ? 252 : 240;       // set up for auto repeat (faster if viewing)
		}
	}
	else
	{
		cMenuSwitchCount = 0;             // switch up, restart
	}

/*
	if ( input ( SELECT_SWITCH ) == LOW )
	{
		if ( cSelectSwitchCount++ == 1 )  // debounce for 30mS (was 2)
		{
			cSelectSwitchOn = YES;        // signal that switch was pressed
			cSelectSwitchCount = 240; // cViewing ? 252 : 240;       // set up for auto repeat (faster if viewing)
		}
	}
	else
	{
		cSelectSwitchCount = 0;             // switch is up, restart count
	}
*/
	set_rtcc ( 6 );     // Prescaler value, restart at adjusted value for 1-second accuracy
}

//*****************************************************************************

void printVersionInfo ( void )
{
	LCD_SetPosition ( LINE_1 + 0 );
	printf ( LCD_PutChar, "BADEVIFTE v2.3.0" );
	LCD_SetPosition ( LINE_2+ 0 );
	printf ( LCD_PutChar, "Kenny 15.10.2007" );
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


	setup_adc_ports ( AN0_AN1_AN3 );	// set up the ADC

	//setup_adc_ports ( RA0_ANALOG );     // set up the ADC
	setup_adc ( ADC_CLOCK_INTERNAL );   // clock source
	//set_adc_channel ( 0 );              // select channel




	enable_interrupts ( INT_RTCC );     // turn on timer interrupt
	enable_interrupts ( GLOBAL );       // enable interrupts

	// INITIALIZE VARIABLES
	cFan1Flag = OFF;
	cSelectFlag = OFF;
	//cFan1IsAutoMode = NO;
	cToggleFlag = 0;
	cMenuSwitchOn = OFF;
	cSelectSwitchOn = OFF;
	cMenuSwitchCount = 0;
	cSelectSwitchCount = 0;

	output_high ( FAN_1 );
	output_float ( LED_2 );


	// start moduser
	cMenuState = STATE_AUTO;  // set first menu
	//cFan1IsAutoMode = YES;

	//AutoFan();


	cDisplayDataFlag = ON; // update display



	while ( TRUE )              // do forever
	{
		PrintMenu();            // display screens and enviroment info
		CheckSwitches();        // check and do any switch activity
		AutoFan();				// routine itself checks if automode enabled
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
			printf ( LCD_PutChar, "ON        Change" );
		}
		break;

		case STATE_STOP:
		{
			printf ( LCD_PutChar, "OFF       Change" );
		}
		break;

		case STATE_AUTO:
		{
			printf ( LCD_PutChar, "AUTO      Change" );
		}
		break;

		case STATE_SETTINGS:
		{
			printf ( LCD_PutChar, "Settings  Change" );
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

		cSelectFlag = ON; // ny i v1.5
	}
/*
   if ( cSelectSwitchOn == YES )      // if interrupt caught the switch press
   {
      cSelectFlag = ON;
      cSelectSwitchOn = NO;     // turn back off
   }
*/
   // CHECK IF SWITCH IS PRESSED, PERFORM ACTION AND PRINT MENU
	switch ( cMenuState )
	{
		case ( STATE_START ): // START FAN
		{
			if ( cSelectFlag == ON )    // if switch is pressed
			{
				cSelectFlag = OFF;         // turn flag off

				//cFan1AutoState = OFF; //v1.6
				//cFan1IsAutoMode = NO;
				StartFan();

				//delay_ms ( 1000 );
				LCD_PutCmd ( CLEAR_DISP );
				cDisplayDataFlag = ON;
				cMenuSwitchOn = NO;
				cSelectSwitchOn = NO;
				cInterruptCount = 0;    // synchronize interrupt timing from here
				//cMenuState = STATE_STOP;    // menu displays "STOP"
				break;
			}
		}

		case ( STATE_STOP ): // STOP FAN
		{
			if ( cSelectFlag == ON )    // if switch is pressed
			{
				cSelectFlag = OFF;         // turn flag off

				//cFan1AutoState = OFF; //v1.6
				//cFan1IsAutoMode = NO;
				StopFan();

				//delay_ms ( 1000 );
				LCD_PutCmd ( CLEAR_DISP );
				cDisplayDataFlag = ON;
				cMenuSwitchOn = NO;
				cSelectSwitchOn = NO;
				//cMenuState = STATE_START;    // menu displays "START"

				break;
			}
		}

		case ( STATE_AUTO ):
		{
			if ( cSelectFlag == ON )    // if switch is pressed
			{
				cSelectFlag = OFF;         // turn flag off
				LCD_PutCmd ( CLEAR_DISP );
				cDisplayDataFlag = ON;
				cMenuSwitchOn = NO;
				cSelectSwitchOn = NO;
				cInterruptCount = 0;    // synchronize interrupt timing from here

				break;
			}
		}

		case ( STATE_SETTINGS ):
		{
			if ( cSelectFlag == ON )    // if switch is pressed
			{
				cSelectFlag = OFF;         // turn flag off

				LCD_PutCmd ( CLEAR_DISP );
				LCD_SetPosition ( LINE_1 );
				printf ( LCD_PutChar, " Humidity limit " );
				LCD_SetPosition ( LINE_2 );
				printf ( LCD_PutChar, "      %u%c       ", MAXIMUM_RH, C_PERCENT );
				delay_ms ( 3000 );

				LCD_PutCmd ( CLEAR_DISP );
				LCD_SetPosition ( LINE_1 );
				printf ( LCD_PutChar, "Minimum run time" );
				LCD_SetPosition ( LINE_2 );
				printf ( LCD_PutChar, "%umin if Auto ON", MINIMUM_INTERVAL );
				delay_ms ( 3000 );

				LCD_PutCmd ( CLEAR_DISP );
				LCD_SetPosition ( LINE_1 );
				printf ( LCD_PutChar, "Not run if under" );
				LCD_SetPosition ( LINE_2 );
				printf ( LCD_PutChar, "       %u%cC     ", MINIMUM_TEMP, C_DEGREES );
				delay_ms ( 3000 );


				LCD_PutCmd ( CLEAR_DISP );
				cDisplayDataFlag = ON;
				cMenuSwitchOn = NO;
				cSelectSwitchOn = NO;
				cInterruptCount = 0;    // synchronize interrupt timing from here
				cMenuState = STATE_AUTO;    // menu displays "AUTO"

				break;
			}
		}
	}
}

void DisplayData ( void )
{
	float f_rh;
	int f_temp;

	f_rh = getRH();

	// Relative Humidity
	LCD_SetPosition ( LINE_1 );

	if ( f_rh == 0.0 ) // 1023
	{
		printf ( LCD_PutChar, "O/R" );    // out of range
	}
	else
	{
		printf ( LCD_PutChar, "%2.1f%c", f_rh, C_PERCENT );
	}

	// Temperature
	LCD_SetPosition ( LINE_1 + 12 );

	//f_temp = 0; // not implemented yet
	f_temp = getTemp();

	if ( f_temp == 100 )
	{
		printf ( LCD_PutChar, "O/R" );    // out of range
	}
	else
	{
		printf ( LCD_PutChar, "%.2u%c", f_temp, C_DEGREES );
	}

	//printf ( LCD_PutChar, "%2f%c", f_temp, C_DEGREES );



	// FAN operational status
	LCD_SetPosition ( LINE_1 + 6); // '45.0% AU OFF 25�'
	if( cMenuState == STATE_AUTO )
	{
		if( cFan1AutoState == OFF )
		{
			printf ( LCD_PutChar, "AU OFF" );
		}
		else
		{
			printf ( LCD_PutChar, "AU ON " );
		}
	}
	else
	{
		if( cFan1Flag == ON )
		{
			printf ( LCD_PutChar, "ON    " );
		}
		else
		{
			printf ( LCD_PutChar, "OFF   " );
		}
	}
}


float getTemp()
{
	int16 adc_value;
	float ohm, tcelsius;


	// see ntc.h and ntc.c

	set_adc_channel ( 1 ); // RA1, pin 3
	delay_us(50);
	adc_value = read_adc();
	ohm = adc_2_ohm ( adc_value );
	tcelsius = r2temperature ( ohm );

	return tcelsius;
}


float getRH()
{

	int16 adc_value;
	float adc_volt, rh;

	rh = 0.0;

	// RH: RH = ((A/D voltage / supply voltage) - 0.16) / 0.0062
	// volt adc = iAdcHumValue / 1024 * 5
	// (417 / 1024) * 5 = 2,0361328125
	// (2,0361328125 / 5) = 0,4072265625 - 0,16 = 0,2472265625 / 0,0062 = 39,87 RH
	// ELLER
	// (417 / 1024) = 0,4072265625 - 0,16 = 0,2472265625 / 0,0062 = 39,87 RH
	// 0,4072265625 - 0,16 =
	//
	set_adc_channel ( 0 ); // RA0, pin 2
	delay_us(50);

	adc_value = read_adc();

	if ( adc_value < 1 || adc_value > 1023 )
		return rh;

	// calc relative humidity from adc value
	adc_volt = ( float ) adc_value / 1023  * VDD; // 1023 leaves room for out-of-range
	//rh = ( adc_volt / VDD  - 0.16 ) / 0.0062;
	rh = ( ( float ) adc_volt / VDD  - 0.16 ) / 0.0062; // v1.5

	return rh;

}


void Reboot( void )
{
	reset_cpu();
}

void StartFan( void )
{
	cFan1Flag = ON;
	// reset possible auto counters
	//cFan1AutoState = OFF;
	//cFan1RunTime = 0; // stopping fan, reset interval counter

	output_low ( FAN_1 );
	output_low ( LED_2 );
}

void StopFan( void )
{
	cFan1Flag = OFF;
	// reset possible auto counters
	//cFan1AutoState = OFF;
	//cFan1RunTime = 0; // stopping fan, reset interval counter

	output_high ( FAN_1 );
	output_float ( LED_2 );
}


void AutoFan( void )
{
	float rh, tcelsius;

	if ( cMenuState != STATE_AUTO ) return;

	tcelsius = getTemp();
	rh = getRH();

	if( rh > MAXIMUM_RH )
	{
		if ( input (INCLUDE_TEMP_SWITCH) == LOW && tcelsius < MINIMUM_TEMP )
		{
			//don't run if temp to low and check-temp switch is on
			return;
		}
		// RH is above upper threshold and temp check is either ok or deactivated, start fan if not already running
		if ( cFan1Flag == OFF )
		{
			cFan1Flag = ON;
			cFan1CanStop = NO;
			//cFan1AutoState = ON;
			output_low ( FAN_1 );

		}

		cFan1RunTime = 0; // reset interval counter. increases by interrupt timer.
	}
	else
	{
		if( cFan1CanStop == NO )
		{
			// time forces us to keep running (to prevent hysteresis)
		}
		else
		{
			if ( cFan1Flag == ON )
			{
				cFan1Flag = OFF;
				cFan1CanStop = NO;
				//cFan1AutoState = OFF;
				//cFan1RunTime = 0; // stopping fan, reset interval counter
				output_high ( FAN_1 );

			}
		}
	}

	cFan1AutoState = cFan1Flag; // for correct view in display

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
