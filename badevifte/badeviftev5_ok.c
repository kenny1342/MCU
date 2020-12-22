/**
Datastyring for baderomsvifte, basert på relativ luftfuktighet (HIH3610 sensor) og temperatur (NTC sensor).

Ken-Roger Andersen Oktober 2007 - April 2008 <ken.roger@gmail.com>

v5.1: Endret MAXIMUM_RH_1 fra 47% til 55%, den gikk litt for lenge. Endret DELAYED_INTERVAL fra 3 min til 10 min.
	  Fikset bug med display når RH >= 100%, prosent tegnet ble forskyvet, og ble stående når RH ble < 100%
v5.0: Prosjekt gjenopptatt. (29.03.2008)
v3.14: Flyttet NTC og LCD funksjoner til egne include filer
v3.13: Bug i interrupt timing, kjørte kode hvert halve sekund istedet for hele sekund...  (endret count fra 31 til 61)
v3.12: Div bugfix. Fjernet igjen blink av ON i display når fan ON
v3.11: Endret modus "show settings" til modus DELAYED RUN. Lagt inn "show settings" hvis knapp trykt inn ca 2 sek istedet.
v3.10: Div bugfix av 3.9 features...
v3.9: La inn blink av ON i display når fan ON, div bugfix
v3.8: Justert NTC_RN fra 1000 til 932 (viste ca 2 grader for mye).
v3.7: Lagt til jumper RH grense default eller høy (default + 10%)
v3.6: Cleanup i source koden
v3.5: Funksjon for reboot når knappen holdes inne > 3,9 sekunder
v2.3: Lagt inn ny B verdi for NTC. Flytting handling av knappen til "slipp opp" istedet for trykk.
v2.2: Lagt inn temperatur sensor (NTC 1k@25°C) + menyvalget for visning av settings
v1.5: Fjernet OK knappen for valg av modus. Modus velges kun med 1 knapp, den øker sekvensielt for hvert trykk til denne modusen.
v1.0: Proof of concept, bygging og feilsøking av hardware

Datasheets:
hih3610.pdf
PIC16F87X.pdf
HD44780.pdf
*/

#include <16F876.H>

#device ADC=10

#include <kenny.h>	// Defines used in all my projects
#include <math.h>	// C math operations (NTC routines uses §log
#include <ntc.h>	// NTC defines
#include <lcd.h>	// LCD


#fuses XT, NOPROTECT, NOPUT, NOWDT, NOBROWNOUT, NOLVP, NOCPD, NOWRT

// ADC
#define VDD                 5.00


// Enviroment configurations, adjusted after final installation in bathroom, programmed over ICSP
#define MINIMUM_INTERVAL		3  // minimum minutes to run when auto started (prevent hysteresis)
#define MAXIMUM_RH_1			55 // rh % level 1 when fan should start (set with jumpers)
#define MAXIMUM_RH_2			MAXIMUM_RH_1+10 // rh % level 2 when fan should start (set with jumpers)
#define MINIMUM_TEMP			22 // °C, never auto run if under this
#define DELAYED_INTERVAL		10  // minutes to run from delayed mode started

// Hardware configurations
#define MENU_SWITCH				PIN_C1
#define INCLUDE_TEMP_JUMPER		PIN_C2
#define LED_1					PIN_B3 // pinne 24
#define LED_2					PIN_B4 // pinne 25
#define FAN_1					PIN_C0
#define RH_LIMIT_2_JUMPER		PIN_A4 // pinne 6

// Menu configurations
#define STATE_AUTO				0
#define STATE_START				1
#define STATE_STOP				2
#define STATE_DELAYED			3
#define MAX_MENU_STATE			3

// symbols from LCD char map used for Hitachi 44780 chipsets, see LCD_Char_Map.gif for binary map
#define C_PERCENT				0x25 // % = 00100101 = 25 = 0x25
#define C_DEGREES				0xDF // ° = 11011111 = DF = 0xDF


