#include "18F1320.h"
/* BUGS
- FIXED - sl�r av og p� rele, mens de er p�
- FIXED - bytt max_run_min til int16
- FIXED - ikke sensor_fault n�r �pen krets

TODO's
- Add auto-reset alarm etter feks 16 timer
- FIXED - Kalibrer frys POT, helt kaos i koden (og sjekk kj�l POT)
- Pr�v � f� et system i green/amber led h�ndtering
- FIXED - Flytt mer vars til structs
- FIXED - Revurder hvordan debug blir gjort
- FIXED - Add blinkrate on red led
- FIXED - Add supercool, kj�r kj�l til 0C (kun om innenfor max_run_min, evt juster denne under denne modusen)
- FIXED - Add deepfreeze, kj�r frys ned til -26 (kun om innenfor max_run_min, evt juster denne under denne modusen)
- FIXED - Add #IFDEF DEBUG p� alle debug statements
- FIXED - Add #IFDEF DEBUG p� aktuelle defines som har med timing
- FIXED - F� switches mer responsive ogs� med delay's i koden


*/
/****** LEDS *************
com=low and DL1=high: green
com=high and DL1/DL2=low: orange
com=low and DL3=high: red
- com usually low = green
***************************/

/****** SWITCHES *************
Alltid lav p� en side
For � teste SW2:
- Kj�r B2 h�y
***************************/


//#include <math.h>
#include "main.h"


#zero_ram
void main()
{
    
    
    prev_restart_reason = read_eeprom (MEM_ADDR_RESTART);
    write_eeprom (MEM_ADDR_RESTART, restart_cause()); 
    curr_restart_reason = read_eeprom (MEM_ADDR_RESTART);        
    
    //can be fex RESTART_WATCHDOG, RESTART_POWER_UP
          
    initialize_hardware();
    initialize_software();
      
    delay_ms(1000);
  
    
  #ifdef DEBUG printf(STREAM_DEBUG, "** Startup, curr/prev reason: %u,%u", curr_restart_reason, prev_restart_reason);  #endif
  
  
       
  while(TRUE)
  {
    restart_wdt();

    
    //delay_ms(2000);
    
    if(do_read_temp && !Status.sampling)
    {
        readTempFreezer();
        readTempFridge();                 
        readPotFridge();
        readPotFreezer();            
        do_read_temp = 0;
        updateAlarms();
    }        
    
                 
    doDecision_Freezer();
    doDecision_Fridge();
    
    if((SampleFridge.do_actions || SampleFreezer.do_actions) && poweredup_mins >= 1) 
        doActions();
    
    //checkAlarms();
    
    #ifdef DEBUG

    if(print_debug)
    {
	    print_debug = NO;        
	    
	    printf(STREAM_DEBUG, "deepfreeze (status/reached-temp/run-min): %u/%u/%u\r\n", Status.mode_fastfreeze, Status.fastfreeze_reached_temp, Status.fastfreeze_run_mins);     
	    printf(STREAM_DEBUG, "supercool: %u\r\n", Status.mode_supercool);     
	    
	    //printf(STREAM_DEBUG, "curr/prev boot reason: %u,%u\r\n", curr_restart_reason, prev_restart_reason); 
	    
	    printf(STREAM_DEBUG, "Fridge FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u\r\n", Temps.fridge_calc, Temps.fridge_real, Status.fridge_stopped_mins, Status.fridge_run_mins);
	    printf(STREAM_DEBUG, "Freezer FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u\r\n", Temps.freezer_calc, Temps.freezer_real, Status.freezer_stopped_mins, Status.freezer_run_mins);        
	    printf(STREAM_DEBUG, "Fridge/Freezer O/U %u/%d %u/%d\r\n\r\n", FRIDGE_DEGREES_OVER, config.fridge_degrees_under, FREEZER_DEGREES_OVER, config.freezer_degrees_under); 
	    
	    printf(STREAM_DEBUG, "POT Fridge %d/%Lu\r\n", Temps.pot_fridge, SampleFridgePot.avg_adc_value);    
	    printf(STREAM_DEBUG, "POT Freezer %d/%Lu\r\n\r\n", Temps.pot_freezer, SampleFreezerPot.avg_adc_value);
	    
	    printf(STREAM_DEBUG, "Fridge/Freezer Starts: %u/%u\r\n", started2, started1);         
	    printf(STREAM_DEBUG, "Fridge/Freezer running: %u/%u\r\n", Status.fridge_running, Status.freezer_running);
	
	    printf(STREAM_DEBUG, "Q5: %d\r\n", input(TRANSISTOR_Q5C));         
	
	    printf(STREAM_DEBUG, "Fridge/Freezer total run: %Lu/%Lu\r\n", Status.fridge_total_run_mins, Status.freezer_total_run_mins);
	    
	    printf(STREAM_DEBUG, "Alarm First: %s\r\n", Alarms.first_alarm);
	    printf(STREAM_DEBUG, "Alarm Last: %s\r\n", Alarms.last_alarm);
	    
	    printf(STREAM_DEBUG, "**END CYCLE**\r\n");
	} 
    #endif
    
    
                    
            
        
  }

}