#define hi(x)  (*(&x+1))

#use delay ( clock=8000000 )
#use standard_io ( A )
#use standard_io ( B )
#use standard_io ( C )


// protos
void SetTime ( void );
void CheckSwitches ( void );
void printVersionInfo ( void );
void DisplaySettings ( void );
void DisplayData ( void );
void DelayedFan( void );
void StartFan ( void );
void StopFan ( void );
void AutoFan ( void );
float getRH ( void );
float getTemp ( void );
float getInterval ( void );

static char cDisplayDataFlag, cFan1Flag;
static char cInterruptCount;
static char cMenuState, cSelectFlag;
static char cMenuSwitchOn, cMenuSwitchCount;
static char cSelectSwitchOn, cSelectSwitchCount;
static char cDisplayUpdate, cToggleFlag, cToggleFlagLCD, cDisplaySettingsFlag;
static char cFan1RunTime, cFan1DelayedRunTime, cFan1AutoState, cFan1DelayedState, cFan1CanStop, IntervalMinutes;

#include "ntc.c"	// NTC calculations
#include "lcd.c"	// LCD interfacing

/*******************************************************************

-----------------------------------------------------------------------------------------
Finne Interrupt rate (antall pr sekund):
-----------------------------------------------------------------------------------------
F: Oscillator frekvens i Hz (8MHz = 8000000)
P: Prescaler 8-bit verdi (defineres i koden, f.eks: setup_counters ( RTCC_INTERNAL, RTCC_DIV_128 ) )
PRT: Preset Timer0 før hopper ut av interrupt (0-255)
I: Interrupt rate (antall pr sekund)

I = F / 4 / P / (256-PRT)

Med 8MHz oscillator, prescale bit 128 og PRT 0:
I = 8000000 / 4 / 128 / (256-0)
I = 61,03
	=====

Med 8MHz oscillator, prescale bit 128 og PRT 4:
I = 8000000 / 4 / 128 / (256-4)
I = 62,00
	=====


-----------------------------------------------------------------------------------------
Velge loop teller verdi i interrupt:
-----------------------------------------------------------------------------------------
N: Sekund interval (ms) du ønsker tick
I: Interrupt rate (antall pr sekund)
C: loop teller i Interrupt

C = I * 1000 / N

Med 2 sekunds tick og interrupt rate 61,03:
C = 61,03 * 1000 / 2000
C = 30,515
	=======



***********************************************************************/

#int_rtcc
void TimerInterrupt ( void ) // Gets here every 16.4mS at 8MHz, 8.2mS at 16MHz
{
	if ( cInterruptCount++ >= 61 )      // if one second yet
	{

		cInterruptCount = 0;

		if(cDisplayUpdate ++ == 3) // update disp every 5 sec
		{
			cDisplayUpdate = 0;
			cDisplayDataFlag = ON; // signal time to update temp/humidity
		}


		/************ AUTO MODE ******************/
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


		/************ DELAYED MODE ******************/
		if ( cMenuState == STATE_DELAYED && cFan1Flag == ON && cFan1CanStop == NO )
		{
			//if( cFan1DelayedRunTime ++ == ( DELAYED_INTERVAL * 60 ) )
			if( cFan1DelayedRunTime ++ == ( IntervalMinutes * 60 ) )
			{
				cFan1CanStop = YES; // signal ok to turn off fan, delayed period finished
				cDisplayDataFlag = ON; // signal time to update temp/humidity
			}
			else
			{
				cFan1CanStop = NO; // keep on running
			}
		}
		else
		{
			cFan1DelayedRunTime = 0;
		}



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
			output_float ( LED_2 ); // turn off LED if fan not running
		}

	}

	if ( input ( MENU_SWITCH ) == LOW )	// if button still pressed
	{
		if ( cMenuSwitchCount < 128 )	// hold at 128
		{
			cMenuSwitchCount++;			// otherwise increment
		}

	}
	else								// if button is unpressed
	{
		if ( cMenuSwitchCount > 2 )			// filter out glitches
		{
			// If button press is greater than 3.9 seconds (31msec * 128 / 1000), cold reset
			if ( cMenuSwitchCount == 128 )
			{
				reset_cpu();
			}
			else if ( cMenuSwitchCount >= 64 && cMenuSwitchCount < 128 ) // ~2 to 3.8 secs, display settings
			{
				cDisplaySettingsFlag = 1;
			}
			else
			{
				cMenuSwitchOn = YES;        	// signal that switch was pressed
			}
		}

		cMenuSwitchCount = 0;             // switch up, restart
	}

	set_rtcc ( 4 );     // Prescaler value, restart at adjusted value for 1-second accuracy
}

//*****************************************************************************

void printVersionInfo ( void )
{
	LCD_SetPosition ( LINE_1 + 0 );
	printf ( LCD_PutChar, " BADEVIFTE v5.0" );
	LCD_SetPosition ( LINE_2+ 0 );
	printf ( LCD_PutChar, "Kenny 29.03.2008" );
}

void main ( void )
{

	delay_ms ( 200 );           // wait enough time after VDD rise
	setup_counters ( RTCC_INTERNAL, RTCC_DIV_128 );       // 31mS roll @8MHz

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
	setup_ccp1 ( CCP_OFF );
	setup_ccp2 ( CCP_OFF );

	setup_adc_ports ( AN0_AN1_AN3 );	// set up the ADC on analog inputs AN0, AN1 and AN3
	setup_adc ( ADC_CLOCK_INTERNAL );   // clock source

	enable_interrupts ( INT_RTCC );     // turn on timer interrupt
	enable_interrupts ( GLOBAL );       // enable interrupts

	// INITIALIZE VARIABLES
	cFan1Flag = OFF;
	cSelectFlag = OFF;
	cToggleFlag = 0;
	cToggleFlagLCD = 0;
	cDisplaySettingsFlag = OFF;
	cMenuSwitchOn = OFF;
	cSelectSwitchOn = OFF;
	cMenuSwitchCount = 0;
	cSelectSwitchCount = 0;

	// bootup diagnose view
	output_high ( FAN_1 );
	output_float ( LED_2 );

	output_float ( RH_LIMIT_2_JUMPER );
	output_float ( INCLUDE_TEMP_JUMPER );
	// start modus
	cMenuState = STATE_AUTO;  // set first menu

	cDisplayDataFlag = ON; // update display immediatly

	while ( TRUE )              // do forever
	{
		PrintMenu();            // display screens and enviroment info
		CheckSwitches();        // check and do any switch activity
		AutoFan();				// routine itself checks if automode active
		DelayedFan();			// routine itself checks if delayed mode active
		IntervalMinutes = getInterval();
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
			printf ( LCD_PutChar, "ON        Next->" );
		}
		break;

		case STATE_STOP:
		{
			printf ( LCD_PutChar, "OFF       Next->" );
		}
		break;

		case STATE_AUTO:
		{
			printf ( LCD_PutChar, "AUTO      Next->" );
		}
		break;

		case STATE_DELAYED:
		{
			printf ( LCD_PutChar, "Delayed   Next->" );
		}
		break;

	}

	if( cDisplaySettingsFlag == ON )
	{
		cDisplaySettingsFlag = OFF;
		DisplaySettings();
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

   // CHECK IF SWITCH IS PRESSED, PERFORM ACTION AND PRINT MENU
	switch ( cMenuState )
	{
		case ( STATE_START ): // START FAN
		{
			if ( cSelectFlag == ON )    // if switch is pressed
			{
				cSelectFlag = OFF;         // turn flag off

				StartFan();

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

				StopFan();

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

				cFan1CanStop = YES;		// possible to stop when auto is selected

				LCD_PutCmd ( CLEAR_DISP );
				cDisplayDataFlag = ON;
				cMenuSwitchOn = NO;
				cSelectSwitchOn = NO;
				cInterruptCount = 0;    // synchronize interrupt timing from here

				break;
			}
		}

		case ( STATE_DELAYED ):
		{
			if ( cSelectFlag == ON )    // if switch is pressed
			{
				cSelectFlag = OFF;         // turn flag off

				cFan1CanStop = NO;		// always start when delayed is selected

				LCD_PutCmd ( CLEAR_DISP );
				cDisplayDataFlag = ON;
				cMenuSwitchOn = NO;
				cSelectSwitchOn = NO;
				cInterruptCount = 0;    // synchronize interrupt timing from here

				break;
			}
		}
	}
}