#separate
void initialize_hardware(void)
{
    setup_wdt(WDT_ON); // settings/Postscaler set in fuses
    
    setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_BIT); // ISR 1ms
    setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
    //setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us, ISR 4ms
    setup_timer_2(T2_DIV_BY_4,255,1); // OF 1ms, ISR 1ms
    //setup_timer_2(T2_DIV_BY_1,9,10); // OF 10uS, ISR 100Us
    //setup_timer_2(T2_DIV_BY_4,100,8); 
    
    setup_timer_3(T3_INTERNAL|T3_DIV_BY_1); // ISR 65,5ms
    //setup_timer_3(T3_INTERNAL|T3_DIV_BY_8); // ISR 524ms
    // setup_timer_3(T3_INTERNAL|T3_DIV_BY_2); // ISR 131ms
    
    setup_adc_ports(sAN1|sAN4|sAN5|sAN6|VSS_VDD); // Inc the POTs   
    setup_adc(ADC_CLOCK_DIV_64|ADC_TAD_MUL_20);
    
    setup_ccp1(CCP_OFF);
    port_b_pullups(FALSE);   
    
    enable_interrupts(INT_TIMER0);
    enable_interrupts(INT_TIMER1);
    enable_interrupts(INT_TIMER2);
    enable_interrupts(INT_TIMER3);   
    enable_interrupts(GLOBAL);

    set_tris_a(TRISA_NORMAL); 
    set_tris_b(TRISB_NORMAL); 
    
}

#separate    
void initialize_software(void)
{    
    
    Stop_Fridge_Motor;
    Stop_Freezer_Motor;
    Temps.freezer_real = NOT_READY;
    Temps.fridge_real = NOT_READY;
    Temps.pot_freezer = NOT_READY;
    Temps.pot_fridge = NOT_READY;
    
    Alarms.reset_done = YES;
    
    LEDs.DL1_Color = LED_GREEN;
    LEDs.DL2_Color = LED_GREEN;
    do_LED_updates = YES;
    
    Status.fridge_stopped_mins = FRIDGE_MIN_STOP_MIN; // instant turn-on at power on
    Status.freezer_stopped_mins = FREEZER_MIN_STOP_MIN; // instant turn-on at power on
    config.freezer_degrees_under = FREEZER_DEGREES_UNDER;
    config.fridge_degrees_under = FRIDGE_DEGREES_UNDER;

    config.freezer_run_temp_offset = FREEZER_RUN_TEMP_OFFSET; 
    config.freezer_stop_temp_offset = FREEZER_STOP_TEMP_OFFSET; 
    
    config.fridge_run_temp_offset = FRIDGE_RUN_TEMP_OFFSET; 
    config.fridge_stop_temp_offset = FRIDGE_STOP_TEMP_OFFSET; 
    
    DisableSensors; 
    
}    