void DisplaySettings ( void )
{

	disable_interrupts ( INT_RTCC );     // turn off timer interrupt
	disable_interrupts ( GLOBAL );       // disable interrupts


	LCD_PutCmd ( CLEAR_DISP );
	LCD_SetPosition ( LINE_1 );
	printf ( LCD_PutChar, " Humidity limit " );
	LCD_SetPosition ( LINE_2 );
	if ( input (RH_LIMIT_2_JUMPER ) == LOW )
	{
		printf ( LCD_PutChar, "%u%c - default+10", MAXIMUM_RH_2, C_PERCENT );
	}
	else
	{
		printf ( LCD_PutChar, "      %u%c       ", MAXIMUM_RH_1, C_PERCENT );
	}
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
	LCD_SetPosition ( LINE_1 );
	printf ( LCD_PutChar, "When auto mode:" );
	LCD_SetPosition ( LINE_2 );
	if ( input (INCLUDE_TEMP_JUMPER) == LOW)
	{
		printf ( LCD_PutChar, "No check on temp" );
	}
	else
	{
		printf ( LCD_PutChar, "Check on temp   " );
	}
	delay_ms ( 3000 );

	LCD_PutCmd ( CLEAR_DISP );
	LCD_SetPosition ( LINE_1 );
	printf ( LCD_PutChar, "Delayed run time" );
	LCD_SetPosition ( LINE_2 );
	printf ( LCD_PutChar, "%umin           ", IntervalMinutes );
	delay_ms ( 3000 );


	LCD_PutCmd ( CLEAR_DISP );
	cDisplayDataFlag = ON;
	cMenuSwitchOn = NO;
	cSelectSwitchOn = NO;
	cInterruptCount = 0;    // synchronize interrupt timing from here
	enable_interrupts ( INT_RTCC );     // turn on timer interrupt again
	enable_interrupts ( GLOBAL );       // enable interrupts again

}

void DisplayData ( void )
{
	float f_rh;
	float f_temp;

	f_rh = getRH();

	// Relative Humidity
	LCD_SetPosition ( LINE_1 );

	if ( f_rh == 0.0 ) // 1023
	{
		printf ( LCD_PutChar, "O/R" );    // out of range
	}
	else
	{
		if(f_rh < 100)
		{
			printf ( LCD_PutChar, "%2.1f%c ", f_rh, C_PERCENT );
		}
		else
		{
			printf ( LCD_PutChar, "%2.1f%c", f_rh, C_PERCENT );
		}
	}

	// Temperature from NTC device
	LCD_SetPosition ( LINE_1 + 13 );

	f_temp = getTemp();

	if ( f_temp == 100 )
	{
		printf ( LCD_PutChar, "O/R" );    // out of range
	}
	else
	{
		printf ( LCD_PutChar, "%2.0f%c", f_temp, C_DEGREES );
	}

	// Display FAN operational status
	LCD_SetPosition ( LINE_1 + 6); // '45.0% AU OFF 25°'
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
			printf ( LCD_PutChar, "  ON  " );
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

	return ( float ) tcelsius;
}


float getRH()
{

	int16 adc_value;
	float adc_volt, rh;

	rh = 0.0;

	/*
		Formulas for the HIH3610 sensor
		RH: RH = ((A/D voltage / supply voltage) - 0.16) / 0.0062
		volt adc = iAdcHumValue / 1024 * 5
		(417 / 1024) * 5 = 2,0361328125
		(2,0361328125 / 5) = 0,4072265625 - 0,16 = 0,2472265625 / 0,0062 = 39,87 RH
		ELLER
		(417 / 1024) = 0,4072265625 - 0,16 = 0,2472265625 / 0,0062 = 39,87 RH
		0,4072265625 - 0,16 =
	*/
	set_adc_channel ( 0 ); // RA0, pin 2
	delay_us(50);

	adc_value = read_adc();

	if ( adc_value < 1 || adc_value > 1023 )
		return rh;

	// calc relative humidity from 10bit adc value
	adc_volt = ( float ) adc_value / 1023  * VDD; // 1023 leaves room for out-of-range
	rh = ( ( float ) adc_volt / VDD  - 0.16 ) / 0.0062; // v1.5

	return rh;

}


float getInterval()
{

	int16 adc_value;
	float interval ;

	interval = 0;

	set_adc_channel ( 2 ); // RA2, pin 4
	delay_us(50);

	adc_value = read_adc();

	if ( adc_value < 1 || adc_value > 1023 )
		return 30; // default 30 min

	// calc minutes from 10bit adc value in range 1 - 60 min
	interval = ( ( float ) adc_value * 3.6 / 60 ); // 3.6 because 60 min = 3600 sec (!?!)

	if ( interval < 1 )
		interval = 1;

	return interval;
//30min = 1800 sec
//60min = 3600 sec
}

//1023 * 3,6 = 3682,8 sec / 60 = 61,38 min
//400 * 3,6 = 1440 sec / 60 = 24 min
//100 * 3,6 = 360 sec / 60 = 6 min

void Reboot( void )
{
	reset_cpu();
}

void StartFan( void )
{
	cFan1Flag = ON;
	output_low ( FAN_1 );
}

void StopFan( void )
{
	cFan1Flag = OFF;
	output_high ( FAN_1 );
}

void DelayedFan( void )
{
	if ( cMenuState != STATE_DELAYED ) return;


	if( cFan1CanStop == NO )
	{
			// time forces us to keep running
		if ( cFan1Flag == OFF )
		{
			cFan1Flag = ON;
			//cFan1CanStop = NO;
			output_low ( FAN_1 );

		}
	}
	else
	{
		if ( cFan1Flag == ON ) // always on when here but....
		{
			cFan1Flag = OFF;
			cFan1CanStop = NO;
			output_high ( FAN_1 );
		}

		cMenuState = STATE_AUTO;    // menu returns to "AUTO"
	}

	cFan1DelayedState = cFan1Flag; // for correct view in display
}


void AutoFan( void )
{
	float rh, tcelsius;
	float SELECTED_RH_LIMIT;

	if ( cMenuState != STATE_AUTO ) return;

	tcelsius = getTemp();
	rh = getRH();

	// Determine witch RH limit to use, based on jumper is on or not, can be changed on the fly
	if ( input (RH_LIMIT_2_JUMPER ) == LOW )
	{
		SELECTED_RH_LIMIT = MAXIMUM_RH_2;
	}
	else
	{
		SELECTED_RH_LIMIT = MAXIMUM_RH_1;
	}

	if( rh > SELECTED_RH_LIMIT )
	{
		if ( input (INCLUDE_TEMP_JUMPER) == LOW && tcelsius < MINIMUM_TEMP )
		{
			//don't run if temp to low and check-temp switch is on
			return;
		}
		// RH is above upper threshold and temp check is either ok or deactivated, start fan if not already running
		if ( cFan1Flag == OFF )
		{
			cFan1Flag = ON;
			cFan1CanStop = NO;
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
				output_high ( FAN_1 );
			}
		}
	}
	cFan1AutoState = cFan1Flag; // for correct view in display
}