#separate
void setLEDs(void)
{

    do_LED_updates = NO;
    //delay_ms(1);
    
    /*********** FRIDGE LEDs ***********/        
    if(Status.fridge_running)
    {
        LEDs.DL2=LED_ON;
        LEDs.DL2_Color = LED_GREEN;
    }
    else
        LEDs.DL2=LED_OFF;
        
    if(Status.fridge_running && Status.mode_supercool)
    { 
        LEDs.DL2=LED_BLINK;     
        LEDs.DL2_Color = LED_AMBER;
        LEDs.DL1_Color = LED_AMBER; // if on is amber, both needs to be....HW bug
    }
    

    /*********** FREEZER LEDs ***********/        

    if(Status.freezer_running)
    {
        LEDs.DL1=LED_ON;
        LEDs.DL1_Color = LED_GREEN;
    }
    else
        LEDs.DL1=LED_OFF;
            
    if(Status.freezer_running && Status.mode_fastfreeze)
    { 
        LEDs.DL1=LED_BLINK;     
        LEDs.DL1_Color = LED_AMBER;
        LEDs.DL2_Color = LED_AMBER; // if on is amber, both needs to be....HW bug
    }

    
    if(Alarms.alarm_active)
    {
        //LEDs.DL1_Color = LED_GREEN; //needed for the RED LED to work...
        //LEDs.DL2_Color = LED_GREEN; //needed for the RED LED to work...
        
        //*********** ALARM LEDs ***********
        if(Alarms.freezer_run_to_long || Alarms.freezer_sensor_fault || Alarms.freezer_temp)
            LEDs.DL1=LED_BLINK_FAST;        
    
        if(Alarms.fridge_run_to_long || Alarms.fridge_sensor_fault || Alarms.fridge_temp)
            LEDs.DL2=LED_BLINK_FAST;
        
        LEDs.DL3 = LED_ON;
        
        /* 
        if(Alarms.freezer_temp || Alarms.fridge_temp)
            LEDs.DL3 = LED_ON;
        else if (Alarms.fridge_sensor_fault || Alarms.freezer_sensor_fault || Alarms.fridge_run_to_long || Alarms.freezer_run_to_long)
            LEDs.DL3 = LED_BLINK;        
        */     
    }    
    else 
    {
        if(Alarms.reset_done)
            LEDs.DL3 = LED_OFF;
        else
            LEDs.DL3 = LED_BLINK;            
    }        
    
    do_LED_updates = YES;
}
    
/**************** Logic/Decisions *************************/            

/**
* Logic for start/stop/keep running the FREEZER
*/
#separate
void doDecision_Freezer(void)
{
        
    if( Temps.freezer_real == 0 || Temps.freezer_real == NOT_READY || Temps.pot_freezer == NOT_READY) 
        return; // not fully initialized yet
   

    Status.run_freezer = YES; //default
		
    if(Status.freezer_running && (Temps.freezer_real > -33 && !Status.mode_fastfreeze)) // safety, real -35=-28 i skapet, etter kj�rt 240 min...
        Temps.freezer_calc = Temps.freezer_real + config.freezer_run_temp_offset;
    else if(!Status.freezer_running && Status.freezer_stopped_mins < 5)					// use "run" params for a few minutes after stop, while sensor "de-ices"
    	Temps.freezer_calc = Temps.freezer_real + config.freezer_run_temp_offset;
    else
        Temps.freezer_calc = Temps.freezer_real + config.freezer_stop_temp_offset;


    // it happened that it ran for 90 min, it was -25 but calc value showed -18...prevent this "hickup" from happening again
    // we just "add some cold", instead of stop running, in case of run after de-iceing or whatnot...this way it resume running normally
    if(Status.freezer_run_mins > 90 && Temps.freezer_calc > -20)
        Temps.freezer_calc -= 4;
                   
    if(Status.freezer_run_mins >= FREEZER_MAX_RUN_MIN) // run too long
    {        
        Status.run_freezer = NO;        
        Alarms.freezer_run_to_long = YES;
        Status.mode_fastfreeze = OFF;
        #ifdef DEBUG2 printf(STREAM_DEBUG, "freezer_run > FREEZER_MAX_RUN\r\n");  #endif
    }    
    else
        Alarms.freezer_run_to_long = NO;
        
    if(Alarms.freezer_sensor_fault)
    {
        Status.run_freezer = NO;        
    }    
    
    if(Status.run_freezer) // None of the above checks changed the default YES, let's do the temp checks 
    {
        if(Status.freezer_running)
        {            
            if(Temps.freezer_calc <= (Temps.pot_freezer - config.freezer_degrees_under)) // shall we stop?
            {
                Status.run_freezer = NO;                
            }
        }
        else
        {
            if(Temps.freezer_calc < (Temps.pot_freezer + FREEZER_DEGREES_OVER)) // shall we ignore start?
            {
                Status.run_freezer = NO;                
            }
        }        
    }
        
    if(Temps.freezer_to_warm && Temps.pot_freezer != POT_OFF) Status.run_freezer = YES;            
    if(Temps.freezer_to_cold) Status.run_freezer = NO;
        
    if(Status.freezer_running && !Status.run_freezer) 
        if(Status.freezer_run_mins < FREEZER_MIN_RUN_MIN)
            Status.run_freezer = YES; // abort stop if not run minimum time
        
    
    if(!Status.freezer_running && Status.run_freezer) // we are supposed to start
        if(Status.freezer_stopped_mins < FREEZER_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
            Status.run_freezer = NO; // abort start            
    
        
    /******************** Fastfreeze might override *******************************/

    if(Status.mode_fastfreeze) // ISR turn it off
    {
        // check if ISR can start the timeout counting...
        if(!Status.fastfreeze_reached_temp && Temps.freezer_calc <= M_FASTFREEZE_TEMP)
            Status.fastfreeze_reached_temp = YES;        
        
        Status.run_freezer = YES;            
    }    
    
    /****** final checks, nothing can override this! *************/
    if(Alarms.freezer_run_to_long) 
        Status.run_freezer = NO;

    if(Temps.pot_freezer == POT_OFF) // pot turned all off
        Status.run_freezer = NO;
}    

/**
* Logic for start/stop/keep running the FRIDGE
*/
#separate
void doDecision_Fridge(void)
{
    
    if( Temps.fridge_real == 0 || Temps.fridge_real == NOT_READY || Temps.pot_fridge == NOT_READY) 
        return; // not fully initialized yet

    Status.run_fridge = YES; // Default
    
    if(Status.fridge_running)
        Temps.fridge_calc = Temps.fridge_real + config.fridge_run_temp_offset;
    else if(!Status.fridge_running && Status.fridge_stopped_mins < 5)				// use "run" params for a few minutes after stop, while temp stabilizes
    	Temps.fridge_calc = Temps.fridge_real + config.fridge_run_temp_offset;
    else
        Temps.fridge_calc = Temps.fridge_real + config.fridge_stop_temp_offset;

    	
    	   
    if(Status.fridge_run_mins >= FRIDGE_MAX_RUN_MIN) // run too long
    {
        Status.run_fridge = NO;        
        Alarms.freezer_run_to_long = YES;
        Status.mode_supercool = OFF;
        #ifdef DEBUG2 printf(STREAM_DEBUG, "fridge_run > FRIDGE_MAX_RUN\r\n");  #endif
    }    
    
    if(Alarms.fridge_sensor_fault)
    {
        Status.run_fridge = NO;        
    }        

    if(Status.run_fridge) // None of the above checks changed the default YES, let's do the temp checks
    {
        
        if(Status.fridge_running) 
        {
            if(Temps.fridge_calc <= (Temps.pot_fridge - config.fridge_degrees_under)) // shall we stop?
            {
                Status.run_fridge = NO;
            }    
        }
        else
        {
            if(Temps.fridge_calc < (Temps.pot_fridge + FRIDGE_DEGREES_OVER)) // shall we ignore start?
            {
                Status.run_fridge = NO;                
            }
        }        
    }
    
    if(Temps.fridge_to_warm && Temps.pot_fridge != POT_OFF) Status.run_fridge = YES;        
    if(Temps.fridge_to_cold) Status.run_fridge = NO;

    if(Status.fridge_running && !Status.run_fridge) 
        if(Status.fridge_run_mins < FRIDGE_MIN_RUN_MIN) 
            Status.run_fridge = YES; // abort stop if not run minimum time
        
 
    if(!Status.fridge_running && Status.run_fridge) // we are supposed to start
        if(Status.fridge_stopped_mins < FRIDGE_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...            
            Status.run_fridge = NO; // abort start                        
        
    
        
    /********* SuperCool might override ******************/
    if(Status.run_fridge == NO && Status.mode_supercool)
    {
        if(Temps.fridge_calc <= M_SUPERCOOL_TEMP) // reached Supercool temp        
            Status.mode_supercool = OFF;
        else
            Status.run_fridge = YES;
    }
    
    /****** final checks, nothing can override this! *************/
    if(Alarms.fridge_run_to_long) 
        Status.run_fridge = NO;     

    if(Temps.pot_fridge == POT_OFF) // pot turned all off
        Status.run_fridge = NO;          
}


/**
* Perform any actions with the motors
*/
void doActions(void)
{       
    
    if(SampleFridge.do_actions)
    {   
        SampleFridge.do_actions = NO;
        if(Status.run_fridge && !Status.fridge_running) 
        {           
            Start_Fridge_Motor;            
        }    
        
        if(!Status.run_fridge && Status.fridge_running)
        {
            Stop_Fridge_Motor;
            Status.fridge_stopped_mins = 0;                    
        }    
    }   
    if(SampleFreezer.do_actions)
    {
        SampleFreezer.do_actions = NO;
        if(Status.run_freezer && !Status.freezer_running) 
        {           
            Start_Freezer_Motor;            
        }    
        
        if(!Status.run_freezer && Status.freezer_running) 
        {
            Stop_Freezer_Motor;
            Status.freezer_stopped_mins = 0;            
        }
    }    
}


void updateAlarms(void)
{    
    if(poweredup_mins < 2) return;
    if(Temps.fridge_real == NOT_READY || Temps.pot_fridge == NOT_READY || Temps.freezer_real == NOT_READY || Temps.pot_freezer == NOT_READY) return;
         
    // Update FRIDGE temp struct    
    //if(Temps.fridge_calc > (Temps.pot_fridge + FRIDGE_DEGREES_OVER)) Temps.fridge_to_warm = YES; else Temps.fridge_to_warm = NO;        
    if((Temps.fridge_real + (config.fridge_stop_temp_offset-2)) > (Temps.pot_fridge + FRIDGE_DEGREES_OVER)) Temps.fridge_to_warm = YES; else Temps.fridge_to_warm = NO;        
    if((Temps.fridge_real + config.fridge_run_temp_offset) < 0) Temps.fridge_to_cold = YES; else Temps.fridge_to_cold = NO;
    //if((Temps.fridge_real + config.fridge_run_temp_offset) < (Temps.pot_fridge - config.fridge_degrees_under)) 
    
    // Update FREEZER temp struct        
    if( (Temps.freezer_real + config.freezer_stop_temp_offset) > FREEZER_MAX_TEMP) Temps.freezer_to_warm = YES; else Temps.freezer_to_warm = NO;
    //if(tmp < FREEZER_MIN_TEMP) Temps.freezer_to_cold = YES; else Temps.freezer_to_cold = NO;
    
    
    if(Temps.fridge_to_warm) 
    { 
        Alarms.fridge_temp = YES;
        
        sprintf(Alarms.tmp, "KJ WA, %d+%d-2>%d+%d = %d>%d", Temps.fridge_real, config.fridge_stop_temp_offset, Temps.pot_fridge, FRIDGE_DEGREES_OVER, Temps.fridge_real+(config.fridge_stop_temp_offset-2), Temps.pot_fridge+FRIDGE_DEGREES_OVER);
        if(strlen(Alarms.first_alarm) < 2) sprintf(Alarms.first_alarm, Alarms.tmp); else sprintf(Alarms.last_alarm, Alarms.tmp);
        
        #ifdef DEBUG_ALARMS printf(STREAM_DEBUG, "**A: Fridge to WARM\r\n");  #endif
    }
    else
    {
        Alarms.fridge_temp = NO;
    }
        
    if(Temps.fridge_to_cold)
    {
        Alarms.fridge_temp = YES;
        #ifdef DEBUG_ALARMS printf(STREAM_DEBUG, "**A: Fridge to COLD\r\n");  #endif        
    }        
    
    
    if(Temps.freezer_to_warm) 
    { 
        Alarms.freezer_temp = YES;
        sprintf(Alarms.tmp, "FR WA, %d+%d>%d = %d>%d", Temps.freezer_real, config.freezer_stop_temp_offset, FREEZER_MAX_TEMP, Temps.freezer_real+config.freezer_stop_temp_offset, FREEZER_MAX_TEMP);
        if(strlen(Alarms.first_alarm) < 2) sprintf(Alarms.first_alarm, Alarms.tmp); else sprintf(Alarms.last_alarm, Alarms.tmp);

        #ifdef DEBUG printf(STREAM_DEBUG, "**A: Freezer to WARM\r\n");  #endif
    }
    else
    {
        Alarms.freezer_temp = NO;
    }
        
    /*if(Temps.freezer_to_cold)
    {
        Alarms.freezer_temp = YES;
        #ifdef DEBUG_ALARMS printf(STREAM_DEBUG, "**A: Freezer to COLD\r\n");  #endif
    }    
    */



    if(Alarms.fridge_run_to_long)
        if(Status.fridge_stopped_mins >= WAIT_MIN_AFTER_LONG_RUN)
            Alarms.fridge_run_to_long = NO;

    if(Alarms.freezer_run_to_long)
        if(Status.freezer_stopped_mins >= WAIT_MIN_AFTER_LONG_RUN)
            Alarms.freezer_run_to_long = NO;

    
    if(SampleFridge.avg_adc_value < 400 || SampleFridge.avg_adc_value > 800)    
         Alarms.fridge_sensor_fault = YES; 
    else
        Alarms.fridge_sensor_fault = NO;


    if(SampleFreezer.avg_adc_value < 400 || SampleFreezer.avg_adc_value > 800) 
        Alarms.freezer_sensor_fault = YES; 
    else 
        Alarms.freezer_sensor_fault = NO;

     if(Alarms.fridge_run_to_long || Alarms.freezer_run_to_long || Alarms.fridge_sensor_fault || Alarms.freezer_sensor_fault || Alarms.fridge_temp || Alarms.freezer_temp)
     {
        Alarms.alarm_active = YES;
        Alarms.reset_done = NO;
     }   
     else
        Alarms.alarm_active = NO;
    // reset must be done by user pushing a button    
    
    #ifdef DEBUG_ALARMS
    if(Alarms.fridge_sensor_fault) { printf(STREAM_DEBUG, "A: Fridge SENSOR_FAULT\r\n");  }
    if(Alarms.freezer_sensor_fault) { printf(STREAM_DEBUG, "A: Freezer SENSOR_FAULT\r\n");  }
    #endif        
        
    
}

    


#separate            
void STREAM_DEBUG(unsigned int8 c)
{
    disable_interrupts(GLOBAL);
    
    putc(c);
    //printf("C:%c,U:%u,H:%X", c, c, c);
    enable_interrupts(GLOBAL);

}

/*
void debug(void)
{
  //using SW UART we can't interrupt while writing...
  disable_interrupts(GLOBAL);
  printf("%s\r\n", STREAM_DEBUG);
  delay_ms(5);
  //strcpy(STREAM_DEBUG, '\0');
  enable_interrupts(GLOBAL);
    
} 
*/   